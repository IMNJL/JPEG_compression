cmake_minimum_required(VERSION 3.20)
project(JPEG_compression LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Явно указываем пути к Homebrew
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/jpeg")

# Находим JPEG
find_package(JPEG REQUIRED)

# Выводим информацию для отладки
message(STATUS "JPEG include dirs: ${JPEG_INCLUDE_DIRS}")
message(STATUS "JPEG libraries: ${JPEG_LIBRARIES}")

# Создаем исполняемый файл
add_executable(jpeg_compressor main.cpp)

# Подключаем заголовочные файлы
target_include_directories(jpeg_compressor PRIVATE
        ${JPEG_INCLUDE_DIRS}
        /opt/homebrew/include
        /opt/homebrew/Cellar/jpeg/9f/include
)

# Подключаем библиотеки
target_link_libraries(jpeg_compressor PRIVATE
        ${JPEG_LIBRARIES}
        /opt/homebrew/lib/libjpeg.dylib
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(jpeg_compressor PRIVATE
            -O2             # Максимальная оптимизация скорости
            -march=native   # Использовать специфичные для CPU инструкции (AVX, SSE и т. д.)
            -flto           # Link-Time Optimization
            -fno-exceptions # Убрать обработку исключений (если не используются)
    )
    target_link_options(jpeg_compressor PRIVATE -flto)
endif()