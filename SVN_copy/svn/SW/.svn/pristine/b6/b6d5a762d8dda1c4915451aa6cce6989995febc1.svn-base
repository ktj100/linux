#!/usr/bin/env python

# This program will place an executable only in the AACM directory that instantly ends, or place
# a non-executable in the AACM directory. BARSM should try to launch it five times, then exit.


# add location of the utilities for this script to search out
import sys
sys.path.insert(0, '/svn/SW/test/utils')

import log
import barsm
import subprocess
import random

if __name__ == '__main__':

    # navigate to the end of the SYSLOG
    follow = log.logfollower()

    # clear out all app directories
    barsm.remove()

    # compile needed applications and place in required directories
    random.seed()
    r = random.randint(1, 2)
    print(r)
    if 1 == r:
        subprocess.call('gcc '+barsm.apps_loc+'zero_sec.c -o /opt/rc360/system/zero_sec_aacm', shell=True)
    else:
        subprocess.call('cp '+barsm.apps_loc+'non_exec.c /opt/rc360/system/', shell=True)

    # start barsm
    barsm.start_barsm()

    # launch AACM
    launch_result = barsm.monitor_launch()
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