#!/usr/bin/env python

# This program will place an executable only in the AACM directory that instantly ends.
# BARSM should try to launch it five times, then exit.

# TODO:
# 1. Change file paths into Python equivalent of #define 

import sys
import time
import subprocess
import proc
import signal
import os
import log
import barsm

if __name__ == '__main__':

    # navigate to the end of the SYSLOG
    follow = log.logfollower()

    # clear out all app directories
    barsm.remove()

    # compile needed applications and place in required directories
    subprocess.call('gcc '+barsm.apps_loc+'zero_sec.c -o /opt/rc360/system/zero_sec_aacm', shell=True)

    # start BARSM
    test_output = open('test.out', 'w')

    subprocess.call('gcc '+barsm.barsm_loc+'barsm.c -o '+barsm.barsm_loc+'barsm', shell=True)
    subprocess.Popen('valgrind '+barsm.barsm_loc+'barsm -v --read-var-info=yes --leak-check=full --track-origins=yes ' \
        'show-reachable=yes --malloc-fill=B5 --free-fill=4A', stdout=test_output, stderr=test_output, shell=True)

    # # Ensure that BARSM tries to start aacm exactly five times./
    # barsm_info = proc.findproc("valgrind.bin")
    # # '0' selects the one and only directory in the list, and 'pid' selects the item in the directory.
    # barsm_pid = (barsm_info[0]['pid'])
    # print(barsm_info[0]['pid'])

    # run_time = 0
    # child_amount = 0
    # start_tries = 0
    # while 30 >= run_time and barsm_info:
    #     # print(follow.read())
    #     all_child = proc.findchild(barsm_pid)
    #     print(all_child)

    #     # update number of children
    #     # if there were none last time
    #     if 0 == child_amount:
    #         # check if there is one now
    #         if 1 <= len(all_child):
    #             child_amount = 1
    #             child_pid = int(all_child[0]['pid'])
    #             start_tries += 1

    #     # if there was one last time
    #     else:
    #         # check if there is one now
    #         if 1 > len(all_child):
    #             child_amount = 0

    #         elif int(all_child[0]['pid']) != child_pid:
    #             start_tries += 1
    #             child_pid = int(all_child[0]['pid'])

    #     time.sleep(0.1)
    #     run_time += 0.1
    #     barsm_info = proc.findproc("valgrind.bin")

    launch_result = monitor_launch()
    if launch_result != 0:
        print("FAIL")

    else:
        # check logs to see if five errors were logged for each launch failure
        logs = follow.read()
        launch_errors = sum(1 for d in logs if "File launch failed!" in d.get('message') and "/opt/rc360/system/" in d.get('message'))

        if 5 == launch_errors:
            print("SUCCESS")
        else:
            print("FAIL")

    print("SCRIPT ENDED")