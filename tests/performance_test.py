import sys
import os
from pathlib import Path
import timeit
import re
import random
import string
import numpy as np
from matplotlib import pyplot as plt
from datetime import datetime
import importlib.util
import ctypes
import platform
import subprocess
import shutil
import statistics
import threading
from concurrent.futures import ThreadPoolExecutor, as_completed, TimeoutError

class TimeoutException(Exception):
    pass

def run_with_timeout(func, timeout=5):
    """Выполняет функцию с таймаутом"""
    result = [None]
    exception = [None]

    def worker():
        try:
            result[0] = func()
        except Exception as e:
            exception[0] = e

    thread = threading.Thread(target=worker)
    thread.start()
    thread.join(timeout)

    if thread.is_alive():
        raise TimeoutException(f"Функция превысила таймаут {timeout} секунд")
    if exception[0] is not None:
        raise exception[0]
    return result[0]

def find_dll(dll_name):
    """Поиск DLL в стандартных путях"""
    search_paths = [
        *os.environ.get('PATH', '').split(os.pathsep),
        str(Path(sys.prefix) / "bin"),
        str(Path(sys.prefix) / "Library" / "bin"),
        "C:/msys64/mingw64/bin",
        "C:/Windows/System32"
    ]

    for path in search_paths:
        dll_path = Path(path) / dll_name
        if dll_path.exists():
            return str(dll_path)
    return None

def check_dependencies(pyd_path):
    """Проверка всех зависимостей модуля"""
    print("\n🔍 Проверка зависимостей модуля:")

    if shutil.which('dumpbin'):
        try:
            print("\nАнализ зависимостей с помощью dumpbin:")
            result = subprocess.run(
                ['dumpbin', '/dependents', str(pyd_path)],
                capture_output=True,
                text=True,
                check=True
            )
            print(result.stdout)
        except subprocess.CalledProcessError as e:
            print(f"Ошибка при анализе зависимостей: {e.stderr}")

    required_dlls = [
        f"python{sys.version_info.major}{sys.version_info.minor}.dll",
        "libstdc++-6.dll",
        "libgcc_s_seh-1.dll",
        "libwinpthread-1.dll",
        "zlib1.dll"
    ]

    missing_dlls = []
    for dll in required_dlls:
        dll_path = find_dll(dll)
        if dll_path:
            print(f"✅ Найдена: {dll} -> {dll_path}")
        else:
            print(f"❌ Не найдена: {dll}")
            missing_dlls.append(dll)

    return missing_dlls

def load_fastregex_module():
    """Улучшенная загрузка модуля с таймаутом"""
    print("\n" + "=" * 50)
    print("Начало загрузки модуля fastregex")
    print("=" * 50)

    build_dir = Path(__file__).parent.parent / "build"
    pyd_path = build_dir / "fastregex.pyd"

    print(f"\n🔍 Поиск модуля по пути: {pyd_path}")

    if not pyd_path.exists():
        print(f"\n❌ Файл {pyd_path} не найден!")
        return None

    print(f"✅ Файл модуля найден. Размер: {pyd_path.stat().st_size / 1024:.2f} KB")

    # Проверка архитектуры
    print("\n🖥️ Информация о системе:")
    print(f"ОС: {platform.system()} {platform.release()}")
    print(f"Архитектура: {'x64' if sys.maxsize > 2 ** 32 else 'x86'}")
    print(f"Версия Python: {sys.version}")

    # Проверка зависимостей
    missing_dlls = check_dependencies(pyd_path)
    if missing_dlls:
        print("\n❌ Отсутствуют DLL:", missing_dlls)
        return None

    # Загрузка через ctypes
    print("\n🔎 Проверка загрузки через ctypes:")
    try:
        ctypes.CDLL(str(pyd_path))
        print("✅ Модуль может быть загружен через ctypes")
    except Exception as e:
        print(f"❌ Ошибка загрузки: {e}")
        return None

    # Импорт модуля с таймаутом
    print("\n🚀 Попытка импорта модуля...")
    try:
        sys.path.append(str(build_dir))
        import fastregex
        print("✅ Модуль успешно загружен!")
        return fastregex
    except Exception as e:
        print(f"❌ Ошибка импорта: {e}")
        return None

