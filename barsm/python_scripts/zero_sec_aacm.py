#!/usr/bin/env python

# This program will place an executable only in the AACM directory that instantly ends.
# BARSM should try to launch it five times, then exit.

import sys
import time
import subprocess
import proc
import signal
import os

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
    subprocess.call("rm /opt/rc360/system/*", shell=True)
    subprocess.call("rm /opt/rc360/modules/GE/*", shell=True)
    subprocess.call("rm /opt/rc360/modules/TPA/*", shell=True)
    subprocess.call("rm /opt/rc360/apps/GE/*", shell=True)
    subprocess.call("rm /opt/rc360/apps/TPA/*", shell=True)

    # compile needed applications and place in required directories
    subprocess.call("gcc ~/Documents/linux/barsm/dummies/zero_sec.c -o /opt/rc360/system/zero_sec_aacm", shell=True)
    # subprocess.call("gcc ~/Documents/linux/barsm/dummies/thirty_sec.c -o /opt/rc360/system/thirty_sec_aacm", shell=True)

    # start BARSM
    test_output = open('test.out', 'w')

    subprocess.call("gcc ~/Documents/linux/barsm/barsm.c -o ~/Documents/linux/barsm/barsm", shell=True)
    subprocess.Popen("valgrind ~/Documents/linux/barsm/barsm -v --read-var-info=yes --leak-check=full --track-origins=yes --show-reachable=yes --malloc-fill=B5 --free-fill=4A", stdout=test_output, stderr=test_output, shell=True)

    # monitor SYSLOG

    # THE FOLLOWING IS FUNTIONAL TEST CODE TO MAKE SURE BARSM QUITS WITHIN 30 SECONDS OF STARTING
    # # barsm_info stores the list of the one directory containing BARSM's info
    # barsm_info = proc.findproc("valgrind.bin")l
    # # '0' selects the one and only directory in the list, and 'pid' selects the item in the directory.
    # barsm_pid = int(barsm_info[0]['pid'])
    # print(barsm_info[0]['pid'])

    # # BARSM should quit within about 10 seconds. 
    # # This will quit when BARSM quits or 30 seconds pass.
    # run_time = 0
    # while barsm_info and run_time <= 30:
    #     time.sleep(1)
    #     run_time += 1
    #     barsm_info = proc.findproc("valgrind.bin")

    # # If BARSM is still alive, kill BARSM
    # if barsm_info:
    #     # print(barsm_pid)
    #     os.kill(barsm_pid, signal.SIGKILL)
    

    # Ensure that BARSM tries to start aacm exactly five times./
    barsm_info = proc.findproc("valgrind.bin")
    # '0' selects the one and only directory in the list, and 'pid' selects the item in the directory.
    barsm_pid = (barsm_info[0]['pid'])
    print(barsm_info[0]['pid'])

    run_time = 0
    child_amount = 0
    start_tries = 0
    while run_time <= 30 and barsm_info:
        all_child = proc.findchild(barsm_pid)
        print(all_child)

        # update number of children
        # if there were none last time
        if child_amount == 0:
            # check if there is one now
            if len(all_child) >= 1:
                child_amount = 1
                child_pid = int(all_child[0]['pid'])
                start_tries += 1

        # if there was one last time
        else:
            # check if there is one now
            if len(all_child) < 1:
                child_amount = 0

            elif child_pid != int(all_child[0]['pid']):
                start_tries += 1
                child_pid = int(all_child[0]['pid'])

        # if child_amount == len(all_child):
        #     for c in all_child:
        #         if child_pids[c] != all_child[c]['pid']:

        time.sleep(0.1)
        barsm_info = proc.findproc("valgrind.bin")

    if start_tries == 5:
        print("SUCCESS")
    else:
        print("FAIL")

    print("ENDED SCRIPT")

    # Ensure that BARSM quits after five tries and exits.