#!/usr/bin/env python3

test_dirs = [
    'barsm',
    'simm',
    'fdl',
]

if __name__ == '__main__':
    from utils import test
    test.run_tests(test_dirs)
