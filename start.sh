#!/bin/bash

rm -rf benchmark_results

# Создаем папку для результатов
mkdir -p benchmark_results
cd benchmark_results

# Функция для компиляции и тестирования
run_benchmark() {
    local opt_flag=$1
    local name=$2
    local folder="build_${name}"

    echo "Running benchmark for ${name} (${opt_flag})..."

    mkdir -p "${folder}"
    cd "${folder}"

    # Компилируем с текущими флагами
    clang++ -v -std=c++17 -I/opt/homebrew/include -L/opt/homebrew/lib -ljpeg ${opt_flag} ../../main.cpp -o jpeg_compressor

    # Запускаем и сохраняем вывод
    ./jpeg_compressor > output.txt 2>&1

    # Извлекаем только время выполнения (строку с "Time:")
    time_ms=$(grep "^Time:" output.txt | awk '{print $2}')
    echo "${time_ms}" > time_ms.txt

    # Сохраняем информацию о бинарнике
    ls -lh jpeg_compressor > binary_info.txt
    size=$(stat -f%z jpeg_compressor)
    echo "${size}" > size_bytes.txt

    cd ..
}

# Запускаем тесты для разных флагов
run_benchmark "-O0" "O0"
run_benchmark "-O2" "O2"
run_benchmark "-O3" "O3"
run_benchmark "-Os" "Os"
run_benchmark "-Ofast" "Ofast"

# Создаем сводную таблицу результатов
echo "Флаг | Время (мс) | Размер бинарника | Примечания" > results.md
echo "-----|------------|------------------|-----------" >> results.md

for folder in build_*; do
    time_ms=$(cat "${folder}/time_ms.txt")
    size_bytes=$(cat "${folder}/size_bytes.txt")
    size_kb=$((size_bytes/1024))

    notes=""
    case "${folder}" in
        *O0*) notes="Нет оптимизации" ;;
        *O2*) notes="Баланс скорости/размера" ;;
        *O3*) notes="Агрессивная оптимизация" ;;
        *Os*) notes="Оптимизация по размеру" ;;
        *Ofast*) notes="Нарушает стандарты IEEE" ;;
    esac

    flag="${folder#build_}"
    echo "${flag} | ${time_ms} | ${size_kb}K | ${notes}" >> results.md
done

echo "Все тесты завершены. Результаты сохранены в benchmark_results/results.md"
cat results.md