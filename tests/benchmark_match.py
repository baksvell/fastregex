#!/usr/bin/env python3
"""
Специальный бенчмарк для тестирования оптимизированного match()
"""

import time
import fastregex
import re
import random
import string

def generate_test_data(size=10000):
    """Генерирует тестовые данные"""
    chars = string.ascii_letters + string.digits + " \n\t.,!?@#$%^&*()"
    return ''.join(random.choice(chars) for _ in range(size))

def benchmark_match_operations():
    """Бенчмарк для match операций"""
    print("=== БЕНЧМАРК: MATCH ОПЕРАЦИИ ===")
    
    # Тестовые данные
    test_cases = [
        ("hello", "hello world"),           # Совпадение с начала
        ("hello", "world hello"),           # Не совпадение с начала
        ("test123", "test123abc"),          # Совпадение с начала
        ("test123", "abctest123"),          # Не совпадение с начала
        ("123", "123456"),                  # Числа
        ("abc", "abcdef"),                  # Буквы
        ("hello", "hello"),                 # Точное совпадение
        ("hello", "hell"),                  # Короче паттерна
    ]
    
    for pattern, text in test_cases:
        print(f"\nПаттерн: '{pattern}', Текст: '{text}'")
        
        # FastRegex
        fr_pattern = fastregex.compile(pattern)
        
        # Тест match
        start = time.time()
        for _ in range(10000):
            fr_result = fr_pattern.match(text)
        fr_time = time.time() - start
        
        # Стандартный re
        start = time.time()
        for _ in range(10000):
            re_result = bool(re.match(pattern, text))
        re_time = time.time() - start
        
        print(f"  FastRegex: {fr_time:.4f}s, re: {re_time:.4f}s")
        print(f"  Результат: FastRegex={fr_result}, re={re_result}")
        
        if re_time > 0:
            speedup = re_time / fr_time
            print(f"  Ускорение: {speedup:.2f}x {'✅' if speedup > 1 else '❌'}")

def benchmark_special_patterns():
    """Бенчмарк для специальных паттернов"""
    print("\n=== БЕНЧМАРК: СПЕЦИАЛЬНЫЕ ПАТТЕРНЫ ===")
    
    test_cases = [
        (r'\d+', "123456"),                 # Только цифры
        (r'\d+', "abc123"),                 # Не только цифры
        (r'[a-zA-Z]+', "hello"),            # Только буквы
        (r'[a-zA-Z]+', "hello123"),         # Не только буквы
        (r'\w+', "hello123"),               # Буквы и цифры
        (r'\w+', "hello world"),            # С пробелом
    ]
    
    for pattern, text in test_cases:
        print(f"\nПаттерн: {pattern}, Текст: '{text}'")
        
        # FastRegex
        fr_pattern = fastregex.compile(pattern)
        
        # Тест match
        start = time.time()
        for _ in range(10000):
            fr_result = fr_pattern.match(text)
        fr_time = time.time() - start
        
        # Стандартный re
        start = time.time()
        for _ in range(10000):
            re_result = bool(re.match(pattern, text))
        re_time = time.time() - start
        
        print(f"  FastRegex: {fr_time:.4f}s, re: {re_time:.4f}s")
        print(f"  Результат: FastRegex={fr_result}, re={re_result}")
        
        if re_time > 0:
            speedup = re_time / fr_time
            print(f"  Ускорение: {speedup:.2f}x {'✅' if speedup > 1 else '❌'}")

def benchmark_large_data():
    """Бенчмарк для больших данных"""
    print("\n=== БЕНЧМАРК: БОЛЬШИЕ ДАННЫЕ ===")
    
    # Генерируем большие данные
    large_text = generate_test_data(100000)
    patterns = ["hello", "test123", r'\d+', r'[a-zA-Z]+']
    
    for pattern in patterns:
        print(f"\nПаттерн: {pattern}")
        
        # FastRegex
        fr_pattern = fastregex.compile(pattern)
        
        # Тест match
        start = time.time()
        for _ in range(1000):
            fr_result = fr_pattern.match(large_text)
        fr_time = time.time() - start
        
        # Стандартный re
        start = time.time()
        for _ in range(1000):
            re_result = bool(re.match(pattern, large_text))
        re_time = time.time() - start
        
        print(f"  FastRegex: {fr_time:.4f}s, re: {re_time:.4f}s")
        print(f"  Результат: FastRegex={fr_result}, re={re_result}")
        
        if re_time > 0:
            speedup = re_time / fr_time
            print(f"  Ускорение: {speedup:.2f}x {'✅' if speedup > 1 else '❌'}")

def main():
    """Основная функция"""
    print("FastRegex Match() Performance Benchmark")
    print("=" * 50)
    
    try:
        benchmark_match_operations()
        benchmark_special_patterns()
        benchmark_large_data()
        
        print("\n=== РЕЗУЛЬТАТЫ ===")
        print("Бенчмарк match() завершен!")
        print("Проверьте результаты выше для анализа производительности.")
        
    except Exception as e:
        print(f"Ошибка во время бенчмарка: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()
