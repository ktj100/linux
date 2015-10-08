#!/usr/bin/env python

# ls



# add location of the utilities for this script to search out
import sys
sys.path.insert(0, '/svn/SW/test/utils')

import log
import barsm
# import subprocess
import time
import datetime
import proc

if __name__ == '__main__':

    # navigate to the end of the SYSLOG
    follow = log.logfollower()

    # clear out all app directories
    barsm.remove()

    # compile needed applications and place in required directories
    # barsm.short_launch()
    barsm.launch_bad()
#     subprocess.call('gcc '+barsm.apps_loc+'infinite_sec.c -o /opt/rc360/system/aacm', shell=True)
#     subprocess.call('gcc '+barsm.apps_loc+'thirty_sec.c -o /opt/rc360/modules/TPA/thirty_sec', shell=True)

    # start barsm
    barsm.start_barsm()

    # launch processes
    launch_result = barsm.monitor_launch(5)
    # if launch_result != 9:
    #     print("Launch Failure")
    #     print("FAIL")
    #     # kill off all started processes
    #     barsm.kill_procs()
    #     sys.exit()

    # get the time of the launch finish
    logs = follow.read()
    launch_finish_ts = ((filter(lambda t: "COMPLETED: Launch sequence complete!" in t['message'], logs))[0]['timestamp'])
    # timestamps from SYSLOG don't incule a year, and it defaults to 1900.. So 42003 days are added to bring it to 2015
    launch_finish_ts = launch_finish_ts + datetime.timedelta(days = 42003)

    # wait for 35 seconds to pass to check for 5 zombies
    while 35 > (datetime.datetime.now() - launch_finish_ts).total_seconds():
        print("Waiting 35 seconds after launch for children to become zombies...")
        time.sleep(3)

    # make sure all 5 thirty second apps ended and none others
    barsm_pid = (proc.findproc("valgrind.bin"))[0]['pid']
    child_pids = proc.findchild(barsm_pid)
    print(child_pids)
    zombies = sum(1 for c in child_pids if "Z" == c.get('state'))
    print(zombies)
    # if 5 != zombies:
    #     print("Zombie Creation Failure")
    #     print("FAIL")        
    #     # kill off all started processes
    #     barsm.kill_procs()
    #     sys.exit()

    # wait for 65 seconds to pass to check for child recreation
    while 65 > (datetime.datetime.now() - launch_finish_ts).total_seconds():
        print("Waiting 65 seconds after launch for children to be replaced...")
        time.sleep(3)

    # make sure all apps get started back up
    child_pids = proc.findchild(barsm_pid)
    print(child_pids)
    zombies = sum(1 for c in child_pids if "Z" == c.get('state'))
    print(zombies)
    if 0 == zombies:
        print("SUCCESS")
    else:
        print("FAIL")

    # kill off all started processes
    barsm.kill_procs()
