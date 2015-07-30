#!/usr/bin/env python3

# This script tests whether the applications/modules are launched in the correct order.

import sys
import os.path
sys.path.append(os.path.join(os.path.dirname(__file__), '../utils'))
import barsm_utils
import log

def run():
    result = False

    # navigate to the end of the SYSLOG
    follow = log.logfollower()

    # execute noraml actions
    barsm_utils.norm_launch()

    # launch processes
    launch_result = barsm_utils.monitor_launch()

    # check logs for launch errors
    logs = follow.read('barsm')
    print(logs)

    # kill off all started processes
    barsm_utils.kill_procs()

    launch_errors = sum(1 for d in logs if "File launch failed!" in d.get('message'))

    if launch_result == 5 and launch_errors == 0:
        # store all timestamps
        aacm_ts =   list(filter(lambda t: "/opt/rc360/system/" in t['message'], logs))[0]['timestamp']
        gemod_ts =  list(filter(lambda t: "/opt/rc360/modules/GE/" in t['message'], logs))[0]['timestamp']
        tpamod_ts = list(filter(lambda t: "/opt/rc360/modules/TPA/" in t['message'], logs))[0]['timestamp']
        geapp_ts =  list(filter(lambda t: "/opt/rc360/apps/GE/" in t['message'], logs))[0]['timestamp']
        tpaapp_ts = list(filter(lambda t: "/opt/rc360/apps/TPA/" in t['message'], logs))[0]['timestamp']

        # Ensure that 5 apps were launched, there were no errors, and that the apps
        # were launched in the correct order. 
        if gemod_ts  > aacm_ts   and \
           tpamod_ts > aacm_ts   and \
           geapp_ts  > gemod_ts  and \
           tpaapp_ts > gemod_ts  and \
           geapp_ts  > tpamod_ts and \
           tpaapp_ts > tpamod_ts:
            result = True

    print('{}'.format('PASSED' if result else 'FAILED'))
    return result

if __name__ == '__main__':
    run()
