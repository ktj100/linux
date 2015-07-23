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

# import re
import sys
# import time
import subprocess
#
# import log
#
# if __name__ == '__main__':
#
#     if len(sys.argv) >= 2:
#         proc = sys.argv[1]
#     else:
#         proc = None
#
#     follow = log.logfollower()
#
#     # subprocess.call("cd ~/Documents/linux/barsm/", shell=True)
#     subprocess.call("gcc ~/Documents/linux/barsm/barsm.c -o barsm", shell=True)
#     # subprocess.call("~/Documents/linux/barsm/barsm", shell=True)
#     test_output = open('test.out', 'w')
#     subprocess.Popen("valgrind ~/Documents/linux/barsm/barsm -v --read-var-info=yes --leak-check=full --track-origins=yes --show-reachable=yes --malloc-fill=B5 --free-fill=4A", stdout=test_output, stderr=test_output, shell=True)
#
#     while True:
#         #print(follow.read(proc))  # we can specify a process ID on the command
#         logs = follow.read(proc)
#         errors = 0
#         # for "'ERROR:" in logs
#         #     errors = errors + 1
#         search = "ERROR"
#         for l in logs:
#             errors += l['message'].count(search)
#
#         print(logs)
#         print(errors)
#
#         time.sleep(1)

if __name__ == '__main__':
    # clear out all app directories
    subprocess.call("sudo rm /opt/rc360/system/*", shell=True)
    subprocess.call("sudo rm /opt/rc360/modules/GE/*", shell=True)
    subprocess.call("sudo rm /opt/rc360/modules/TPA/*", shell=True)
    subprocess.call("sudo rm /opt/rc360/apps/GE/*", shell=True)
    subprocess.call("sudo rm /opt/rc360/apps/TPA/*", shell=True)

    # compile needed applications
    subprocess.call("gcc /home/travis/Documents/linux/barsm/dummies/hundred_sec.c -o hundred_sec", shell=True)

    # send needed applications to necessary locations