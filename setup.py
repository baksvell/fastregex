from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import sys
import os
import subprocess


# Класс для интеграции CMake и setuptools
class CMakeBuildExt(build_ext):
    def run(self):
        # Создаем директорию сборки
        build_dir = os.path.abspath(os.path.dirname(self.build_temp))
        os.makedirs(build_dir, exist_ok=True)

        # Конфигурация CMake
        cmake_args = [
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={build_dir}",
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DPYTHON_EXECUTABLE={sys.executable}"
        ]

        # Платформо-специфичные флаги
        if sys.platform == "win32":
            cmake_args.extend([
                "-A", "x64",
                "-T", "host=x64",
                "/std:c++17"
            ])
        else:
            cmake_args.extend([
                "-DCMAKE_CXX_FLAGS=-O3 -mavx2 -std=c++17"
            ])

        # Выполняем CMake
        subprocess.check_call(["cmake", f"-B{build_dir}", f"-S{os.getcwd()}"] + cmake_args)
        subprocess.check_call(["cmake", "--build", build_dir, "--config", "Release"])


# Основная конфигурация
setup(
    name="fastregex",
    version="0.1.0",
    author="baksvell",
    author_email="patsi2@yandex.ru",
    description="High-performance regex with JIT/SIMD optimizations",
    long_description=open("README.md", encoding="utf-8").read(),
    long_description_content_type="text/markdown",
    url="https://github.com/baksvell/Fastregex",
    project_urls={
        "Bug Reports": "https://github.com/baksvell/Fastregex/issues",
        "Source": "https://github.com/baksvell/Fastregex",
        "Documentation": "https://github.com/baksvell/Fastregex#readme",
    },
    ext_modules=[Extension("fastregex", sources=[])],  # Пустые источники - сборка через CMake
    cmdclass={"build_ext": CMakeBuildExt},
    packages=[],  # Не используем packages, так как у нас нативное расширение
    python_requires=">=3.10",
    install_requires=[
        "pybind11>=2.10",
    ],
    extras_require={
        "dev": [
            "pytest>=6.0",
            "pytest-benchmark>=3.0",
            "numpy>=1.20",
        ],
        "benchmark": [
            "numpy>=1.20",
            "matplotlib>=3.0",
        ],
    },
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: C++",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Topic :: Text Processing :: General",
        "Topic :: Software Development :: Libraries :: Python Modules",
        "Operating System :: OS Independent",
    ],
    keywords="regex regular expressions performance optimization JIT SIMD",
    zip_safe=False,  # Важно для нативных расширений
)