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
- **High Performance**: 1.5-5x faster than standard `re` module for specific use cases

## 📊 Performance Benchmarks

| Test Case          | Python re (ms) | FastRegex (ms) | Speedup |
|--------------------|---------------|----------------|---------|
| Short literals     | 0.0040        | 0.0023         | 1.7x ✅ |
| Simple patterns    | 0.0041        | 0.0025         | 1.6x ✅ |
| Find all matches   | 0.0127        | 0.0095         | 1.3x ✅ |
| Match operations   | 0.0040        | 0.0023         | 1.7x ✅ |

**Key insights**:
- 1.5-1.9x faster for most use cases
- Best performance on short literals and simple patterns
- Fully compatible with standard `re` module behavior
- Optimized for patterns < 50 characters

## 🛠 Installation

### From PyPI (Recommended)
### Using Docker (Recommended)
```bash
# Clone the repository
git clone https://github.com/baksvell/fastregex.git
cd fastregex

# Run with Docker
docker-compose up -d fastregex

# Enter the container
docker exec -it fastregex-dev bash

# Use FastRegex
python -c "import fastregex; print('FastRegex ready!')"
```

### From PyPI
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

# Pattern information
compiled = fastregex.compile(r'\d+')
print(f"Pattern: {compiled.pattern()}")
print(f"JIT compiled: {compiled.jit_compiled}")
```

## 🎯 When to Use FastRegex

### ✅ **Use FastRegex when:**
- Short literal patterns (1.7x faster)
- Simple regex patterns (1.6x faster)
- Match operations (1.7x faster)
- Find all operations (1.3x faster)
- Patterns < 50 characters

### ⚠️ **Use standard `re` when:**
- Very large texts (>10MB)
- Complex regex patterns with many groups
- Need advanced regex features
- Long patterns (>50 characters)

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

### Pattern Information
- `compiled.pattern()` - Get the compiled pattern
- `compiled.jit_compiled` - Check if pattern is JIT compiled
- `compiled.compile_time()` - Get compilation time

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