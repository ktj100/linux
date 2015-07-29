#!/usr/bin/env python

# This program will check the timing on the BARSM monitoring period. One thirty second 
# application will be loaded and it will be verified that the app restarts have a
# period of 55 to 65 seconds.


# add location of the utilities for this script to search out
import sys
sys.path.insert(0, '/svn/SW/test/utils')

import log
import barsm
import subprocess
import time

if __name__ == '__main__':

    # navigate to the end of the SYSLOG
    follow = log.logfollower()

    # clear out all app directories
    barsm.remove()

    # compile needed applications and place in required directories
    subprocess.call('gcc '+barsm.apps_loc+'infinite_sec.c -o /opt/rc360/system/aacm', shell=True)
    subprocess.call('gcc '+barsm.apps_loc+'thirty_sec.c -o /opt/rc360/modules/TPA/thirty_sec', shell=True)

    # start barsm
    barsm.start_barsm()

    # launch processes
    launch_result = barsm.monitor_launch(5)
    if launch_result != 2:
        print("FAIL")
        sys.exit()

    # capture the time of the first restart
    restarts = 0
    while 1 > restarts:
        print("Waiting for first restart...")
        time.sleep(5)
        logs = follow.read()
        restarts = sum(1 for d in logs if "Restarting" in d.get('message'))

    restart1_ts = (filter(lambda t: "Restarting" in t['message'], logs))[0]['timestamp']

    # capture the time of the second restart
    restarts = 0
    while 1 > restarts:
        print("Waiting for second restart...")
        time.sleep(5)
        logs = follow.read()
        restarts = sum(1 for d in logs if "Restarting" in d.get('message'))

    restart2_ts = (filter(lambda t: "Restarting" in t['message'], logs))[0]['timestamp']

    # compare timestamps
    dif = (restart2_ts - restart1_ts).total_seconds()

    if 55 <= dif and 65 >= dif:
        print("SUCCESS")
    else:
        print("FAIL")

    # kill off all started processes
    barsm.kill_procs()
