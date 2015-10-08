#!/usr/bin/env python3

# File:
#     simm_regDataAck.py
#
# Purpose:
#     This program will test the simm for REGISTER DATA ACK ATTEMPT errors.
# Thirty six invalid attempts will be made, testing different parameters, and finally
# one valid message will be sent.
# A success is returned if no errors are detected.

import sys
import os.path
sys.path.append(os.path.join(os.path.dirname(__file__), '../utils'))

import simm_utils
import log
import subprocess
import time

# This is structured pretty much the same way as MyServer.c
def run():
    SimmTestResult      = True
    RunTimetoTest       = 180
    # commented values can be used as PASS values.  Set to 0 for full test.  
    regAppAck_passfail  = 3     # 3
    regDataAck_passfail = 0    # 36
    sysInit_passfail    = 2     # 2
    subscribe_passfail  = 76    # 76

    sysLogFollow = log.logfollower()

    cntPassFail = 0
    #testLoop = 0
    #BOOT PROCESSES

    # start the FPGA sim
    simm_utils.startFPGAsimult()

    while SimmTestResult != False:
        time.sleep(1)
        print("TEST STARTED!")
        print("PassFail Attempts: ", cntPassFail)
        #print('Test Attempt: ', testLoop)
        #testLoop = testLoop + 1
        sysLogFollow.start()
        #if False == KeepMoving
        #   close TCP and UDP connections?  so far, it's working without doing so ...

        KeepMoving = True
        if KeepMoving != False:
            #UDP SETUP
            UDPsock = simm_utils.UDPsetup()
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                print("UDP SETUP errors: ", launch_errors)
                KeepMoving = False

        if KeepMoving != False:
            # TCP SETUP
            TCPconn, sVal = simm_utils.TCPsetup()
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                print("TCP SETUP errors: ", launch_errors)
                KeepMoving = False

        if KeepMoving != False:
            # REGISTER APP
            #time.sleep(1)
            simm_utils.registerApp(TCPconn, sVal)
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
            simm_utils.registerAppAck(TCPconn, regAppAck_passfail)
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
            simm_utils.registerData(TCPconn)
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
            simm_utils.registerDataAck(TCPconn, regDataAck_passfail)
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
            simm_utils.udpOpen(UDPsock)
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
            simm_utils.sysInit(UDPsock, sysInit_passfail)
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
        # SUBSCRIBE
            print("SUBSCRIBE ATTEMPT: ", subscribe_passfail)
            # cosnider sending some random parameters as well ...
            if 2 == subscribe_passfail:
                simm_utils.subscribe(TCPconn, 0)
            elif 3 == subscribe_passfail:
                simm_utils.subscribe(TCPconn, 0)
            elif 4 == subscribe_passfail:
                simm_utils.subscribe(TCPconn, 0)
            elif 6 == subscribe_passfail:
                simm_utils.subscribe(TCPconn, 0)
            else:
                simm_utils.subscribe(TCPconn, subscribe_passfail)
            #time.sleep(1)
            #sysLog = sysLogFollow.read()
            #launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            #if 0 < launch_errors:
                #print("SUBSCRIBE errors: ", launch_errors)
                #KeepMoving = False

        #if KeepMoving != False:
        # SUBSCRIBE ACK
            #sysLogFollow.start 
            simm_utils.subscribeAck(TCPconn, subscribe_passfail)
            sysLog = sysLogFollow.read()
            sysLogFollow.start
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            #launch_success = sum(1 for d in sysLog if "simm_init() SUCCESS!" in d.get('message'))
            if 0 < launch_errors:
                sVal.close()
                print("SUBSCRIBE ACK errors: ", launch_errors)
                KeepMoving = False
                subscribe_passfail = subscribe_passfail + 1

        cntPassFail = cntPassFail + 1
        # RUN-TIME PROCESSING
        if KeepMoving != False:
            print("Total tests peformed: ", cntPassFail)

            simm_utils.start_threads()

            time.sleep(3)
            simm_utils.subscribe(TCPconn, 100)
            time.sleep(1)
            simm_utils.subscribeAck(TCPconn, 100)
            time.sleep(3)
            simm_utils.subscribe(TCPconn, 200)
            time.sleep(1)
            simm_utils.subscribeAck(TCPconn, 200)

               
            print(cntPassFail, "/37 tests passed")
            simm_utils.stop_simm()
            simm_utils.stop_fpga()
            return True

        # stop SIMM if it isn't stopped yet
        simm_utils.stop_simm()


if __name__ == '__main__':
    run()
