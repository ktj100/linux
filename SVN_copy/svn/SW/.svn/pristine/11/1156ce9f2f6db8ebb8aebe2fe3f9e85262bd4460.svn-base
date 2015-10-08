#!/usr/bin/env python3

import socket
import subprocess
import proc
import time
import datetime
import random
import struct

#TCP
TCP_IP = "127.0.0.1"   
TCP_PORT = 8000 
#
##UDP
#UDP_IP
#UDP_PORT

#COMMANDS
CMD_REGISTER_APP      = 1
CMD_REGISTER_APP_ACK  = 2
CMD_REGISTER_DATA     = 3
CMD_REGISTER_DATA_ACK = 4
CMD_SUBSCRIBE         = 5
CMD_SUBSCRIBE_ACK     = 6
CMD_HEARTBEAT         = 7
CMD_OPEN              = 8 
CMD_CLOSE             = 9 
CMD_PUBLISH           = 10
CMD_SYSINIT           = 11

#USED TO TRIGGER A FAIL IN SIMM
INVALID_CMD           = 90
ERROR_toFAIL          = 91
NUM_BYTES             = 92
TRIGGER_FAIL          = 93



#UNPACK FORMAT STRINGS (see enums from simm_functions.h)
REGISTER_APP_STR_FMT = '=HHBII'
REGISTER_APP_ACK_STR_FMT ='HHH'
regAppAckData = [CMD_REGISTER_APP_ACK, 2, 0]

REGISTER_DATA_STR_FMT = '=HHB'+23*'I' 
REGISTER_DATA_ACK_STR_FMT = 23*'H'
regDataAckErrors = [0] * 21
regDataAckData = [CMD_REGISTER_DATA_ACK, len(regDataAckErrors)*2] + regDataAckErrors


# or, use random numbers other than the pass values (usually 0)
#
#
#
#def UDPsetup():
#
#

def TCPsetup():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    print( 'Socket created')
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        s.bind( (TCP_IP, TCP_PORT) )
    except msg:
        print( 'Bind failed. Error code: ' + str(msg[0]) + 'Error message: ' + msg[1])
        conn.close()
        s.close()
        sys.exit()
    print('Socket bind complete')
    s.listen(1)
    print('Socket now listening')
    (TCPconn, TCPaddr) = s.accept()
    return TCPconn



#TCP side of testing
def TCPconnectRegisterApp_delay(delayTime):
    time.sleep(delayTime)



# RECEIVE FROM SIMM - should not be forced to fail
# check syslog for messages
def registerApp(TCPconn):
    #I need to poll ...
    regAppData = TCPconn.recv(1024)
    from struct import unpack
    print(unpack(REGISTER_APP_STR_FMT , regAppData))
    print('REGISTER APP DONE!')


def registerAppAck(TCPconn, passfail):
    if 0 == passfail:
        from struct import pack 
        TCPconn.send(pack(REGISTER_APP_ACK_STR_FMT, *regAppAckData))
        print('REGISTER APP ACK DONE!')
    else:
        print('REGISTER APP ACK FAIL!')

    # pass this a PASS or FAIL condition flag
    #if fail, systems logs message and exits
    # checks for:
    #   (1) bytes returned != MSG_SIZE
    #       setup to receive a calculated MSG_SIZE.  How does recv() handle retBytes > MSG_SIZE?  retBytes < MSG_SIZE, fails.
    #   (2) payload error value (just one error)
    #       err = 0, pass.  err != 0, fail
    #   (3) invalid command
    #   (4) payload length != calculated length
    #   (5) poll return val


## RECEIVE FROM SIMM - should not be forced to fail
## check syslog for messages
def registerData(TCPconn):
    #I need to poll ...
    regDataData = TCPconn.recv(1024)
    #print(regDataData)
    #stringData = regAppData.decode('utf-8')
    #print(stringData)
    from struct import unpack
    print(unpack(REGISTER_DATA_STR_FMT , regDataData))
    print('REGISTER DATA DONE!')
#nothing yet ...


def registerDataAck(TCPconn, passfail):
    if 0 == passfail:
        from struct import pack 
        TCPconn.send(pack(REGISTER_DATA_ACK_STR_FMT, *regDataAckData))
        print('REGISTER DATA ACK DONE!')
    else:
        print('REGISTER DATA ACK FAIL!')
# pass this a PASS or FAIL condition flag
#if fail, systems logs message and exits
# checks for:
#   (1) bytes returned != MSG_SIZE
#       setup to receive a calculated MSG_SIZE.  How does recv() handle retBytes > MSG_SIZE?  retBytes < MSG_SIZE, fails.
#   (2) payload error value (just one error)
#       err = 0, pass.  err != 0, fail
#   (3) invalid command
#   (4) payload length != calculated length
#   (5) poll return val

#def UDPopen():
##nothing yet ...
#
#def SysInitComplete(passfail):
## pass this a PASS or FAIL condition flag
##if fail, systems logs message and exits
## checks for:
##   (1) bytes returned != MSG_SIZE
##       setup to receive a calculated MSG_SIZE.  How does recv() handle retBytes > MSG_SIZE?  retBytes < MSG_SIZE, fails.
##   (2) invalid command
##   (3) payload length != calculated length
##   (4) poll return val