# Dockerfile для интерактивной работы с FastRegex
FROM python:3.12-slim

# Установка системных зависимостей
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Установка Python зависимостей
RUN pip install --no-cache-dir --upgrade pip
RUN pip install --no-cache-dir pybind11[global] cmake setuptools wheel

# Создание рабочей директории
WORKDIR /app

# Копирование исходного кода
COPY src/ ./src/
COPY CMakeLists.txt ./
COPY LICENSE ./

# Создание простого README.md
RUN echo "# FastRegex\nHigh-performance regex with JIT/SIMD optimizations" > README.md

# Сборка через CMake напрямую
RUN mkdir build
RUN cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DPYTHON_EXECUTABLE=/usr/local/bin/python3.12
RUN cd build && make -j$(nproc)

# Копирование собранного модуля в системный Python
RUN cp build/*.so /usr/local/lib/python3.12/site-packages/ 2>/dev/null || echo "No .so files to copy"

# Проверка установки
RUN python -c "import fastregex; print('FastRegex успешно установлен!')"

# Команда по умолчанию - держит контейнер запущенным
CMD ["tail", "-f", "/dev/null"]
