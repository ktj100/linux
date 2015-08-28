#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <poll.h>

#include "barsm_functions.h"

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
    uint8_t sendData[ MSG_SIZE ];
    int32_t sentBytes;

    ptr = sendData;
    memcpy(ptr, &CMD_BARSM_TO_AACM_INIT, CMD_ID);
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
    uint8_t retData[ MAXBUFSIZE ];

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
            retBytes = recv(csocket , retData , MAXBUFSIZE , 0 );
            if ( -1 == retBytes )
            {
                syslog(LOG_ERR, "%s:%d ERROR: recv() failure! (%d: %s)", \
                    __FUNCTION__, __LINE__, errno, strerror(errno));
                success = false;
            }
            else if ( 4 != retBytes || retData != BARSM_TO_AACM_INIT_ACK_MSG )
            {
                syslog(LOG_ERR, "%s:%d ERROR: Message recieved was not expected ACK!", \
                    __FUNCTION__, __LINE__);
                success = false;
            }
        }
    }

    return (success);
}