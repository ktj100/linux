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

    # Clear old test output
    test.clear_old_output('test.out')
    test.clear_old_output('fpga.out')

    test.run_tests(test_dirs, manual_tests)

