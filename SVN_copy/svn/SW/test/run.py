#!/usr/bin/env python3

test_dirs = [
    'barsm',
    'simm',
    'fdl',
]

if __name__ == '__main__':
    # Check for an optional test name argument
    import sys
    manual_tests = []
    if len(sys.argv) > 1:
        manual_tests = sys.argv[1:]

    from utils import test
    test.run_tests(test_dirs, manual_tests)
