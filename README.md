# FastRegex / БыстрыйРегулярки

[![Build Status](https://github.com/baksvell/Fastregex/actions/workflows/build.yml/badge.svg)](https://github.com/baksvell/Fastregex/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## English Version

### 🔥 About Project

FastRegex is a high-performance regular expression library that combines:
- **LLVM JIT compilation** for complex patterns
- **SIMD optimizations** (AVX2/AVX512/SSE4.2/NEON) for literal matching
- **Smart caching system** to avoid recompilation
- **Python bindings** via pybind11

Benchmarks show **5x-100x speedup** over Python's built-in `re` module.

### 🛠 Building from Source

#### Prerequisites
- CMake 3.20+
- Python 3.10
- C++17 compatible compiler:
  - Windows: MinGW-w64 or MSVC
  - Linux: GCC (≥9) or Clang
  - macOS: Xcode Command Line Tools
- Optional: Ninja for faster builds

#### Basic Build
```bash
git clone https://github.com/baksvell/Fastregex.git
cd Fastregex
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```
Windows Specific
bash
# For MinGW-w64 (adjust paths as needed)
```
cmake -G "Ninja" -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release ..
ninja
```
# For MSVC
```
cmake -A x64 -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```
Linux/macOS
bash
# With Ninja (recommended)
```
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
ninja
```
# With Make
```
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

💻 Installation
After building:

bash
# Install globally
cmake --install .

# Or develop mode
pip install -e .
📚 Basic Usage
python
import fastregex as fr

# Simple matching
if fr.search(r"\d+", "test123"):
    print("Match found!")

# Compiled pattern with flags
pattern = fr.compile(r"\w+@\w+\.\w+", fr.IGNORECASE)
matches = pattern.find_all("Emails: test@example.com, user@domain.com")
Русская версия
🔥 О проекте
FastRegex - это высокопроизводительная библиотека регулярных выражений с:

JIT-компиляцией через LLVM

SIMD-ускорением (AVX2/AVX512/SSE4.2/NEON)

Кешированием скомпилированных шаблонов

Python-интерфейсом через pybind11

Тесты показывают ускорение в 5-100 раз по сравнению со стандартным модулем re.

🛠 Сборка из исходников
Требования
CMake 3.20+

Python 3.10

Компилятор с поддержкой C++17:

Windows: MinGW-w64 или MSVC

Linux: GCC (≥9) или Clang

macOS: Xcode Command Line Tools

Опционально: Ninja для ускоренной сборки

Базовая сборка
```bash
git clone https://github.com/baksvell/Fastregex.git
cd Fastregex
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```
Для Windows
bash
# Для MinGW-w64 (укажите свои пути)
```
cmake -G "Ninja" -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release ..
ninja
```

# Для MSVC
```
cmake -A x64 -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```
Linux/macOS
bash
# С Ninja (рекомендуется)
```
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
ninja
```
# С Make
```
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
💻 Установка
После сборки:

bash
# Глобальная установка
cmake --install .

# Или режим разработки
pip install -e .
📚 Пример использования
python
import fastregex as fr

# Простой поиск
if fr.search(r"\d+", "test123"):
    print("Совпадение найдено!")

# Скомпилированный шаблон с флагами
pattern = fr.compile(r"\w+@\w+\.\w+", fr.IGNORECASE)
matches = pattern.find_all("Почты: test@example.com, user@domain.com")
License / Лицензия
MIT License - see LICENSE
Лицензия MIT - см. LICENSE