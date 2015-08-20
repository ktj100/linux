#!/usr/bin/env python3

import sys
sys.path.insert(0, '/home/norwood/sandbox/repos/trunk/SW/test/utils')
import simm
import log
import subprocess
import time
import threading

# This is structured pretty much the same way as MyServer.c
if __name__ == '__main__':
    SimmTestResult      = True

    # commented values can be used as PASS values.  Set to 0 for full test.  
    regAppAck_passfail  = 3     # 3
    regDataAck_passfail = 26    # 26
    sysInit_passfail    = 3     # 3
    subscribe_passfail  = 76    # 76

    sysLogFollow = log.logfollower()

    #consider using a stateMachine 
    while SimmTestResult != False:   
        subprocess.Popen("clear")
        time.sleep(1)
        print("TEST STARTED!")
        sysLogFollow.start()
        #if False == KeepMoving
        #   close TCP and UDP connections?  so far, it's working without doing so ...

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

        if KeepMoving != False:
            # REGISTER APP
            #time.sleep(1)
            simm.registerApp(TCPconn)
            print("REGISTER APP: FINISHED RECEIVING")
            #time.sleep(1)
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                print("REGISTER APP errors: ", launch_errors)
                KeepMoving = False

        if KeepMoving != False:
            # REGISTER APP ACK
            print("REGISTER APP ACK ATTEMPT: ", regAppAck_passfail)
            simm.registerAppAck(TCPconn, regAppAck_passfail)
            time.sleep(1)
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                print("REGISTER APP ACK errors: ", launch_errors)
                KeepMoving = False
                regAppAck_passfail = regAppAck_passfail + 1
                #time.sleep(1)

        if KeepMoving != False:
        # REGISTER DATA
            simm.registerData(TCPconn)
            print("READY TO GET REGISTER DATA")
            #time.sleep(1)
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                print("REGISTER DATA errors: ", launch_errors)
                KeepMoving = False

        if KeepMoving != False:
        # REGISTER DATA ACK
            print("REGISTER DATA ACK ATTEMPT: ", regDataAck_passfail)
            simm.registerDataAck(TCPconn, regDataAck_passfail)
            time.sleep(1)
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                print("REGISTER DATA ACK errors:",launch_errors)
                KeepMoving = False
                regDataAck_passfail = regDataAck_passfail + 1
                #time.sleep(1)

        if KeepMoving != False:
        # UDP OPEN 
        # If I don't put a large delay in my .c, this fails.  If the .c stuff executes before this starts, it fails.  
            #print("UDP OPEN")
            SenderAddr = simm.udpOpen(UDPsock)
            time.sleep(1)
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                print("UDP OPEN errors: ", launch_errors)
                KeepMoving = False

        if KeepMoving != False:
        # SYS INIT
            print("SYS INIT ATTEMPT: ", sysInit_passfail)
            simm.sysInit(UDPsock, SenderAddr, sysInit_passfail)
            time.sleep(1)
            sysLog = sysLogFollow.read()
            sysLogFollow.start()
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            if 0 < launch_errors:
                print("SYS INIT errors: ", launch_errors)
                KeepMoving = False
                sysInit_passfail = sysInit_passfail + 1

        if KeepMoving != False:
        # SUBSCRIBE
            print("SUBSCRIBE ATTEMPT: ", subscribe_passfail)
            # cosnider sending some random parameters as well ...
            if 2 == subscribe_passfail:
                simm.subscribe(TCPconn, 0)
            elif 3 == subscribe_passfail:
                simm.subscribe(TCPconn, 0)
            elif 4 == subscribe_passfail:
                simm.subscribe(TCPconn, 0)
            elif 6 == subscribe_passfail:
                simm.subscribe(TCPconn, 0)
            else:
                simm.subscribe(TCPconn, subscribe_passfail)
            #time.sleep(1)
            #sysLog = sysLogFollow.read()
            #launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            #if 0 < launch_errors:
                #print("SUBSCRIBE errors: ", launch_errors)
                #KeepMoving = False

        #if KeepMoving != False:
        # SUBSCRIBE ACK
            #sysLogFollow.start
            simm.subscribeAck(TCPconn, subscribe_passfail)
            sysLog = sysLogFollow.read()
            sysLogFollow.start
            launch_errors = sum(1 for d in sysLog if "ERROR!" in d.get('message'))
            #launch_success = sum(1 for d in sysLog if "simm_init() SUCCESS!" in d.get('message'))
            if 0 < launch_errors:
                print("SUBSCRIBE ACK errors: ", launch_errors)
                #KeepMoving = False
                subscribe_passfail = subscribe_passfail + 1

        if KeepMoving != False:
        # RUN-TIME
            #cnt = 0
            t = threading.Thread(group=None, target=simm.publishThread, name=None, args=(UDPsock,)).start()
            #t = threading.Thread(group=None, target=simm.HeartBeatThread, name=None, args=(TCPconn)).start()
            time.sleep(7)
            simm.subscribe(TCPconn, 100)
            time.sleep(1)
            simm.subscribeAck(TCPconn, 100)
            time.sleep(7)
            simm.subscribe(TCPconn, 200)
            time.sleep(1)
            simm.subscribeAck(TCPconn, 200)
            alwaysRun = True
            while True == alwaysRun:
                alwaysRun = True
