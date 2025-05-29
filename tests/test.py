import sys
import os
import ctypes
import unittest
from pathlib import Path
import traceback
import re  # Для проверки ожидаемого поведения

# Настройка путей
project_root = Path(__file__).parent.parent
sys.path.insert(0, str(project_root))


class TestFastRegex(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        """Инициализация перед всеми тестами"""
        print("\n=== Инициализация тестовой среды ===")
        cls.dll_dir = project_root
        cls.load_dependencies()
        cls.import_module()

    @classmethod
    def load_dependencies(cls):
        """Загрузка и проверка DLL-зависимостей"""
        print("\n[1/2] Проверка системных зависимостей:")
        dlls = [
            'libffi-8.dll',
            'libgcc_s_seh-1.dll',
            'libwinpthread-1.dll',
            'libstdc++-6.dll',
            'zlib1.dll',
            'libzstd.dll',
            'libxml2-2.dll',
            'libLLVM-20.dll'
        ]

        os.environ['PATH'] = f"{cls.dll_dir};{os.environ['PATH']}"

        for dll in dlls:
            dll_path = cls.dll_dir / dll
            try:
                ctypes.CDLL(str(dll_path))
                print(f"  ✓ {dll:20} загружена успешно")
            except Exception as e:
                print(f"  ✗ {dll:20} ошибка загрузки: {str(e)}")
                raise RuntimeError(f"Не удалось загрузить {dll}") from e

    @classmethod
    def import_module(cls):
        """Импорт и проверка модуля"""
        print("\n[2/2] Инициализация модуля fastregex:")
        try:
            global fastregex
            import fastregex
            print(f"  ✓ Модуль загружен из: {fastregex.__file__}")
        except Exception as e:
            print(f"  ✗ Ошибка импорта: {traceback.format_exc()}")
            raise

    def setUp(self):
        """Настройка перед каждым тестом"""
        self.test_data = [
            # (pattern, text, expected_match, expected_search)
            ("abc", "abc", True, True),      # match: True (полное совпадение)
            ("abc", "aabc", False, True),    # match: False (не с начала строки)
            ("abc", "def", False, False),    # match: False (нет совпадения)
            (r"\d+", "123", True, True),     # match: True (цифры с начала)
            (r"\d+", "abc123", False, True), # match: False (не цифры в начале)
            (r"^\d+$", "123", True, True)    # match: True (полное совпадение)
        ]

    def test_module_interface(self):
        """Тестирование наличия всех методов"""
        print("\nТест 1: Проверка интерфейса модуля")
        required_methods = ['match', 'search', 'compile', 'FastRegex']
        for method in required_methods:
            with self.subTest(method=method):
                self.assertTrue(hasattr(fastregex, method),
                              f"Метод {method} отсутствует")

    def test_basic_functionality(self):
        """Базовые тесты на корректность работы"""
        print("\nТест 2: Базовые функциональные тесты")
        for pattern, text, exp_match, exp_search in self.test_data:
            with self.subTest(pattern=pattern, text=text):
                print(f"\nТестируем: match('{pattern}', '{text}')")
                try:
                    # Проверяем ожидаемое поведение через re.match
                    py_match = bool(re.match(pattern, text))
                    self.assertEqual(py_match, exp_match,
                                   f"Ожидаемое поведение Python re.match: {py_match}")

                    result = fastregex.match(pattern, text)
                    self.assertIsInstance(result, bool)
                    self.assertEqual(result, exp_match,
                                   f"Ожидалось: {exp_match}, получено: {result}")
                    print(f"  ✓ match: {result} (ожидалось: {exp_match})")
                except Exception as e:
                    print(f"  ✗ Ошибка: {traceback.format_exc()}")
                    raise

                print(f"Тестируем: search('{pattern}', '{text}')")
                try:
                    # Проверяем ожидаемое поведение через re.search
                    py_search = bool(re.search(pattern, text))
                    self.assertEqual(py_search, exp_search,
                                   f"Ожидаемое поведение Python re.search: {py_search}")

                    result = fastregex.search(pattern, text)
                    self.assertIsInstance(result, bool)
                    self.assertEqual(result, exp_search,
                                   f"Ожидалось: {exp_search}, получено: {result}")
                    print(f"  ✓ search: {result} (ожидалось: {exp_search})")
                except Exception as e:
                    print(f"  ✗ Ошибка: {traceback.format_exc()}")
                    raise

    def test_compiled_regex(self):
        """Тестирование компилированных выражений"""
        print("\nТест 3: Работа с компилированными regex")
        for pattern, text, exp_match, _ in self.test_data:
            with self.subTest(pattern=pattern, text=text):
                try:
                    print(f"\nКомпилируем: '{pattern}'")
                    regex = fastregex.compile(pattern)
                    self.assertTrue(callable(regex.match))
                    self.assertTrue(callable(regex.search))

                    result = regex.match(text)
                    self.assertEqual(result, exp_match,
                                   f"Ожидалось: {exp_match}, получено: {result}")
                    print(f"  ✓ compiled.match: {result} (ожидалось: {exp_match})")
                except Exception as e:
                    print(f"  ✗ Ошибка: {traceback.format_exc()}")
                    raise

    def test_error_handling(self):
        """Тестирование обработки ошибок"""
        print("\nТест 4: Обработка некорректных входных данных")
        test_cases = [
            (None, "abc"),
            ("abc", None),
            (123, "abc"),
            ("abc", 123)
        ]

        for pattern, text in test_cases:
            with self.subTest(pattern=pattern, text=text):
                print(f"\nТестируем невалидные данные: {pattern}, {text}")
                try:
                    with self.assertRaises((TypeError, ValueError)):
                        fastregex.match(pattern, text)
                    print("  ✓ Ошибка обработана корректно")
                except Exception as e:
                    print(f"  ✗ Неожиданное поведение: {traceback.format_exc()}")
                    raise


if __name__ == "__main__":
    print("=== Запуск комплексного тестирования fastregex ===")
    print(f"Рабочая директория: {os.getcwd()}")
    print(f"Python path: {sys.path}")

    try:
        unittest.main(argv=['first-arg-is-ignored'], exit=False)
    except Exception as e:
        print(f"\n!!! Критическая ошибка: {traceback.format_exc()}")

    print("\n=== Тестирование завершено ===")
    if '--wait' in sys.argv:
        input("Нажмите Enter для выхода...")