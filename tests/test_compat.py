import sys
from pathlib import Path
import unittest
import re
import timeit
import random
import string
from matplotlib import pyplot as plt
import numpy as np
import logging
from typing import List, Tuple, Dict
import traceback
import platform
from datetime import datetime

# Настройка логирования
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler(f'test_compat_{datetime.now().strftime("%Y%m%d_%H%M")}.log')
    ]
)
logger = logging.getLogger(__name__)

# Добавляем путь к модулю
sys.path.insert(0, str(Path(__file__).parent.parent))
try:
    import fastregex
    IMPORT_SUCCESS = True
except ImportError as e:
    logger.error(f"Ошибка импорта fastregex: {e}")
    IMPORT_SUCCESS = False


class RegexCompatibilityTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        """Инициализация тестовых данных с LLVM-совместимыми паттернами"""
        if not IMPORT_SUCCESS:
            raise unittest.SkipTest("Модуль fastregex не найден")

        cls.system_info = {
            "platform": platform.platform(),
            "python": platform.python_version(),
            "cpu": platform.processor()
        }

        # Тестовые паттерны с заменой метасимволов
        cls.test_patterns = [
            # (название, паттерн, текст, ожидаемый результат)
            ("Литерал", "abc", "123abcdef", True),
            ("Цифры", r"[0-9]+", "abc123def", True),  # Замена \d+
            ("Границы", r"\bword\b", "word wordword", True),
            ("Негативный", r"[0-9]{5}", "abc123", False),  # Замена \d
            ("Сложный", r"[0-9]{3}-[0-9]{2}-[0-9]{4}", "123-45-6789", True),
            ("Спецсимволы", r"[^a-zA-Z0-9_]+", "test!@#test", True),  # Замена \W
            ("Пустая строка", r"[0-9]+", "", False),
            ("Слова", r"[a-zA-Z0-9_]+", "word123", True)  # Замена \w+
        ]

        # Генерация тестового текста
        cls.large_text = cls.generate_text(100000)
        cls.performance_iterations = 100

        logger.info("\n" + "=" * 80)
        logger.info("Начало тестирования совместимости")
        logger.info(f"Система: {cls.system_info}")
        logger.info(f"Размер тестового текста: {len(cls.large_text):,} символов")
        logger.info("Используемые паттерны:")
        for p in cls.test_patterns:
            logger.info(f"  {p[0]}: {p[1]}")

    @staticmethod
    def generate_text(length: int) -> str:
        """Генерация реалистичного тестового текста"""
        word_chars = string.ascii_letters + string.digits + '_'
        space_chars = ' \t\n'
        text = []

        # Генерация слов разной длины
        while len(text) < length:
            word_len = random.randint(1, 10)
            word = ''.join(random.choices(word_chars, k=word_len))
            space = random.choice(space_chars) * random.randint(0, 3)
            text.append(word + space)

        return ''.join(text)[:length]

    def assert_regex_compatibility(self, pattern: str, text: str, expected: bool):
        """Проверка совместимости fastregex с re"""
        try:
            # Проверка fastregex
            fr_result = fastregex.search(pattern, text)
            self.assertIsInstance(fr_result, bool)

            # Проверка стандартного re
            re_result = re.search(pattern, text) is not None

            # Сравнение результатов
            self.assertEqual(
                fr_result, re_result,
                f"Несоответствие для '{pattern}': "
                f"fastregex={fr_result}, re={re_result}"
            )

            # Проверка ожидаемого результата
            self.assertEqual(fr_result, expected)

        except Exception as e:
            logger.error(f"Ошибка при тестировании {pattern}:\n{traceback.format_exc()}")
            raise

    def test_basic_compatibility(self):
        """Тест базовой совместимости"""
        logger.info("\nЗапуск тестов базовой совместимости...")
        for name, pattern, text, expected in self.test_patterns:
            with self.subTest(name=name, pattern=pattern):
                logger.info(f"Тестирование: {name} ({pattern})")
                self.assert_regex_compatibility(pattern, text, expected)

    def test_performance_comparison(self):
        """Сравнение производительности"""
        logger.info("\nЗапуск тестов производительности...")
        results = []

        for name, pattern, _, _ in self.test_patterns[:5]:
            with self.subTest(name=name):
                logger.info(f"Тестирование производительности: {name}")

                # Замер времени для re
                re_time = timeit.timeit(
                    lambda: re.search(pattern, self.large_text),
                    number=self.performance_iterations
                )

                # Замер времени для fastregex
                fr_time = timeit.timeit(
                    lambda: fastregex.search(pattern, self.large_text),
                    number=self.performance_iterations
                )

                results.append((name, re_time, fr_time))

                logger.info(
                    f"Результаты: re={re_time:.4f}s, "
                    f"fastregex={fr_time:.4f}s, "
                    f"ratio={fr_time / re_time:.2f}x"
                )

        # Визуализация результатов
        self.plot_performance_results(results)

    def plot_performance_results(self, results: List[Tuple[str, float, float]]):
        """Визуализация результатов производительности"""
        try:
            # Проверяем доступные стили и выбираем подходящий
            available_styles = plt.style.available
            style = 'seaborn-v0_8' if 'seaborn-v0_8' in available_styles else \
                   'seaborn' if 'seaborn' in available_styles else \
                   'ggplot' if 'ggplot' in available_styles else 'default'
            plt.style.use(style)
        except Exception as e:
            logger.warning(f"Не удалось установить стиль: {e}")
            plt.style.use('default')

        names = [r[0] for r in results]
        re_times = [r[1] * 1000 for r in results]  # Конвертируем в миллисекунды
        fr_times = [r[2] * 1000 for r in results]

        x = np.arange(len(names))
        width = 0.35

        fig, ax = plt.subplots(figsize=(14, 7))

        # Построение графиков
        re_bars = ax.bar(x - width/2, re_times, width, label='re', color='#1f77b4')
        fr_bars = ax.bar(x + width/2, fr_times, width, label='fastregex', color='#ff7f0e')

        # Настройка графика
        ax.set_title(
            'Сравнение производительности fastregex и re\n'
            f'({datetime.now().strftime("%Y-%m-%d %H:%M")})',
            fontsize=14,
            pad=20
        )
        ax.set_ylabel('Время выполнения (мс)', fontsize=12)
        ax.set_xticks(x)
        ax.set_xticklabels(names, rotation=45, ha='right', fontsize=10)
        ax.legend(fontsize=12)

        # Добавление значений на столбцы
        for bars in (re_bars, fr_bars):
            for bar in bars:
                height = bar.get_height()
                ax.annotate(
                    f'{height:.1f}',
                    xy=(bar.get_x() + bar.get_width()/2, height),
                    xytext=(0, 3),
                    textcoords='offset points',
                    ha='center',
                    va='bottom',
                    fontsize=9
                )

        # Сохранение графика
        plt.tight_layout()
        filename = f"performance_comparison_{datetime.now().strftime('%Y%m%d_%H%M')}.png"
        try:
            plt.savefig(filename, dpi=300, bbox_inches='tight')
            logger.info(f"График сохранен как: {filename}")
        except Exception as e:
            logger.error(f"Ошибка при сохранении графика: {e}")
        finally:
            plt.close()

    def test_caching_behavior(self):
        """Тестирование поведения кэширования"""
        logger.info("\nЗапуск теста кэширования...")
        pattern = r"[a-zA-Z0-9_]+\d+"  # Замена \w+\d+

        # Первый вызов (компиляция)
        first_time = timeit.timeit(
            lambda: fastregex.search(pattern, self.large_text),
            number=1
        )

        # Повторные вызовы (использование кэша)
        cached_time = timeit.timeit(
            lambda: fastregex.search(pattern, self.large_text),
            number=100
        ) / 100

        speedup = first_time / cached_time
        logger.info(
            f"Кэширование: первый вызов={first_time:.6f}s, "
            f"кэшированный={cached_time:.6f}s, "
            f"ускорение={speedup:.1f}x"
        )

        self.assertGreater(
            speedup, 2,
            f"Ожидалось ускорение в 2+ раз, получено {speedup:.1f}x"
        )


if __name__ == "__main__":
    # Настройка запуска тестов
    unittest.main(
        testRunner=unittest.TextTestRunner(verbosity=2),
        failfast=True,
        argv=['first-arg-is-ignored'] + sys.argv[1:]
    )