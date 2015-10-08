#!/usr/bin/env python3

import sys
sys.path.insert(0, '/home/norwood/sandbox/repos/trunk/SW/test/utils')

import simm
import log
import subprocess
import time

if __name__ == '__main__':

    TCPconn = simm.TCPsetup()
    time.sleep(3)
    simm.registerApp(TCPconn)
    time.sleep(3)
    simm.registerAppAck(TCPconn, 0)
    time.sleep(3)
    simm.registerData(TCPconn)
    time.sleep(3)
    simm.registerDataAck(TCPconn, 0)