class RegexBenchmark:
    def __init__(self, fastregex_module):
        self.fastregex = fastregex_module
        self.results = []
        self.cache_stats = []
        self.test_cases = [
            ('Буквальный текст', 'abc', 1000),
            ('Цифры', r'\d+', 1000),
            ('Email', r'[\w.-]+@[\w.-]+\.\w+', 1000),
            ('Границы слов', r'\b\w{4}\b', 1000),
            ('Сложный паттерн', r'([A-Za-z0-9]+[.-_])*[A-Za-z0-9]+@[A-Za-z0-9-]+(\.[A-Z|a-z]{2,})+', 500),
            ('Многострочный текст', r'^[A-Z].*$', 5000, lambda: '\n'.join(['Test'] + ['x' * 100] * 100)),
            ('Большие данные', r'\b\w{10}\b', 10000,
             lambda: ' '.join([''.join(random.choices(string.ascii_letters, k=10)) for _ in range(1000)])),
            ('Кириллица', r'[А-Яа-яЁё]+', 1000),
            ('Хештеги', r'#[A-Za-z0-9_]+', 1000),
            ('URL', r'https?://(?:[-\w.]|(?:%[\da-fA-F]{2}))+', 1000)
        ]

    def generate_text(self, length, pattern=None, custom_generator=None):
        """Генерация текста с паттернами"""
        if custom_generator:
            return custom_generator()

        chars = string.ascii_letters + string.digits + ' \t\n'
        text = ''.join(random.choices(chars, k=length))

        if pattern:
            clean_pattern = re.sub(r'[\\\[\](){}^$.*+?|]', '', pattern)
            if clean_pattern:
                for _ in range(10):
                    pos = random.randint(0, len(text) - len(clean_pattern))
                    text = text[:pos] + clean_pattern + text[pos + len(clean_pattern):]
        return text

    def run_test(self, name, pattern, text_size, text_gen=None):
        """Запуск одного теста с таймаутом"""
        print(f"\n🔹 Тест: {name}")
        print(f"Паттерн: {pattern}")
        print(f"Размер текста: {text_size} символов")

        try:
            text = run_with_timeout(
                lambda: self.generate_text(text_size, pattern, text_gen),
                timeout=3
            )
        except TimeoutException:
            print(f"⏱️ Таймаут генерации текста для {name}")
            self.results.append({
                'name': name,
                'status': 'timeout',
                'stage': 'text_generation'
            })
            return

        # Подготовка regex
        try:
            re_pattern = run_with_timeout(
                lambda: re.compile(pattern),
                timeout=1
            )
            fr_pattern = run_with_timeout(
                lambda: self.fastregex.compile(pattern),
                timeout=1
            )
        except TimeoutException:
            print(f"⏱️ Таймаут компиляции паттерна {pattern}")
            self.results.append({
                'name': name,
                'status': 'timeout',
                'stage': 'pattern_compilation'
            })
            return
        except Exception as e:
            print(f"⛔ Ошибка компиляции: {e}")
            self.results.append({
                'name': name,
                'status': 'error',
                'stage': 'pattern_compilation',
                'error': str(e)
            })
            return

        # Тестирование re
        try:
            re_time = run_with_timeout(
                lambda: min(timeit.repeat(
                    lambda: re_pattern.search(text),
                    number=10,
                    repeat=3
                )),
                timeout=10
            )
            re_stddev = statistics.stdev(timeit.repeat(
                lambda: re_pattern.search(text),
                number=10,
                repeat=3
            )) if text_size < 10000 else 0
        except TimeoutException:
            print(f"⏱️ Таймаут re для {name}")
            re_time = float('inf')
            re_stddev = 0
        except Exception as e:
            print(f"⛔ Ошибка re: {e}")
            re_time = float('inf')
            re_stddev = 0

        # Тестирование fastregex
        try:
            fr_time = run_with_timeout(
                lambda: min(timeit.repeat(
                    lambda: fr_pattern.search(text),
                    number=10,
                    repeat=3
                )),
                timeout=10
            )
            fr_stddev = statistics.stdev(timeit.repeat(
                lambda: fr_pattern.search(text),
                number=10,
                repeat=3
            )) if text_size < 10000 else 0
            ratio = fr_time / re_time if re_time != float('inf') else float('inf')
            result = "✅ быстрее" if ratio < 1 else "❌ медленнее"
        except TimeoutException:
            print(f"⏱️ Таймаут fastregex для {name}")
            fr_time = float('inf')
            fr_stddev = 0
            ratio = float('inf')
            result = "❌ ТАЙМАУТ"
        except Exception as e:
            print(f"⛔ Ошибка fastregex: {e}")
            fr_time = float('inf')
            fr_stddev = 0
            ratio = float('inf')
            result = "❌ ОШИБКА"

        self.results.append({
            'name': name,
            'pattern': pattern,
            'text_size': len(text),
            're_time': re_time,
            're_stddev': re_stddev,
            'fr_time': fr_time,
            'fr_stddev': fr_stddev,
            'ratio': ratio,
            'result': result,
            'status': 'completed'
        })

        # Сбор статистики кэша
        if hasattr(self.fastregex, 'cache_size'):
            self.cache_stats.append(self.fastregex.cache_size())

    def print_results(self):
        """Вывод результатов"""
        print("\n" + "=" * 50)
        print("Результаты тестирования")
        print("=" * 50)

        completed_tests = [r for r in self.results if r['status'] == 'completed']
        other_tests = [r for r in self.results if r['status'] != 'completed']

        for result in completed_tests:
            print(f"\n🔸 {result['name']}")
            print(f"re: {result['re_time']:.6f} sec (±{result['re_stddev']:.2e})")
            if result['fr_time'] == float('inf'):
                print("fastregex: ❌ ОШИБКА")
            else:
                print(f"fastregex: {result['fr_time']:.6f} sec (±{result['fr_stddev']:.2e})")
                print(f"Отношение: {result['ratio']:.2f}x {result['result']}")

        if other_tests:
            print("\n⚠️ Проблемные тесты:")
            for test in other_tests:
                print(f"{test['name']}: {test['status']} на этапе {test.get('stage', '?')}")

        if self.cache_stats:
            print("\n📊 Статистика кэша:")
            print(f"Максимальный размер: {max(self.cache_stats)}")
            print(f"Минимальный размер: {min(self.cache_stats)}")
            print(f"Средний размер: {statistics.mean(self.cache_stats):.1f}")

    def plot_results(self):
        """Визуализация результатов"""
        valid_results = [r for r in self.results if r['status'] == 'completed' and r['fr_time'] != float('inf')]
        if not valid_results:
            print("\n⚠️ Нет данных для графиков")
            return

        plt.figure(figsize=(14, 8))

        # График сравнения времени
        plt.subplot(2, 1, 1)
        labels = [r['name'] for r in valid_results]
        re_times = [r['re_time'] * 1000 for r in valid_results]
        fr_times = [r['fr_time'] * 1000 for r in valid_results]
        re_err = [r['re_stddev'] * 1000 for r in valid_results]
        fr_err = [r['fr_stddev'] * 1000 for r in valid_results]

        x = np.arange(len(labels))
        width = 0.35

        plt.bar(x - width / 2, re_times, width, label='re', color='blue', yerr=re_err, capsize=3)
        plt.bar(x + width / 2, fr_times, width, label='fastregex', color='orange', yerr=fr_err, capsize=3)

        plt.title('Сравнение производительности')
        plt.ylabel('Время (мс)')
        plt.xticks(x, labels, rotation=45, ha="right")
        plt.legend()
        plt.grid(True, axis='y', linestyle='--', alpha=0.7)

        # График относительной производительности
        plt.subplot(2, 1, 2)
        ratios = [r['ratio'] for r in valid_results]
        plt.bar(x, ratios, color=['green' if r < 1 else 'red' for r in ratios])
        plt.axhline(1, color='gray', linestyle='--')
        plt.title('Отношение времени выполнения (fastregex/re)')
        plt.ylabel('Отношение')
        plt.xticks(x, labels, rotation=45, ha="right")
        plt.grid(True, axis='y', linestyle='--', alpha=0.7)

        plt.tight_layout()

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"performance_results_{timestamp}.png"
        plt.savefig(filename)
        print(f"\n📊 График сохранён как {filename}")

