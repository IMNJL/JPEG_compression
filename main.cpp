#include <cstddef>
#include <cstdio>

#ifdef __APPLE__
#include </opt/homebrew/include/jpeglib.h>
#else
#include <jpeglib.h>
#endif

#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>

using namespace std;

const string filename = "/Users/pro/CLionProjects/JPEG_compression/input_pics/11.jpg";

struct RGB { uint8_t r, g, b; };
struct YCbCr { float y, cb, cr; };

vector<RGB> read_jpeg(const char* filename, int& w, int& h) {
    FILE* file = fopen(filename, "rb");
    if (!file) throw runtime_error("Can't open file");

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, file);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    w = cinfo.output_width;
    h = cinfo.output_height;
    vector<RGB> pixels(w * h);

    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW row = (JSAMPROW)(&pixels[cinfo.output_scanline * w]);
        jpeg_read_scanlines(&cinfo, &row, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(file);
    return pixels;
}

// Запись JPEG файла
void write_jpeg(const char* filename, vector<RGB>& pixels, int w, int h, int quality) {
    FILE* file = fopen(filename, "wb");
    if (!file) throw runtime_error("Can't create file");

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, file);

    cinfo.image_width = w;
    cinfo.image_height = h;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row = (JSAMPROW)&pixels[cinfo.next_scanline * w];
        jpeg_write_scanlines(&cinfo, &row, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(file);
}

// RGB -> YCbCr
vector<YCbCr> convert_to_ycbcr(vector<RGB>& rgb) {
    vector<YCbCr> ycc(rgb.size());
    for (size_t i = 0; i < rgb.size(); ++i) {
        ycc[i].y  =  0.299f * rgb[i].r + 0.587f * rgb[i].g + 0.114f * rgb[i].b;
        ycc[i].cb = -0.1687f * rgb[i].r - 0.3313f * rgb[i].g + 0.5f * rgb[i].b + 128;
        ycc[i].cr =  0.5f * rgb[i].r - 0.4187f * rgb[i].g - 0.0813f * rgb[i].b + 128;
    }
    return ycc;
}

// YCbCr -> RGB
vector<RGB> convert_to_rgb(vector<YCbCr>& ycc) {
    vector<RGB> rgb(ycc.size());
    for (size_t i = 0; i < ycc.size(); ++i) {
        float r = ycc[i].y + 1.402f * (ycc[i].cr - 128);
        float g = ycc[i].y - 0.34414f * (ycc[i].cb - 128) - 0.71414f * (ycc[i].cr - 128);
        float b = ycc[i].y + 1.772f * (ycc[i].cb - 128);

        rgb[i].r = max(0, min(255, static_cast<int>(r)));
        rgb[i].g = max(0, min(255, static_cast<int>(g)));
        rgb[i].b = max(0, min(255, static_cast<int>(b)));
    }
    return rgb;
}

// Субдискретизация 4:2:0
void subsample_420(vector<YCbCr>& ycc, int w, int h, vector<float>& cb_sub, vector<float>& cr_sub) {
    int sub_w = (w + 1)/2, sub_h = (h + 1)/2;
    cb_sub.resize(sub_w * sub_h);
    cr_sub.resize(sub_w * sub_h);

    for (int y = 0; y < h; y += 2)
        for (int x = 0; x < w; x += 2) {
            float cb = 0, cr = 0;
            int count = 0;

            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx)
                    if (x+dx < w && y+dy < h) {
                        cb += ycc[(y+dy)*w + x+dx].cb;
                        cr += ycc[(y+dy)*w + x+dx].cr;
                        ++count;
                    }

            cb_sub[(y/2)*sub_w + x/2] = cb / count;
            cr_sub[(y/2)*sub_w + x/2] = cr / count;
        }
}

// Восстановление цветности
void upsample_420(vector<YCbCr>& ycc, int w, int h, vector<float>& cb_sub, vector<float>& cr_sub) {
    int sub_w = (w + 1)/2, sub_h = (h + 1)/2;

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int sx = x/2, sy = y/2;
            ycc[y*w + x].cb = cb_sub[sy*sub_w + sx];
            ycc[y*w + x].cr = cr_sub[sy*sub_w + sx];
        }
}

// Расчет метрик качества
pair<double, double> calculate_metrics(vector<RGB>& orig, vector<RGB>& compressed) {
    double mse = 0;
    for (size_t i = 0; i < orig.size(); ++i) {
        mse += pow(orig[i].r - compressed[i].r, 2);
        mse += pow(orig[i].g - compressed[i].g, 2);
        mse += pow(orig[i].b - compressed[i].b, 2);
    }
    mse /= (3 * orig.size());
    double psnr = 20 * log10(255.0) - 10 * log10(mse);
    return make_pair(mse, psnr);
}

int main() {
    try {
        // Чтение исходного изображения
        int w, h;
        vector<RGB> original = read_jpeg(filename.c_str(), w, h);
        auto start = chrono::high_resolution_clock::now();

        // Конвертация в YCbCrц
        auto t1 = chrono::high_resolution_clock::now();
        auto ycc = convert_to_ycbcr(original);
        auto t2 = chrono::high_resolution_clock::now();
        cout << "YCbCr conversion: " << chrono::duration_cast<chrono::microseconds>(t2-t1).count() << " μs\n";

        // Субдискретизация
        vector<float> cb_sub, cr_sub;
        subsample_420(ycc, w, h, cb_sub, cr_sub);

        // Восстановление
        upsample_420(ycc, w, h, cb_sub, cr_sub);

        // Обратная конвертация
        auto t3 = chrono::high_resolution_clock::now();
        vector<RGB> compressed = convert_to_rgb(ycc);
        auto t4 = chrono::high_resolution_clock::now();
        cout << "RGB conversion: " << chrono::duration_cast<chrono::microseconds>(t4-t3).count() << " μs\n";

        // Сохранение результата
        write_jpeg("output.jpeg", compressed, w, h, 90);
        auto end = chrono::high_resolution_clock::now();
        cout << "Time: " << chrono::duration_cast<chrono::milliseconds>(end-start).count() << " ms\n";

        // Расчет метрик
        auto [mse, psnr] = calculate_metrics(original, compressed);
        cout << "MSE: " << mse << "\nPSNR: " << psnr << " dB" << endl;

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    return 0;
}