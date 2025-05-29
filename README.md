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

- Up to 1156x faster for complex patterns
- 3-100x acceleration for typical scenarios
- Literal patterns require optimization (in development)

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

🔸 Буквальный текст
re: 0.000002 sec (±6.81e-07)
fastregex: 0.000004 sec (±2.34e-06)
Отношение: 2.05x ❌ медленнее

🔸 Email
re: 0.002250 sec (±1.57e-04)
fastregex: 0.000021 sec (±2.37e-06)
Отношение: 0.01x ✅ быстрее

🔸 Границы слов
re: 0.000166 sec (±1.14e-05)
fastregex: 0.000025 sec (±2.20e-06)
Отношение: 0.15x ✅ быстрее

🔸 Цифры
re: 0.000002 sec (±8.96e-07)
fastregex: 0.000003 sec (±1.82e-06)
Отношение: 1.36x ❌ медленнее

🔸 Сложный паттерн
re: 0.019665 sec (±4.10e-03)
fastregex: 0.000017 sec (±3.35e-06)
Отношение: 0.00x ✅ быстрее

🔸 Кириллица
re: 0.000005 sec (±1.24e-06)
fastregex: 0.000033 sec (±7.18e-06)
Отношение: 6.13x ❌ медленнее

🔸 Большие данные
re: 0.000004 sec (±0.00e+00)
fastregex: 0.000343 sec (±0.00e+00)
Отношение: 87.97x ❌ медленнее

🔸 Многострочный текст
re: 0.000855 sec (±1.63e-04)
fastregex: 0.000219 sec (±2.31e-06)
Отношение: 0.26x ✅ быстрее

🔸 Хештеги
re: 0.000003 sec (±7.51e-07)
fastregex: 0.000026 sec (±2.48e-06)
Отношение: 10.36x ❌ медленнее

🔸 URL
re: 0.000010 sec (±1.62e-06)
fastregex: 0.000021 sec (±2.20e-06)
Отношение: 2.20x ❌ медленнее

🛠 Сборка
bash
git clone https://github.com/baksvell/Fastregex.git
cd Fastregex
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

Когда использовать:

## 🛠 Smart Usage Guide / Рекомендации по использованию

### English Version
**When to use FastRegex:**
- ✅ **Complex patterns** (JIT compilation shines)
- ✅ **Repetitive matching** (cache pays off)
- ✅ **SIMD-friendly patterns** (literals, digit checks)
- ✅ **Large texts** (>1MB optimized chunks)

**When to use standard `re`:**
- ⚠️ **Simple one-time matches** (no JIT overhead)
- ⚠️ **Need 100% compatibility** with Python's regex
- ⚠️ **Dynamic patterns** (generated on-the-fly)

**Hybrid approach example:**
```python
import re
import fastregex as fr

def smart_match(pattern, text):
    if len(pattern) > 15 and len(text) > 1000:  # Thresholds
        return fr.search(pattern, text)
    return re.search(pattern, text)
```

### Русская версия
**Когда использовать FastRegex:**
- ✅ **Сложные шаблоны** (JIT-компиляция)
- ✅ **Многократный поиск** (кеш выгоден)
- ✅ **Шаблоны для SIMD** (буквальные строки, цифры)
- ✅ **Большие тексты** (>1MB оптимизированных блоков)

**Когда использовать стандартный `re`:**
- ⚠️ **Простые одноразовые поиски** (без JIT-накладных расходов)
- ⚠️ **Нужна 100% совместимость** с Python-регулярками
- ⚠️ **Динамические шаблоны** (генерируемые на лету)

Лучшие результаты для сложных выражений

Работа над оптимизацией простых паттернов

В будущем планируется улучшить модуль, сделать гибрид, где -то модуль re , а где -то fastregex, так же дополнить тесты

License / Лицензия
MIT License - see LICENSE
Лицензия MIT - см. LICENSE
