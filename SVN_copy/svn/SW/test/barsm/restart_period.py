#!/usr/bin/env python3


# File:
#     restart_period.py
#
# Purpose:
#      This program will check the timing on the BARSM monitoring period. One thirty second
# application will be loaded and it will be verified that the app restarts have a
# period of 55 to 65 seconds. Error logs are also checked. A success is returned if apps start up
# as expected, the messages between BARSM and AACM are as expected, the app restarts have a
# period of 55 to 65 seconds.
#



import sys
import os.path
sys.path.append(os.path.join(os.path.dirname(__file__), '../utils'))

import log
import barsm_utils
import proc

import subprocess
import time

def run():
    result = False
    success = False

    # clear out all app directories
    barsm_utils.remove()

    # compile needed applications and place in required directories
    subprocess.call('gcc '+barsm_utils.apps_loc+'/infinite_sec.c -o /opt/rc360/system/aacm', shell=True)
    subprocess.call('gcc '+barsm_utils.apps_loc+'/thirty_sec.c -o /opt/rc360/modules/TPA/thirty_sec', shell=True)

    # navigate to the end of the SYSLOG
    follow = log.logfollower()

    # start barsm

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
        success = True

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

    # send sysInit message
    if success:
        print('-Waiting to receive OPEN message from BARSM')
        print('-Sending SYS_INIT message to BARSM')
        barsm_utils.sysInit(UDPsock)
        print('--OPEN received')

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

    # Default the time difference to a value that indicates failure
    dif = 0

    if success:
        #TODO: have a maximum time limit to look for the restart messages
        # capture the time of the first restart
        print("Waiting for first restart...")
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

        barsm_utils.receiveBarsmToAacm(TCPconn, expectedERR, expectedName)
        logs = follow.read('BARSM')
        restart1_ts = list(filter(lambda t: "Restarting" in t['message'], logs))[0]['timestamp']
        #print('restart1_ts: {}'.format(restart1_ts))

        #update the children list after the restart
        barsm_utils.populateChildren()

        # capture the time of the second restart
        print("Waiting for second restart...")
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

        barsm_utils.receiveBarsmToAacm(TCPconn, expectedERR, expectedName)
        logs = follow.read('BARSM')
        restart2_ts = list(filter(lambda t: "Restarting" in t['message'], logs))[0]['timestamp']
        #print('restart2_ts: {}'.format(restart2_ts))

        # compare timestamps
        dif = (restart2_ts - restart1_ts).total_seconds()
        #print('dif: {}'.format(dif))
        barsm_utils.populateChildren()

    # kill off all started processes
    barsm_utils.kill_procs()

    if success and 55 <= dif and 65 >= dif:
        result = True

    print('{}'.format('PASSED' if result else 'FAILED'))
    return result

if __name__ == '__main__':
    run()
