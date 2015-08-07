#!/usr/bin/env python3

import sys
sys.path.insert(0, '/home/norwood/sandbox/repos/trunk/SW/test/utils')
import simm
import log
import subprocess
import time

# This is structured pretty much the same way as MyServer.c
if __name__ == '__main__':

    # I'm thinking of looping through each function with a passfail option.  However, that would require
    #   restarting SIMM each time.  Moreover, there are about 4 functions to test each having about 4 tests.  
    #   Not sure if this is the best approach.  

    sysLogFollow = log.logfollower()

    # TCP SETUP
    sysLogFollow.start()
    TCPconn = simm.TCPsetup()
    sysLog = sysLogFollow.read()
    launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
    if 0 < launch_errors:
        print(launch_errors)
        sys.exit()
    

    #UDP SETUP
    sysLogFollow.start
    UDPsock = simm.UDPsetup()
    sysLog = sysLogFollow.read()
    launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
    if 0 < launch_errors:
        print(launch_errors)
        sys.exit()


    # REGISTER APP
    sysLogFollow.start
    time.sleep(1)
    simm.registerApp(TCPconn)
    time.sleep(1)
    sysLog = sysLogFollow.read()
    launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
    if 0 < launch_errors:
        print(launch_errors)
        sys.exit()


    # REGISTER APP ACK
    sysLogFollow.start
    simm.registerAppAck(TCPconn, 0)
    time.sleep(1)
    sysLog = sysLogFollow.read()
    launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
    if 0 < launch_errors:
        print(launch_errors)
        sys.exit()


    # REGISTER DATA
    sysLogFollow.start
    simm.registerData(TCPconn)
    time.sleep(1)
    sysLog = sysLogFollow.read()
    launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
    if 0 < launch_errors:
        print(launch_errors)
        sys.exit()


    # REGISTER DATA ACK
    sysLogFollow.start
    simm.registerDataAck(TCPconn, 0)
    time.sleep(1)
    sysLog = sysLogFollow.read()
    launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
    if 0 < launch_errors:
        print(launch_errors)
        sys.exit()


    # UDP OPEN 
    # If I don't put a large delay in my .c, this fails.  If the .c stuff executes before this starts, it fails.  
    sysLogFollow.start
    SenderAddr = simm.udpOpen(UDPsock)
    time.sleep(3)
    sysLog = sysLogFollow.read()
    launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
    if 0 < launch_errors:
        print(launch_errors)
        sys.exit()


    # SYS INIT
    sysLogFollow.start
    simm.sysInit(UDPsock, SenderAddr, 0)
    time.sleep(1)
    sysLog = sysLogFollow.read()
    launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
    if 0 < launch_errors:
        print(launch_errors)
        sys.exit()


    # SUBSCRIBE
    # incorporate other SUBSCRIBEs during run-time and see how published data changes
    sysLogFollow.start
    simm.subscribe(TCPconn, 0)
    time.sleep(1)
    sysLog = sysLogFollow.read()
    launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
    if 0 < launch_errors:
        print(launch_errors)
        sys.exit()


    # SUBSCRIBE ACK
    sysLogFollow.start
    simm.subscribeAck(TCPconn)
    sysLog = sysLogFollow.read()
    launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
    if 0 < launch_errors:
        print(launch_errors)
        sys.exit()

    # RUN-TIME
    while 1:
        simm.publish(UDPsock)
        

