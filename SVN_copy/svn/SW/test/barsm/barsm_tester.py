#!/usr/bin/env python3

import sys
sys.path.insert(0, '/home/norwood/sandbox/repos/trunk/SW/test/utils')
import simm
import log
import subprocess
import time
import threading

if __name__ == '__main__':
	barsmTestResult = True

    sysLogFollow = log.logfollower()
    lock = threading.Lock()

    #BOOT PROCESSES
    while barsmTestResult != False:
    	subprocess.Popen("clear")
        time.sleep(1)
        print("TEST STARTED!")
        sysLogFollow.start()

        KeepMoving = True
        if KeepMoving != False:
            # TCP SETUP
            TCPconn, sVal = simm.TCPsetup()
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                print("TCP SETUP errors: ", launch_errors)
                KeepMoving = False
            
        if KeepMoving != False:
            #UDP SETUP
            UDPsock = simm.UDPsetup()
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                print("UDP SETUP errors: ", launch_errors)
                KeepMoving = False
