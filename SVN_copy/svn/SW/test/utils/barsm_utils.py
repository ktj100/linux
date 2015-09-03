#!/usr/bin/env python3

# This file provides supporting classes/functions to all the BARSM Python scripts.

import sys
import os.path
sys.path.append(os.path.join(os.path.dirname(__file__), '../utils'))

import subprocess
import proc
import time
import datetime
import random
import shutil
import os
import socket
import multiprocessing
import struct

barsm_top_loc = os.path.join(os.path.dirname(__file__), '../../barsm')
barsm_loc = os.path.join(os.path.dirname(__file__), '../../barsm/src')
apps_loc = os.path.join(os.path.dirname(__file__), '../utils/dummies')


#TCP
TCP_IP                              = "127.0.0.1"   
TCP_PORT                            = 8000 

#COMMAND ID
CMD_BARSM_TO_AACM_INIT              = 16
CMD_BARSM_TO_AACM_INIT_ACK          = 17

#UNPACK STRING FORMATS (see enums from simm_functions.h)
BARSM_TO_AACM_INIT_FMT              = '=HH'
BARSM_TO_AACM_INIT_ACK_FMT          = '=HH'

def remove():
    # Ensure that there are instances of barsm (or valgrind) running
    kill_procs()

    shutil.rmtree('/opt/rc360', ignore_errors=True)
    for d in ['system', 'modules/GE', 'modules/TPA', 'apps/GE', 'apps/TPA']:
        os.makedirs('/opt/rc360/' + d)

def random_fail_start():
    random.seed()
    r = random.randint(1, 8)
    print(r)

    subprocess.call('gcc '+apps_loc+'/infinite_sec.c -o /opt/rc360/system/aacm', shell=True)

    if 1 == r:
        subprocess.call('gcc '+apps_loc+'/zero_sec.c -o /opt/rc360/modules/GE/gemod', shell=True)
    elif 5 == r:
        subprocess.call('cp '+apps_loc+'/non_exec.c /opt/rc360/modules/GE/', shell=True)
    else:
        subprocess.call('gcc '+apps_loc+'/infinite_sec.c -o /opt/rc360/modules/GE/gemod', shell=True)

    if 2 == r:
        subprocess.call('gcc '+apps_loc+'/zero_sec.c -o /opt/rc360/modules/TPA/tpamod', shell=True)
    elif 6 == r:
        subprocess.call('cp '+apps_loc+'/non_exec.c /opt/rc360/modules/TPA/', shell=True)
    else:
        subprocess.call('gcc '+apps_loc+'/infinite_sec.c -o /opt/rc360/modules/TPA/tpamod', shell=True)

    if 3 == r:
        subprocess.call('gcc '+apps_loc+'/zero_sec.c -o /opt/rc360/apps/GE/geapp', shell=True)
    elif 7 == r:
        subprocess.call('cp '+apps_loc+'/non_exec.c /opt/rc360/apps/GE/', shell=True)
    else:
        subprocess.call('gcc '+apps_loc+'/infinite_sec.c -o /opt/rc360/apps/GE/geapp', shell=True)

    if 4 == r:
        subprocess.call('gcc '+apps_loc+'/zero_sec.c -o /opt/rc360/apps/TPA/tpaapp', shell=True)
    elif 8 == r:
        subprocess.call('cp '+apps_loc+'/non_exec.c /opt/rc360/apps/TPA/', shell=True)
    else:
        subprocess.call('gcc '+apps_loc+'/infinite_sec.c -o /opt/rc360/apps/TPA/tpaapp', shell=True)


def start_barsm():
    # start BARSM
    test_output = open('test.out', 'w')

    subprocess.call('cd {}; make'.format(barsm_top_loc), shell=True)
    proc = subprocess.Popen('valgrind '+barsm_top_loc+'/build/barsm -v --read-var-info=yes --leak-check=full --track-origins=yes show-reachable=yes --malloc-fill=B5 --free-fill=4A', stdout=test_output, stderr=test_output, shell=True)
    assert proc



def aacm_setup():
    success = True
    # Get BARSM PID
    barsm_info = proc.findproc("valgrind.bin")
    barsm_pid = (barsm_info[0]['pid'])
    print('BARSM: {}'.format(barsm_info[0]['pid']))

    aacmStarted = False
    while False == aacmStarted:
        all_child = proc.findchild(barsm_pid)
        if 1 == len(all_child):
            aacmStarted = True

    TCPconn, sVal = TCPsetup()  # both return values are objects
    if success:
        success = barsmToAacmInit(TCPconn, sVal) 

    if success:
        barsmToAacmInitAck(TCPconn)

    return success



def TCPsetup():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        s.bind( (TCP_IP, TCP_PORT) )
    except msg:
        print( 'TCP Bind failed. Error code: ' + str(msg[0]) + 'Error message: ' + msg[1])
        conn.close()
        s.close()
        sys.exit()
    s.listen(1)
    (TCPconn, TCPaddr) = s.accept()
    print('DONE WITH TCP SETUP')
    return TCPconn, s



