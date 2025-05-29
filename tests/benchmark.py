import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent))

import timeit
import fastregex
import re
import sys
import time
from threading import Thread, Event


class TimeoutException(Exception):
    pass


def run_with_timeout(func, timeout=5):
    """Выполняет функцию с таймаутом"""
    result = [None]
    exc = [None]
    event = Event()

    def wrapper():
        try:
            result[0] = func()
        except Exception as e:
            exc[0] = e
        finally:
            event.set()

    thread = Thread(target=wrapper)
    thread.daemon = True
    thread.start()

    if not event.wait(timeout):
        raise TimeoutException(f"Функция превысила таймаут {timeout} сек")

    if exc[0]:
        raise exc[0]

    return result[0]


def test_pattern(pattern, text):
    """Тестирует один паттерн"""
    print(f"\n=== Тестирование паттерна: {pattern} ===")
    print(f"Длина текста: {len(text)} символов")

    # Проверка корректности
    try:
        fr_result = run_with_timeout(
            lambda: fastregex.search(pattern, text),
            timeout=3
        )
        re_result = re.search(pattern, text)

        print(f"\nРезультаты:")
        print(f"fastregex.search(): {fr_result}")
        print(f"re.search(): {re_result is not None}")

        if bool(fr_result) != bool(re_result):
            print("❌ Результаты не совпадают!")
            return False

        print("✅ Результаты совпадают")
        return True

    except Exception as e:
        print(f"❌ Ошибка: {str(e)}")
        return False


def run_benchmark():
    try:
        # Простые и сложные паттерны для теста
        test_cases = [
            (r"\d+", "abc123 " * 1000),  # Простой числовой паттерн
            (r"\w+", "word123 " * 1000),  # Простой словесный паттерн
            (r"\w+\d+", "word123 " * 1000),  # Комбинированный
            (r"\d{3}", "abc123 " * 1000)  # Фиксированная длина
        ]

        print("=== Комплексное тестирование производительности ===")

        for pattern, text in test_cases:
            if not test_pattern(pattern, text):
                continue  # Пропускаем benchmark если тест не прошёл

            # Benchmark
            print("\nЗамер производительности...")

            # Прогрев кэша
            fastregex.search(pattern, text)

            # Тестируем fastregex
            fr_time = timeit.timeit(
                lambda: fastregex.search(pattern, text),
                number=100
            )

            # Тестируем re
            re_time = timeit.timeit(
                lambda: re.search(pattern, text),
                number=100
            )

            print("\nРезультаты (100 вызовов):")
            print(f"fastregex: {fr_time:.4f} сек")
            print(f"re: {re_time:.4f} сек")
            print(f"Отношение fastregex/re: {fr_time / re_time:.2f}x")
            print("-" * 50)

        return 0

    except Exception as e:
        print(f"\n⚠️ Критическая ошибка: {str(e)}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    print("=== Запуск теста производительности ===")
    ret_code = run_benchmark()
    print("\n=== Тест завершен ===")
    sys.exit(ret_code)