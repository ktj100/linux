#!/usr/bin/env python3

# File:
#     message_testing.py
#
# Purpose:
#     This script ensures that BARSM restarts any successed applications that started
# up correctly. Error logs are also checked. A success is returned if apps start up
# as expected, the messages between BARSM and AACM are as expected, the apps
# expected to restart are restarted.

import sys
import os.path
sys.path.append(os.path.join(os.path.dirname(__file__), '../utils'))

import log
import barsm_utils

import time
import datetime
import proc
import subprocess

barsm_top_loc = os.path.join(os.path.dirname(__file__), '../../barsm')
barsm_loc = os.path.join(os.path.dirname(__file__), '../../barsm/src')
apps_loc = os.path.join(os.path.dirname(__file__), '../utils/dummies')

def run():
    success = True
    # navigate to the end of the SYSLOG
    follow = log.logfollower()

    # clear out all app directories
    barsm_utils.remove()

    # add apps in directories
    subprocess.call('gcc '+apps_loc+'/infinite_sec.c -o /opt/rc360/system/aacm', shell=True)
    subprocess.call('gcc '+apps_loc+'/hundred_sec.c -o /opt/rc360/modules/GE/gemod', shell=True)

    # start barsm
    if success:
        print('-Starting BARSM')
        barsm_utils.start_barsm()
        logs = follow.read('BARSM')
        errors = sum(1 for d in logs if "ERROR" in d.get('message'))
        if 0 < errors:
            print('ERROR: SYSLOG errors while starting BARSM!')
            for d in logs:
                if "ERROR" in d.get('message'):
                    print (d.get('message'))
            success = False
        else:
            print('--BARSM started')

    # wait for AACM to start
    if success:
        print('-Waiting for AACM')
        barsm_pid = (proc.findproc("valgrind.bin"))[0]['pid']
        aacmStarted = False
        while False == aacmStarted and success:
            # check if AACM has started
            all_child = proc.findchild(barsm_pid)
            if 1 == len(all_child):
                aacmStarted = True
                aacm_pid = int(all_child[0]['pid'])
            # check for launch errors
            logs = follow.read('BARSM')
            errors = sum(1 for d in logs if "ERROR" in d.get('message'))
            if 0 < errors:
                print('ERROR: SYSLOG errors while waiting for AACM startup!')
                for d in logs:
                    if "ERROR" in d.get('message'):
                        print (d.get('message'))
                success = False
        if success:
            print('--AACM started')

    # setup TCP connection
    if success:
        print('-Setting up TCP connection')
        TCPconn, sVal = barsm_utils.TCPsetup()  # both return values are objects
        logs = follow.read('BARSM')
        errors = sum(1 for d in logs if "ERROR" in d.get('message'))
        if 0 < errors:
            print('ERROR: SYSLOG errors while initiating TCP connection!')
            for d in logs:
                if "ERROR" in d.get('message'):
                    print (d.get('message'))
            success = False
        else:
            print('--TCP connection setup successful')

    # receive BARSM to AACM init
    if success:
        print('-Waiting for BARSM to AACM init')
        success = barsm_utils.barsmToAacmInit(TCPconn)
        logs = follow.read('BARSM')
        errors = sum(1 for d in logs if "ERROR" in d.get('message'))
        if 0 < errors:
            print('ERROR: SYSLOG errors while receiving BARSM to AACM init!')
            for d in logs:
                if "ERROR" in d.get('message'):
                    print (d.get('message'))
            success = False
        else:
            print('--Init received')

    # send BARSM to AACM init ack
    if success:
        print('-Sending ACK')
        barsm_utils.barsmToAacmInitAck(TCPconn)
        logs = follow.read('BARSM')
        errors = sum(1 for d in logs if "ERROR" in d.get('message'))
        if 0 < errors:
            print('ERROR: SYSLOG errors while sending BARSM to AACM init ack!')
            for d in logs:
                if "ERROR" in d.get('message'):
                    print (d.get('message'))
            success = False
        else:
            print('--ACK sent')

    # wait for children to complete launching
    if success:
        print('-Waiting for launch completion')
        children, nameList = barsm_utils.waitForLaunch(follow)
        if 2 == children:
            print('--Launch complete!')
        else:
            print('ERROR: only {} child procs (should have 2)'.format(children))
            success = False

    # setup UDP connection
    if success:
        print('-Setting up UDP connection')
        UDPsock = barsm_utils.UDPsetup()
        logs = follow.read('BARSM')
        errors = sum(1 for d in logs if "ERROR" in d.get('message'))
        if 0 < errors:
            print('ERROR: SYSLOG errors while initiating UDP connection!')
            for d in logs:
                if "ERROR" in d.get('message'):
                    print (d.get('message'))
            success = False
        else:
            print('--UDP connection setup')

    # receive OPEN message
    if 0:#success:
        print('-Waiting to receive OPEN message from BARSM')
        SenderAddr = barsm_utils.udpOpen(UDPsock)
        logs = follow.read('BARSM')
        errors = sum(1 for d in logs if "ERROR" in d.get('message'))
        if 0 < errors:
            print('ERROR: SYSLOG errors while opening UDP connection!')
            for d in logs:
                if "ERROR" in d.get('message'):
                    print (d.get('message'))
            success = False
        else:
            print('--OPEN received')

    # send sysInit message
    if success:
        print('-Sending SYS_INIT message to BARSM')
        barsm_utils.sysInit(UDPsock)

    # receive barsm to aacm processes
    if success:
        print('-Receiving BARSM_TO_AACM_PROCESSES')

        expectedData = {}
        expectedData['numMsgBytes']  = 42
        expectedData['numMsgDataBytes']  = 38

        # expect number of processes + 1 for BARSM + 1 for AACM
        expectedData['unpackFMT'] = '=HHH' + 'I4sI' * (len(nameList) + 2)
        expectedData['numProcesses'] = len(nameList) + 2

        barsm_utils.barsmToAacmProcesses(TCPconn, expectedData, aacm_pid, nameList, follow)
        print('--BARSM_TO_AACM_PROCESSES message received')

    # Send for AACM_TO_BARSM_MSG
    if success:
        print('-Sending AACM_TO_BARSM_MSG')

        #send AACM_TO_BARSM_MSG
        appsToRestart = barsm_utils.aacmToBarsmMsg(TCPconn)
        logs = follow.read('BARSM')
        errors = sum(1 for d in logs if "ERROR" in d.get('message'))
        if 0 < errors:
            print('ERROR: SYSLOG errors while sending AACM_TO_BARSM_MSG!')
            for d in logs:
                if "ERROR" in d.get('message'):
                    print (d.get('message'))
            success = False
        if success:
            print('--AACM_TO_BARSM_MSG sent')

    # get AACM_TO_BARSM_MSG ack
    if success:
        print('-Receiving AACM_TO_BARSM_MSG ACK')
        msgNotReceived = True
        while True == msgNotReceived and success:
            expectedERR = 12
            msgNotReceived = barsm_utils.aacmToBarsmAck(TCPconn, expectedERR)
            logs = follow.read('BARSM')
            errors = sum(1 for d in logs if "ERROR" in d.get('message'))
            if 0 < errors:
                print('--ERROR: SYSLOG errors while receiving BARSM to AACM Msg ack!')
                for d in logs:
                    if "ERROR" in d.get('message'):
                        print (d.get('message'))
                success = False
        if success:
            print('--AACM_TO_BARSM_MSG ACK received')

    # verify restart of App(s)
    if success:
        print('-Checking for successful app restart(s)')
        success = barsm_utils.checkForRestart(appsToRestart)
        logs = follow.read('BARSM')
        errors = sum(1 for d in logs if "ERROR" in d.get('message'))
        if 0 < errors:
            print('--ERROR: SYSLOG error, App(s) not restarted!')
            for d in logs:
                if "ERROR" in d.get('message'):
                    print (d.get('message'))
            success = False
        if success:
            print('--App(s) restarted successfully')
            barsm_utils.populateChildren()
        else:
            print('--App(s) restart failure')

    if success:
        print('-Checking BARSM_TO_AACM_MSG')
        msgNotReceived = True
        if True == msgNotReceived and success:
            print('--Receiving BARSM_TO_AACM_MSG')
            expectedERR = 11

            appName = log.logfollower()
            names = []
            expectedName = ''

            while not names and expectedName == '':
                names = appName.read('BARSM')
                try:
                    expectedName = list(filter(lambda t: "DEBUG: Message sent with APP name:" in t['message'], names))[0]['message']
                except:
                    expectedName = ''
                    names = []

            expectedName = expectedName[-4:]

            msgNotReceived, failedPid = barsm_utils.receiveBarsmToAacm(TCPconn, expectedERR, expectedName)
            success = not msgNotReceived
            logs = follow.read('BARSM')
            errors = sum(1 for d in logs if "ERROR" in d.get('message'))
            #all of the errors less the one we are expecting
            errors = errors - sum(1 for d in logs if "ERROR: Process for /opt/rc360/modules/GE/gemod" in d.get('message'))
            if 0 < errors:
                print('--Error Receiving BARSM_TO_AACM_MSG')
        if success:
            print('--BARSM_TO_AACM_MSG received')

    #send ack back to barsm
    if success:
        appsToRestart = []
        restartData   = {}
        restartData['pid']  = failedPid
        restartData['name']  = list(filter(lambda t: "NOTICE: Restarting " in t['message'], logs))[-1]['message'].split(' ')[-1][:-3]
        appsToRestart.append(restartData)

        barsm_utils.sendBarsmToAacmAck(TCPconn, failedPid)
        logs = follow.read('BARSM')
        errors = sum(1 for d in logs if "ERROR" in d.get('message'))
        if 0 < errors:
            print('--ERROR: Sending ACK To BARSM!')
            for d in logs:
                if "ERROR" in d.get('message'):
                    print (d.get('message'))
            success = False

    if success:
        print('Checking for BARSM_TO_AACM_MSG process restart')
        success = barsm_utils.checkForRestart(appsToRestart)

        logs = follow.read('BARSM')
        errors = sum(1 for d in logs if "ERROR" in d.get('message'))
        if 0 < errors:
            print('--ERROR: SYSLOG error, App(s) not restarted!')
            for d in logs:
                if "ERROR" in d.get('message'):
                    print (d.get('message'))
            success = False
        if success:
            print('--BARSM_TO_AACM_MSG App(s) restarted successfully')
            barsm_utils.populateChildren()
        else:
            print('--BARSM_TO_AACM_MSG App(s) restart failure')

    # kill off all started processes
    barsm_utils.kill_procs()

    print('{}'.format('PASSED' if success else 'FAILED'))
    return success

if __name__ == '__main__':
    run()
