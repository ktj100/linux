#!/usr/bin/env python

# place one 170 sec, one 50 sec, one 0 sec, one non executable, and one infinite app/module in each directory
# start barsm 
# watch for creation of a child PID
# check that only AACM has started
# make sure that all six of the next modules starting up are from the modules directories
# make sure that both the 0 sec and non executables try to start 5 times each
# make sure errors are logged for each startup try
# make sure that six more start up, and they match the six applications
# make sure that both the 0 sec and non executables try to start 5 times each
# make sure errors are logged for each startup try
# make sure that barsm replaces all ended processes, and only ended processes
# 1
# 6
# 7
# 8
# 9
# 10
# 11
# 12
# 13
# 14
# 15

# place a non executable file in the AACM directory
# start barsm
# watch for five log errors while attempting to start AACM
# watch for end of AACM
# 2
# 3

# place a 0 sec file in AACM directory
# start barsm
# watch for five log errors while attempting to start AACM
# watch for end of AACM
# 4
# 5

import sys
import time
import subprocess

# This program places compiled versions of each application type into each directory
if __name__ == '__main__':
    # clear out all app directories
    subprocess.call("rm /opt/rc360/system/*", shell=True)
    subprocess.call("rm /opt/rc360/modules/GE/*", shell=True)
    subprocess.call("rm /opt/rc360/modules/TPA/*", shell=True)
    subprocess.call("rm /opt/rc360/apps/GE/*", shell=True)
    subprocess.call("rm /opt/rc360/apps/TPA/*", shell=True)

    # AACM
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/zero_sec.c -o /opt/rc360/system/zero_sec_aacm", shell=True)
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/thirty_sec.c -o /opt/rc360/system/thirty_sec_aacm", shell=True)
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/hundred_sec.c -o /opt/rc360/system/hundred_sec_aacm", shell=True)
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/infinite_sec.c -o /opt/rc360/system/infinite_sec_aacm", shell=True)
    subprocess.call("cp ~/Documents/linux/barsm/dummies/non_exec.c /opt/rc360/system/", shell=True)

    # GE MODULES
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/zero_sec.c -o /opt/rc360/modules/GE/zero_sec_ge_mod", shell=True)
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/thirty_sec.c -o /opt/rc360/modules/GE/thirty_sec_ge_mod", shell=True)
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/hundred_sec.c -o /opt/rc360/modules/GE/hundred_sec_ge_mod", shell=True)
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/infinite_sec.c -o /opt/rc360/modules/GE/infinite_sec_ge_mod", shell=True)
    subprocess.call("cp ~/Documents/linux/barsm/dummies/non_exec.c /opt/rc360/modules/GE/", shell=True)

    # TPA MODULES
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/zero_sec.c -o /opt/rc360/modules/TPA/zero_sec_tpa_mod", shell=True)
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/thirty_sec.c -o /opt/rc360/modules/TPA/thirty_sec_tpa_mod", shell=True)
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/hundred_sec.c -o /opt/rc360/modules/TPA/hundred_sec_tpa_mod", shell=True)
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/infinite_sec.c -o /opt/rc360/modules/TPA/infinite_sec_tpa_mod", shell=True)
    subprocess.call("cp ~/Documents/linux/barsm/dummies/non_exec.c /opt/rc360/modules/TPA/", shell=True)

    # GE APPS
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/zero_sec.c -o /opt/rc360/apps/GE/zero_sec_ge_app", shell=True)
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/thirty_sec.c -o /opt/rc360/apps/GE/thirty_sec_ge_app", shell=True)
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/hundred_sec.c -o /opt/rc360/apps/GE/hundred_sec_ge_app", shell=True)
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/infinite_sec.c -o /opt/rc360/apps/GE/infinite_sec_ge_app", shell=True)
    subprocess.call("cp ~/Documents/linux/barsm/dummies/non_exec.c /opt/rc360/apps/GE/", shell=True)

    # TPA APPS
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/zero_sec.c -o /opt/rc360/apps/TPA/zero_sec_tpa_app", shell=True)
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/thirty_sec.c -o /opt/rc360/apps/TPA/thirty_sec_tpa_app", shell=True)
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/hundred_sec.c -o /opt/rc360/apps/TPA/hundred_sec_tpa_app", shell=True)
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/infinite_sec.c -o /opt/rc360/apps/TPA/infinite_sec_tpa_app", shell=True)
    subprocess.call("cp ~/Documents/linux/barsm/dummies/non_exec.c /opt/rc360/apps/TPA/", shell=True)
