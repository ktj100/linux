#!/usr/bin/env python

# This script uses an AACM that never ends, and one app/module in each directory.
# One directory is selected at random, in which an app/module is placed that ends
# immediately. A success is returned if the app/module is attempted to start five
# times, and then forgotten.


# add location of the utilities for this script to search out
import sys
sys.path.insert(0, '/svn/SW/test/utils')

import barsm
import log

if __name__ == '__main__':

    # navigate to the end of the SYSLOG
    follow = log.logfollower()

    # clear out all app directories
    barsm.remove()

    # place items in directories
    barsm.random_fail_start()

    # start barsm
    barsm.start_barsm()

    # launch apps and modules
    launch_result = barsm.monitor_launch()
    if launch_result != 1:
        print("FAIL")
    else:
    	# check logs to see if five errors were logged for each launch failure
        logs = follow.read()
        launch_errors = sum(1 for d in logs if "File launch failed!" in d.get('message') and "/opt/rc360/" in d.get('message'))

        if 5 == launch_errors:
            print("SUCCESS")
        else:
            print("FAIL")

    barsm.kill_procs()

    print("SCRIPT ENDED")

