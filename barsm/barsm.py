#!/usr/bin/env python
import subprocess

subprocess.call("cd ~/Documents/linux/barsm/", shell=True)
subprocess.call("gcc barsm.c -o barsm", shell=True)
subprocess.call("./barsm", shell=True)