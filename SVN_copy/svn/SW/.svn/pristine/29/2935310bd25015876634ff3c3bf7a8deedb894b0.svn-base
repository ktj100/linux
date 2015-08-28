#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <poll.h>

#include "barsm_functions.h"

#define MAXBUFSIZE  1000

bool send_barsmToAacmInit(int32_t csocket)
{
    bool success = true;

    enum barsmToAacmInit_params
    {
        CMD_ID          = 2,
        LENGTH          = 2,
        MSG_SIZE        =   CMD_ID +
                            LENGTH,
    };

    uint8_t *ptr; 
    uint8_t *msgLenPtr = 0;
    uint32_t actualLength = 0; 
    uint16_t cmdId = CMD_BARSM_TO_AACM_INIT;
    uint8_t sendData[ MSG_SIZE ];
    int32_t sentBytes;


    ptr = sendData;
    memcpy(ptr, &cmdId, CMD_ID);
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    actualLength = MSG_SIZE - CMD_ID - LENGTH;
    memcpy(msgLenPtr, &actualLength, sizeof(actualLength));

    errno = 0;
    sentBytes = send(csocket, sendData, MSG_SIZE, 0);
    if ( -1 == sentBytes )
    {
        syslog(LOG_ERR, "%s:%d ERROR: send() failed! (%d: %s)", \
            __FUNCTION__, __LINE__, errno, strerror(errno));
        success = false;
    }
    else if ( MSG_SIZE != sentBytes )
    {
        syslog(LOG_ERR, "%s:%d ERROR: send() return value (%d) does not match MSG_SIZE!", \
            __FUNCTION__, __LINE__, sentBytes);
    }

    return (success);
}



bool recieve_barsmToAacmInitAck(int32_t csocket)
{
    enum barsmToAacmInitAck_params
    {
        CMD_ID          = 2,
        LENGTH          = 2,
        MSG_SIZE        =   CMD_ID +
                            LENGTH,
    };

    bool success = true;

    int32_t retBytes = 0;
    uint8_t retData[ MSG_SIZE ];
    uint8_t *ptr;
    uint32_t fullMsg = 0;

    struct pollfd myPoll[1];
    int32_t retPoll;

    myPoll[0].fd = csocket;
    myPoll[0].events = POLLIN; 

    errno = 0;
    retPoll = poll( myPoll, 1, -1 );
    if ( -1 == retPoll )
    {
        syslog(LOG_ERR, "%s:%d ERROR: poll() failure! (%d: %s)", \
            __FUNCTION__, __LINE__, errno, strerror(errno));
        success = false;
    }
    else if ( 0 == retPoll )
    {
        syslog(LOG_ERR, "%s:%d ERROR: poll() timeout!",__FUNCTION__, __LINE__);
        success = false;
    }
    else
    {
        if ( myPoll[0].revents && POLLIN )
        {
            retBytes = recv(csocket , retData , MSG_SIZE , 0 );
            ptr = retData;

            memcpy(&fullMsg, ptr, sizeof(fullMsg));
            if ( -1 == retBytes )
            {
                syslog(LOG_ERR, "%s:%d ERROR: recv() failure! (%d: %s)", \
                    __FUNCTION__, __LINE__, errno, strerror(errno));
                success = false;
            }
            else if ( 4 != retBytes || fullMsg != BARSM_TO_AACM_INIT_ACK_MSG )
            {
                syslog(LOG_ERR, "%s:%d ERROR: Message recieved was not expected ACK!", \
                    __FUNCTION__, __LINE__);
                success = false;
            }
        }
    }

    return (success);
}


// MODEL AFTER "process_registerApp_ack" in simm_functions.c
void process_aacmToBarsm( int32_t csocket )
{
    enum aacmToBarsm_params
    {
        CMD_ID                  = 2,
        LENGTH                  = 2,
        PID                     = 4,
        NUM_PROCESSES           = 2,
    }

    bool success = true;

    uint16_t command = 0;
    uint16_t actualLength = 0;
    uint32_t srcPid = 0;
    uint16_t numErrors = 0;
    uint32_t tmpPid = 0;

    myPoll[0].fd = csocket;
    myPoll[0].events = POLLIN; 

    // WAIT TO RECEIVE RESPONSE ...
    retPoll = poll( myPoll, 1, -1 );
    if ( retPoll == -1 ) 
    {
        syslog(LOG_ERR, "%s:%d ERROR! error from pollfd poll()",__FUNCTION__, __LINE__);
        //printf("ERROR WITH SELECT\n");
    }
    else if (retPoll == 0 ) 
    {
        syslog(LOG_ERR, "%s:%d ERROR! timeout error from pollfd poll()",__FUNCTION__, __LINE__);
        //printf("SELECT TIMEOUT ERROR\n");
    } 
    else
    {
        if (myPoll[0].revents & POLLIN)
        {
            retBytes = recv( csocket , retData , MAXBUFSIZE , 0 );

            ptr = retData;
            memcpy(&command, ptr, sizeof(command));
            ptr += CMD_ID;

            memcpy(&actualLength, ptr, sizeof(actualLength));
            ptr += LENGTH;

            memcpy(&srcPid, ptr, sizeof(srcPid));
            ptr += PID;
            restart_select(srcPid);

            memcpy(&numErrors, ptr, sizeof(numErrors));
            ptr += NUM_PROCESSES;

            while ( numErrors > 0 )
            {
                memcpy(&tmpPid, ptr, sizeof(tmpPid));
                ptr += PID;
                restart_select(tmpPid);
                numErrors--;
            }


}