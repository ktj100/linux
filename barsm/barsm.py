#!/usr/bin/env python
import subprocess

subprocess.call("cd ~/Documents/linux/barsm/", shell=True)
subprocess.call("gcc barsm.c -o barsm", shell=True)
subprocess.call("./barsm", shell=True)
subprocess.call("valgrind ./barsm --read-var-info=yes --leak-check=full --track-origins=yes --show-reachable=yes --malloc-fill=B5 --free-fill=4A", shell=True)
