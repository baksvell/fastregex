import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))

import unittest
import fastregex
import re
import time
import threading

# Добавляем корень проекта в путь Python
project_root = Path(__file__).parent.parent
sys.path.insert(0, str(project_root))


class TimeoutException(Exception):
    pass


def run_with_timeout(func, timeout=5):
    """Альтернативная реализация таймаута для Windows"""
    result = [None]
    exc = [None]

    def wrapper():
        try:
            result[0] = func()
        except Exception as e:
            exc[0] = e

    thread = threading.Thread(target=wrapper)
    thread.daemon = True
    thread.start()
    thread.join(timeout)

    if thread.is_alive():
        raise TimeoutException(f"Function timed out after {timeout} seconds")
    if exc[0]:
        raise exc[0]
    return result[0]


class TestBasic(unittest.TestCase):
    def test_pattern_types(self):
        test_cases = [
            # (pattern, text, expected_match, expected_search)
            ("abc", "abc", True, True),
            ("abc", "abcd", True, True),
            ("abc", "aabc", False, True),
            ("abc", "ab", False, False),
            (r"\d+", "123", True, True),
            (r"\d+", "abc123", False, True),
            (r"^\d+$", "123", True, True)
        ]

        for pattern, text, exp_match, exp_search in test_cases:
            with self.subTest(pattern=pattern, text=text):
                # Проверка match
                fr_match = fastregex.match(pattern, text)
                self.assertEqual(fr_match, exp_match,
                                 f"match({pattern!r}, {text!r}): ожидалось {exp_match}, получено {fr_match}")

                # Проверка search
                fr_search = fastregex.search(pattern, text)
                self.assertEqual(fr_search, exp_search,
                                 f"search({pattern!r}, {text!r}): ожидалось {exp_search}, получено {fr_search}")

    def test_compatibility_with_re(self):
        """Проверка совместимости с re"""
        patterns = [
            (r"\w+", "start 123 end"),  # match=False (не с начала), search=True
            (r"\d{3}", "start 123 end"),  # match=False, search=True
            (r"start", "start 123 end"),  # match=True, search=True
            (r"end$", "start 123 end")  # match=False, search=True
        ]

        print("\nTesting compatibility with re module...")
        for pattern, text in patterns:
            with self.subTest(pattern=pattern):
                print(f"  Testing pattern: {pattern!r} in {text!r}")

                # Ожидаемые результаты от re
                re_match = bool(re.match(pattern, text))
                re_search = bool(re.search(pattern, text))

                # Получаем результаты fastregex
                try:
                    fr_match = run_with_timeout(
                        lambda: bool(fastregex.match(pattern, text)),
                        timeout=3
                    )
                    fr_search = run_with_timeout(
                        lambda: bool(fastregex.search(pattern, text)),
                        timeout=3
                    )
                except TimeoutException:
                    self.fail("fastregex timed out")
                except Exception as e:
                    self.fail(f"Unexpected error: {str(e)}")

                # Для match ожидаем такое же поведение как у re.match
                self.assertEqual(re_match, fr_match,
                                 f"match не совпадает с re.match для {pattern}")

                # Для search ожидаем такое же поведение как у re.search
                self.assertEqual(re_search, fr_search,
                                 f"search не совпадает с re.search для {pattern}")


if __name__ == '__main__':
    unittest.main(verbosity=2)