def barsmToAacmInit(TCPconn, sVal) :   
    success = True
    selectList = [sVal]
    print('GOT BARSM TO AACM INIT')
    btaInit = TCPconn.recv(1024)
    retBytes = len(btaInit);
    # print('bytes in Init: ' + str(retBytes))

    if 4 != retBytes:
        success = False
        print('RECEIVED B2A_INIT WAS NOT 4 BYTES!')
        
    if success:
        # print(struct.unpack(BARSM_TO_AACM_INIT_FMT, btaInit))
        print('BARSM TO AACM INIT RECEIVED SUCCESSFULLY!')

    return (success)


def barsmToAacmInitAck(TCPconn):
    barsmToAacmInitAckData = [CMD_BARSM_TO_AACM_INIT_ACK, 0]
    # print('Init ACK to send: ' + str(barsmToAacmInitAckData))

    TCPconn.send(struct.pack(BARSM_TO_AACM_INIT_ACK_FMT, barsmToAacmInitAckData[1], barsmToAacmInitAckData[0]))



# delay should be 25 if an app is expected to fail, otherwise use at least 5,
# although sometimes 5 is not quite enough for a normal launch depending on
# startup times, so 10 is the default delay.
def monitor_launch(delay=10):
    # Get BARSM PID
    barsm_info = proc.findproc("valgrind.bin")
    barsm_pid = (barsm_info[0]['pid'])
    print('BARSM: {}'.format(barsm_info[0]['pid']))

    run_time = 0
    child_amount = 0
    start_tries = 0
    start_time = datetime.datetime.now()

    while (10 * child_amount) + delay >= run_time and barsm_info:
        all_child = proc.findchild(barsm_pid)
        print(all_child)
        # prev_amount = child_amount
        child_amount = len(all_child)

        time.sleep(1)
        run_time = (datetime.datetime.now() - start_time).total_seconds()
        print(run_time)
        # m = (4 * child_amount) + 20
        # print(m)
        barsm_info = proc.findproc("valgrind.bin")

    print('Launch: {}'.format(child_amount))

    if not barsm_info and 0 == child_amount:
        # return -1 if aacm never launched and BARSM exited
        return(-1)
    else:
        return child_amount



def short_launch():
    subprocess.call('gcc '+apps_loc+'thirty_sec.c -o /opt/rc360/system/aacm', shell=True)


def norm_launch():

    # clear out all app directories
    remove()

    # place items in directories
    subprocess.call('gcc '+apps_loc+'/infinite_sec.c -o /opt/rc360/system/aacm', shell=True)
    subprocess.call('gcc '+apps_loc+'/infinite_sec.c -o /opt/rc360/modules/GE/gemod', shell=True)
    subprocess.call('gcc '+apps_loc+'/infinite_sec.c -o /opt/rc360/modules/TPA/tpamod', shell=True)
    subprocess.call('gcc '+apps_loc+'/infinite_sec.c -o /opt/rc360/apps/GE/geapp', shell=True)
    subprocess.call('gcc '+apps_loc+'/infinite_sec.c -o /opt/rc360/apps/TPA/tpaapp', shell=True)

    # start barsm
    start_barsm()


def launch_bad():
    # place items in directories
    subprocess.call('gcc '+apps_loc+'/thirty_sec.c -o /opt/rc360/system/thirty_aacm', shell=True)
    subprocess.call('gcc '+apps_loc+'/thirty_sec.c -o /opt/rc360/modules/GE/thirty_gemod', shell=True)
    subprocess.call('gcc '+apps_loc+'/thirty_sec.c -o /opt/rc360/modules/TPA/thirty_tpamod', shell=True)
    subprocess.call('gcc '+apps_loc+'/thirty_sec.c -o /opt/rc360/apps/GE/thirty_geapp', shell=True)
    subprocess.call('gcc '+apps_loc+'/thirty_sec.c -o /opt/rc360/apps/TPA/thirty_tpaapp', shell=True)
    subprocess.call('gcc '+apps_loc+'/infinite_sec.c -o /opt/rc360/modules/GE/gemod', shell=True)
    subprocess.call('gcc '+apps_loc+'/infinite_sec.c -o /opt/rc360/modules/TPA/tpamod', shell=True)
    subprocess.call('gcc '+apps_loc+'/infinite_sec.c -o /opt/rc360/apps/GE/geapp', shell=True)
    subprocess.call('gcc '+apps_loc+'/infinite_sec.c -o /opt/rc360/apps/TPA/tpaapp', shell=True)


def kill_procs():
    # find child PIDs
    for b in proc.findproc("valgrind.bin"):
        barsm_pid = b['pid']
        for c in proc.findchild(barsm_pid):
            child_pid = c['pid']
            subprocess.call('kill -15 '+child_pid, shell=True)

        subprocess.call('kill -15 '+barsm_pid, shell=True)
