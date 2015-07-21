#!/usr/bin/env python
import subprocess

subprocess.call("cd ~/Documents/linux/barsm/", shell=True)
subprocess.call("gcc barsm.c -o barsm", shell=True)
subprocess.call("./barsm", shell=True)

proc = subprocess.Popen(['tail', '-500', 'mylogfile.log'], stdout=subprocess.PIPE)

for line in proc.stdout.readlines():
    print line.rstrip()