#!/usr/bin/env python3

import sys
import socket
import select
import subprocess
import proc
import time
import datetime
import random
import struct
import os
import multiprocessing


def TCPsetup():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    #s.setblocking(True)
    #print( 'TCP Socket created')
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        s.bind( (TCP_IP, TCP_PORT) )
    except msg:
        print( 'TCP Bind failed. Error code: ' + str(msg[0]) + 'Error message: ' + msg[1])
        conn.close()
        s.close()
        sys.exit()
    #print('TCP Socket bind complete')
    s.listen(1)
    #print('TCP Socket now listening')
    #p = multiprocessing.Process(target=startFPGAsimult)
    #p.start()
    p = multiprocessing.Process(target=startSimm)
    p.start()
    (TCPconn, TCPaddr) = s.accept()
    print('DONE WITH TCP SETUP')
    return TCPconn, s

def UDPsetup():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    #print( 'UDP Socket created')
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    return s