def main():
    """Основная функция тестирования"""
    # Загрузка модуля
    fastregex_module = load_fastregex_module()
    if fastregex_module is None:
        print("\n❌ Не удалось загрузить модуль")
        sys.exit(1)

    print("\n" + "=" * 50)
    print("Тестирование производительности regex")
    print("=" * 50)

    benchmark = RegexBenchmark(fastregex_module)

    # Запуск тестов в пуле потоков
    with ThreadPoolExecutor(max_workers=4) as executor:
        futures = []
        for test_case in benchmark.test_cases:
            if len(test_case) == 3:
                name, pattern, size = test_case
                futures.append(executor.submit(
                    benchmark.run_test, name, pattern, size
                ))
            else:
                name, pattern, size, gen = test_case
                futures.append(executor.submit(
                    benchmark.run_test, name, pattern, size, gen
                ))

        for future in as_completed(futures, timeout=60):
            try:
                future.result()
            except Exception as e:
                print(f"⚠️ Ошибка выполнения теста: {e}")

    benchmark.print_results()
    benchmark.plot_results()

    # Очистка кэша
    if hasattr(fastregex_module, 'clear_cache'):
        fastregex_module.clear_cache()
        print("\n[LOG] Кэш очищен")

if __name__ == "__main__":
    main()