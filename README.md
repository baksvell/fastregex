# FastRegex 

[![Build Status](https://github.com/baksvell/Fastregex/actions/workflows/build.yml/badge.svg)](https://github.com/baksvell/Fastregex/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## English Version

### 🔥 About Project

FastRegex is a high-performance regular expression library that combines:
- **LLVM JIT compilation** for complex patterns
- **SIMD optimizations** (AVX2/AVX512/SSE4.2/NEON) for literal matching
- **Smart caching system** to avoid recompilation
- **Python bindings** via pybind11

### 🚀 Performance Benchmarks

| Test Case          | Python re (ms) | FastRegex (ms) | Speedup |
|--------------------|---------------|----------------|---------|
| Email validation   | 2.250 ±0.157  | 0.021 ±0.002   | 107x ✅ |
| Word boundaries    | 0.166 ±0.011  | 0.025 ±0.002   | 6.6x ✅ |
| Complex pattern    | 19.665 ±4.100 | 0.017 ±0.003   | 1156x ✅ |
| Multiline text     | 0.855 ±0.163  | 0.219 ±0.002   | 3.9x ✅ |
| Simple literal     | 0.002 ±0.001  | 0.004 ±0.002   | 0.5x ⚠️ |
| Digits             | 0.002 ±0.001  | 0.003 ±0.002   | 0.7x ⚠️ |
| Hashtags           | 0.003 ±0.001  | 0.026 ±0.002   | 0.1x ⚠️ |
| URLs               | 0.010 ±0.002  | 0.021 ±0.002   | 0.5x ⚠️ |

**Key insights**:
- До 1156x быстрее для сложных паттернов
- 3-100x ускорение для типовых сценариев
- Литеральные паттерны требуют оптимизации (в разработке)

### 🛠 Building from Source

#### Prerequisites
- CMake 3.20+
- Python 3.10+
- C++17 compiler (GCC/MSVC/Clang)

```bash
git clone https://github.com/baksvell/Fastregex.git
cd Fastregex
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```
## Русская версия


### 🔥 О проекте


FastRegex - это высокопроизводительная библиотека регулярных выражений с:

- **JIT-компиляцией через LLVM**

- **SIMD-ускорением (AVX2/AVX512/SSE4.2/NEON)**

-**Кешированием скомпилированных шаблонов**

### 📊 Результаты тестирования
Тест	Python re (мс)	FastRegex (мс)	Ускорение
Проверка email	2.250 ±0.157	0.021 ±0.002	107x ✅
Границы слов	0.166 ±0.011	0.025 ±0.002	6.6x ✅
Сложные паттерны	19.665 ±4.100	0.017 ±0.003	1156x ✅
Многострочный текст	0.855 ±0.163	0.219 ±0.002	3.9x ✅
Основные выводы:

Лучшие результаты для сложных выражений

Работа над оптимизацией простых паттернов

🛠 Сборка
bash
git clone https://github.com/baksvell/Fastregex.git
cd Fastregex
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

License / Лицензия
MIT License - see LICENSE
Лицензия MIT - см. LICENSE
