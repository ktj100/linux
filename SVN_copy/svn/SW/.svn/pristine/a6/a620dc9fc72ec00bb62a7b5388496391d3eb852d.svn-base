#!/usr/bin/env python3

def find_py_files(x):
    import glob
    import re
    import os.path
    return [re.sub(r'(\S*)/(\S*)\.py', r'\1.\2', f) for f in glob.glob(os.path.join(x, '*.py'))]

def filter_test(x):
    import importlib
    try:
        return (x, getattr(importlib.import_module(x), 'run'))
    except:
        pass

def run_tests(dir_list):
    import itertools
    # get a list of all files in the test subdirectories
    tests = list(itertools.chain.from_iterable(map(find_py_files, dir_list)))

    # find out which files are valid tests
    tests = [x for x in map(filter_test, tests) if x]

    # randomize the list of tests
    import random
    random.shuffle(tests)

    # Run each test
    from . import colors
    passed = colors.colorify(colors.GREEN, 'PASSED')
    failed = colors.colorify(colors.RED, 'FAILED')

    import sys
    import traceback
    for t in tests:
        test_name = colors.colorify(colors.YELLOW, t[0])
        print('running test "{name}"'.format(name=test_name))
        result = False
        try:
            result = t[1]()
        except:
            print('caught exception while execution of test "{name}":\n{tb}'.format(name=test_name, tb=traceback.format_exception(*sys.exc_info())))

        print('test "{name}": {status}'.format(name=test_name, status=passed if result else failed))
