# FastRegex

[![Build Status](https://github.com/baksvell/fastregex/actions/workflows/build.yml/badge.svg)](https://github.com/baksvell/fastregex/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Python Version](https://img.shields.io/badge/python-3.10%2B-blue.svg)](https://www.python.org/downloads/)
[![PyPI version](https://badge.fury.io/py/fastregex.svg)](https://badge.fury.io/py/fastregex)

A high-performance regular expression library for Python with JIT compilation and SIMD optimizations.

## 🚀 Features

- **JIT Compilation**: LLVM-based just-in-time compilation for complex patterns
- **SIMD Optimizations**: AVX2/AVX512/SSE4.2/NEON support for vectorized operations
- **Smart Caching**: Automatic caching of compiled patterns to avoid recompilation
- **Python Integration**: Seamless integration via pybind11
- **High Performance**: Up to 1000x faster than standard `re` module for complex patterns

## 📊 Performance Benchmarks

| Test Case          | Python re (ms) | FastRegex (ms) | Speedup |
|--------------------|---------------|----------------|---------|
| Email validation   | 2.250 ±0.157  | 0.021 ±0.002   | 107x ✅ |
| Word boundaries    | 0.166 ±0.011  | 0.025 ±0.002   | 6.6x ✅ |
| Complex pattern    | 19.665 ±4.100 | 0.017 ±0.003   | 1156x ✅ |
| Multiline text     | 0.855 ±0.163  | 0.219 ±0.002   | 3.9x ✅ |

**Key insights**:
- Up to 1156x faster for complex patterns
- 3-100x acceleration for typical scenarios
- Best performance on repetitive operations

## 🛠 Installation

### From PyPI (Recommended)
```bash
pip install fastregex
```

### From Source
```bash
git clone https://github.com/baksvell/fastregex.git
cd fastregex
pip install -e .
```

### Prerequisites
- CMake 3.20+
- Python 3.10+
- C++17 compiler (GCC/MSVC/Clang)

## 📖 Usage

### Basic Usage

```python
import fastregex

# Simple search
result = fastregex.search(r'\d+', 'abc123def')
print(result)  # True

# Find all matches
matches = fastregex.find_all(r'\w+', 'hello world test')
print(matches)  # ['hello', 'world', 'test']

# Replace
new_text = fastregex.replace(r'\d+', 'abc123def456', 'XXX')
print(new_text)  # 'abcXXXdefXXX'

# Compile for reuse
compiled = fastregex.compile(r'\d+')
result = compiled.search('abc123def')
print(result)  # True
```

### Advanced Features

```python
# Check cache statistics
print(f"Cache size: {fastregex.cache_size()}")
print(f"Hit rate: {fastregex.hit_rate():.2%}")

# SIMD capabilities
caps = fastregex.simd_capabilities()
print(f"AVX2 support: {caps['avx2']}")
print(f"AVX512 support: {caps['avx512']}")

# SIMD statistics
stats = fastregex.get_simd_stats()
print(f"Total calls: {stats['total_calls']}")
```

## 🎯 When to Use FastRegex

### ✅ **Use FastRegex when:**
- Complex patterns (JIT compilation shines)
- Repetitive matching (cache pays off)
- SIMD-friendly patterns (literals, digit checks)
- Large texts (>1MB optimized chunks)

### ⚠️ **Use standard `re` when:**
- Simple one-time matches (no JIT overhead)
- Need 100% compatibility with Python's regex
- Dynamic patterns (generated on-the-fly)

### 🔄 **Hybrid approach:**
```python
import re
import fastregex as fr

def smart_match(pattern, text):
    if len(pattern) > 15 and len(text) > 1000:
        return fr.search(pattern, text)
    return re.search(pattern, text)
```

## 🧪 Testing

Run the test suite:
```bash
python -m pytest tests/
```

Run performance benchmarks:
```bash
python tests/benchmark.py
```

## 📚 API Reference

### Core Functions
- `fastregex.match(pattern, text)` - Match from start of string
- `fastregex.search(pattern, text)` - Search anywhere in string
- `fastregex.find_all(pattern, text)` - Find all matches
- `fastregex.replace(pattern, text, replacement)` - Replace matches
- `fastregex.compile(pattern)` - Compile pattern for reuse

### Cache Management
- `fastregex.cache_size()` - Get current cache size
- `fastregex.hit_rate()` - Get cache hit rate
- `fastregex.clear_cache()` - Clear the cache

### SIMD Features
- `fastregex.simd_capabilities()` - Get SIMD support info
- `fastregex.get_simd_stats()` - Get SIMD usage statistics
- `fastregex.set_simd_mode(mode)` - Set SIMD mode
- `fastregex.get_simd_mode()` - Get current SIMD mode

## 🤝 Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for details.

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests
5. Submit a pull request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🔗 Links

- [GitHub Repository](https://github.com/baksvell/fastregex)
- [Documentation](https://github.com/baksvell/fastregex#readme)
- [Issue Tracker](https://github.com/baksvell/fastregex/issues)

## 🙏 Acknowledgments

- [pybind11](https://github.com/pybind/pybind11) for Python bindings
- [LLVM](https://llvm.org/) for JIT compilation
- [SIMD](https://en.wikipedia.org/wiki/SIMD) for vectorized operations