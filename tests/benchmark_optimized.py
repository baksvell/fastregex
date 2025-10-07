#!/usr/bin/env python3
"""
Бенчмарк для тестирования оптимизированного FastRegex
"""

import time
import fastregex
import re
import random
import string

def generate_test_data(size=10000):
    """Генерирует тестовые данные"""
    # Смешанные данные: буквы, цифры, специальные символы
    chars = string.ascii_letters + string.digits + " \n\t.,!?@#$%^&*()"
    return ''.join(random.choice(chars) for _ in range(size))

def benchmark_literal_search():
    """Бенчмарк для поиска литералов"""
    print("=== БЕНЧМАРК: ПОИСК ЛИТЕРАЛОВ ===")
    
    # Тестовые данные
    text = generate_test_data(50000)
    patterns = [
        "hello",           # Короткий паттерн
        "test123",         # Средний паттерн
        "very_long_pattern_for_testing_simd_optimizations",  # Длинный паттерн
        "nonexistent_pattern_that_should_not_be_found_anywhere_in_text"  # Несуществующий
    ]
    
    for pattern in patterns:
        print(f"\nПаттерн: '{pattern}' (длина: {len(pattern)})")
        
        # FastRegex
        fr_pattern = fastregex.compile(pattern)
        
        # Тест match
        start = time.time()
        for _ in range(1000):
            fr_pattern.match(text)
        fr_match_time = time.time() - start
        
        # Тест search
        start = time.time()
        for _ in range(1000):
            fr_pattern.search(text)
        fr_search_time = time.time() - start
        
        # Стандартный re
        start = time.time()
        for _ in range(1000):
            re.match(pattern, text)
        re_match_time = time.time() - start
        
        start = time.time()
        for _ in range(1000):
            re.search(pattern, text)
        re_search_time = time.time() - start
        
        print(f"  Match - FastRegex: {fr_match_time:.4f}s, re: {re_match_time:.4f}s")
        print(f"  Search - FastRegex: {fr_search_time:.4f}s, re: {re_search_time:.4f}s")
        
        if re_match_time > 0:
            print(f"  Ускорение match: {re_match_time/fr_match_time:.2f}x")
        if re_search_time > 0:
            print(f"  Ускорение search: {re_search_time/fr_search_time:.2f}x")

def benchmark_regex_patterns():
    """Бенчмарк для регулярных выражений"""
    print("\n=== БЕНЧМАРК: РЕГУЛЯРНЫЕ ВЫРАЖЕНИЯ ===")
    
    text = generate_test_data(30000)
    patterns = [
        r'\d+',           # Цифры
        r'[a-zA-Z]+',     # Буквы
        r'\w+@\w+\.\w+',  # Email
        r'https?://\S+',  # URL
    ]
    
    for pattern in patterns:
        print(f"\nПаттерн: {pattern}")
        
        # FastRegex
        fr_pattern = fastregex.compile(pattern)
        
        # Тест search
        start = time.time()
        for _ in range(1000):
            fr_pattern.search(text)
        fr_time = time.time() - start
        
        # Стандартный re
        start = time.time()
        for _ in range(1000):
            re.search(pattern, text)
        re_time = time.time() - start
        
        print(f"  FastRegex: {fr_time:.4f}s, re: {re_time:.4f}s")
        if re_time > 0:
            print(f"  Ускорение: {re_time/fr_time:.2f}x")

def benchmark_find_all():
    """Бенчмарк для find_all"""
    print("\n=== БЕНЧМАРК: FIND_ALL ===")
    
    text = "hello world hello test hello python " * 1000
    pattern = "hello"
    
    # FastRegex
    fr_pattern = fastregex.compile(pattern)
    
    start = time.time()
    for _ in range(100):
        results = fr_pattern.find_all(text)
    fr_time = time.time() - start
    
    # Стандартный re
    start = time.time()
    for _ in range(100):
        results = re.findall(pattern, text)
    re_time = time.time() - start
    
    print(f"FastRegex: {fr_time:.4f}s, re: {re_time:.4f}s")
    if re_time > 0:
        print(f"Ускорение: {re_time/fr_time:.2f}x")

def main():
    """Основная функция"""
    print("FastRegex Performance Benchmark")
    print("=" * 50)
    
    try:
        benchmark_literal_search()
        benchmark_regex_patterns()
        benchmark_find_all()
        
        print("\n=== РЕЗУЛЬТАТЫ ===")
        print("Бенчмарк завершен успешно!")
        print("Проверьте результаты выше для анализа производительности.")
        
    except Exception as e:
        print(f"Ошибка во время бенчмарка: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()
