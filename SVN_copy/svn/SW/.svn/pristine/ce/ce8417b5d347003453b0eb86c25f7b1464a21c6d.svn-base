#!/usr/bin/env python3

# File:
#    fdl_regAppAck.py
#
# Purpose:
#     This program will test the fdl for REGISTER APP ACK ATTEMPT errors.
# Three invalid attempts will be made, testing different parameters, and finally
# one valid message will be sent.
# A success is returned if no errors are detected.

import sys
import os.path
sys.path.append(os.path.join(os.path.dirname(__file__), '../utils'))

import fdl_utils
import log
import subprocess
import time
import threading

def run():
    RunTimetoTest   = 180
    fdlTestResult   = True

    # commented values can be used as PASS values.  Set to 0 for full test.  
    regAppAck_passfail  = 0     # 3
    regDataAck_passfail = 26    # 26
    sysInit_passfail    = 2     # 2

    sysLogFollow = log.logfollower()

    cntPassFail = 0
    #BOOT PROCESSES
    while fdlTestResult != False:   
        time.sleep(1)
        print("TEST STARTED!")
        print("PassFail Attempts: ", cntPassFail)
        sysLogFollow.start()
        #if False == KeepMoving
        #   close TCP and UDP connections?  so far, it's working without doing so ...

        KeepMoving = True
        if KeepMoving != False:
            #UDP SETUP
            UDPsock = fdl_utils.UDPsetup()
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                print("UDP SETUP errors: ", launch_errors)
                KeepMoving = False
        
        if KeepMoving != False:
            # TCP SETUP
            TCPconn, sVal = fdl_utils.TCPsetup()
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                print("TCP SETUP errors: ", launch_errors)
                KeepMoving = False
            

        if KeepMoving != False:
            # REGISTER APP
            #time.sleep(1)
            fdl_utils.registerApp(TCPconn, sVal)
            print("REGISTER APP: FINISHED RECEIVING")
            #time.sleep(1)
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                sVal.close()
                print("REGISTER APP errors: ", launch_errors)
                KeepMoving = False

        if KeepMoving != False:
            # REGISTER APP ACK
            print("REGISTER APP ACK ATTEMPT: ", regAppAck_passfail)
            fdl_utils.registerAppAck(TCPconn, regAppAck_passfail)
            time.sleep(1)
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                sVal.close()
                print("REGISTER APP ACK errors: ", launch_errors)
                KeepMoving = False
                regAppAck_passfail = regAppAck_passfail + 1
                #time.sleep(1)

        if KeepMoving != False:
        # REGISTER DATA
            fdl_utils.registerData(TCPconn)
            print("READY TO GET REGISTER DATA")
            #time.sleep(1)
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                sVal.close()
                print("REGISTER DATA errors: ", launch_errors)
                KeepMoving = False

        if KeepMoving != False:
        # REGISTER DATA ACK
            print("REGISTER DATA ACK ATTEMPT: ", regDataAck_passfail)
            fdl_utils.registerDataAck(TCPconn, regDataAck_passfail)
            time.sleep(1)
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                sVal.close()
                print("REGISTER DATA ACK errors:",launch_errors)
                KeepMoving = False
                regDataAck_passfail = regDataAck_passfail + 1
                #time.sleep(1)

        if KeepMoving != False:
        # UDP OPEN 
        # If I don't put a large delay in my .c, this fails.  If the .c stuff executes before this starts, it fails.  
            print("UDP OPEN")
            fdl_utils.udpOpen(UDPsock)
            time.sleep(1)
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                sVal.close()
                print("UDP OPEN errors: ", launch_errors)
                KeepMoving = False

        if KeepMoving != False:
        # SYS INIT
            print("SYS INIT ATTEMPT: ", sysInit_passfail)
            time.sleep(1)
            fdl_utils.sysInit(UDPsock, sysInit_passfail)
            time.sleep(1)
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                sVal.close()
                print("SYS INIT errors: ", launch_errors)
                KeepMoving = False
                sysInit_passfail = sysInit_passfail + 1

        if KeepMoving != False:
        # GET SUBSCRIBE
            print("GET SUBSCRIBE ATTEMPT")
            fdl_utils.getSubscribe(TCPconn)
            time.sleep(1)
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                print("SYS INIT errors: ", launch_errors)
                KeepMoving = False
                sysInit_passfail = sysInit_passfail + 1

        if KeepMoving != False:
        # SEND SUBSCRIBE ACK
            print("SEND SUB ACK ATTEMPT")
            fdl_utils.sendSubscribeAck(TCPconn)
            time.sleep(1)
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                print("SEND SUB ACK errors:",launch_errors)
                KeepMoving = False
                regDataAck_passfail = regDataAck_passfail + 1

        if KeepMoving != False:
        # SEND PUBLISH DATA
            print("SEND PUBLISH DATA")
            fdl_utils.sendPublish(UDPsock)
            time.sleep(1)
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                print("SEND PUBLISH DATA errors: ", launch_errors)
                KeepMoving = False
                sysInit_passfail = sysInit_passfail + 1
        cntPassFail = cntPassFail + 1
        #RUN-TIME PROCESSING
        if KeepMoving != False:
            print("Total tests peformed: ", cntPassFail)
            print(cntPassFail, "/4 tests passed")
            fdl_utils.stop_fdl()
            return True

        fdl_utils.stop_fdl()

# This is structured pretty much the same way as MyServer.c
if __name__ == '__main__':
    run()
