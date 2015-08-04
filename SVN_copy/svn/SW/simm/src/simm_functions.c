#include <sys/types.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <dirent.h>
#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <stdint.h>
#include "simm_functions.h"

#define MAXBUFSIZE  1000

extern int32_t clientSocket;
struct sockaddr_storage serverStorage_UDP;

struct sockaddr_storage toRcvUDP;
struct sockaddr_storage toSendUDP;

//typedef struct
//{
//    uint32_t mp;
//    uint32_t period;
//    uint32_t numSamples;
//    bool logical;
//    bool valid;
//} MPinfo;
//
//MPinfo *subscribeMP;
//MPinfo *subscribeMP = (MPinfo*) malloc(sizeof(MPinfo)* 1);




//
void process_registerApp( int32_t csocket , struct timespec goTime )
{
    // send at boot
    enum registerApp_params
    {
        CMD_ID                  = 2,
        LENGTH                  = 2,
        HOST_OS                 = 1,
        SRC_PROC_ID             = 4,
        SRC_APP_NAME            = 4,
        MSG_SIZE                =   CMD_ID + 
                                    LENGTH + 
                                    HOST_OS + 
                                    SRC_PROC_ID +
                                    SRC_APP_NAME,
    };

    bool lessOneSecond = true;
    int32_t sendBytes = 0;
    uint8_t *ptr , *msgLenPtr;
    uint32_t actualLength, secToChk;
    uint8_t sendData[ MSG_SIZE ];
    //uint8_t sendData[1];
    uint32_t totalTime, finishTime, startTime, i;
    struct timespec endTime;
    
//  for (i = 0 ; i < 13 ; i++ )
//  {
//      sendData[i] = 1;
//  }

//    sendData[0] = 1;

    ptr = sendData;
    remote_set16( ptr , CMD_REGISTER_APP);
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    *ptr = PETALINUX;
    ptr += HOST_OS;

    //getpid()
    remote_set32( ptr , getpid() );
    ptr += SRC_PROC_ID;

    remote_set32( ptr , 0);
    ptr += SRC_APP_NAME;

    *ptr = MSG_SIZE;
    ptr += MSG_SIZE;

    secToChk = 1;

    // actual length
    actualLength = MSG_SIZE - CMD_ID - LENGTH;
    memcpy(msgLenPtr, &actualLength, sizeof(actualLength));

    // used to check "within 1 second" requirement between server connect and REGISTER_APP
    clock_gettime(CLOCK_REALTIME, &endTime);

    if (true == check_elapsedTime( goTime , endTime , secToChk ) )
    {
        sendBytes = send(csocket, sendData, MSG_SIZE, 0);
    }
    else
    {
        syslog(LOG_ERR, "%s:%d failed to send REGISTER_APP to AACM within one second of connecting", __FUNCTION__, __LINE__);
        printf("ELAPSED TIME GREATER THAN 1 SECOND!\n");
    }

    if (MSG_SIZE != sendBytes)
    {
        syslog(LOG_ERR, "%s:%d insufficient message data %u != %u",__FUNCTION__, __LINE__, sendBytes, MSG_SIZE);
        printf("REGISTER_APP ERROR \n");
    }
    else
    {
        printf("REGISTER_APP TRUE \n");
    }
    printf("REGISTER_APP SIZE: %d\n", sendBytes);
}


bool process_registerApp_ack(int32_t csocket )
{
    enum registerApp_ack_params
    {
        CMD_ID                  = 2,
        LENGTH                  = 2,
        ERROR                   = 2,
        MSG_SIZE                =   CMD_ID + 
                                    LENGTH + 
                                    ERROR,
    };

    bool success = true;

    uint16_t command;

    uint32_t retBytes, actualLength;
    uint8_t retData[ MSG_SIZE ];    // or whatever max is defined
    uint16_t appAckErr;
    uint8_t *ptr;
    int32_t calcLength = 0;

    struct pollfd myPoll[1];
    myPoll[0].fd = csocket;
    myPoll[0].events = POLLIN; 

    int32_t retPoll;
    struct timeval tv;

    // add timeout of ? 3 seconds here
    tv.tv_sec = 3;
    tv.tv_usec = 0;

    // WAIT TO RECEIVE RESPONSE ...
    retPoll = poll( myPoll, 1, -1 );
    if ( retPoll == -1 ) 
    {
        printf("ERROR WITH SELECT\n");
    }
    else if (retPoll == 0 ) 
    {
        printf("SELECT TIMEOUT ERROR\n");
    } 
    else
    {
        if (myPoll[0].revents & POLLIN)
        {
            retBytes = recv(csocket, retData , MSG_SIZE , 0 );

            command = remote_get16(retData);
            ptr = retData;
            ptr += CMD_ID;

            actualLength = remote_get16(ptr);
            ptr += LENGTH;

            appAckErr = remote_get16(ptr);
            ptr += ERROR;

            calcLength = MSG_SIZE - CMD_ID - LENGTH;
        }
    }

    if ( (retPoll <= 0) || (retBytes != MSG_SIZE) || ( appAckErr != 0 ) || (CMD_REGISTER_APP_ACK != command) || (calcLength != actualLength) )
    {
        success = false;
        printf("REGISTER APP ACK ERROR \n");
        if ( (retBytes != MSG_SIZE) )
        {
            syslog(LOG_ERR, "%s:%d insufficient message data %u != %u",__FUNCTION__, __LINE__, retBytes, MSG_SIZE);
        }
        if ( ( appAckErr != 0 ) ) 
        {
            syslog(LOG_ERR, "%s:%d payload error received %u ",__FUNCTION__, __LINE__, appAckErr);

        } 
        if ( (CMD_REGISTER_APP_ACK != command) ) 
        {
            syslog(LOG_ERR, "%s:%d invalid command %u ",__FUNCTION__, __LINE__, command);
        }
        if ( (calcLength != actualLength) ) 
        {
            syslog(LOG_ERR, "%s:%d payload length not as expected %u != %u ",__FUNCTION__, __LINE__, calcLength, actualLength);
        }
        if ( (retPoll <= 0) ) 
        {
            syslog(LOG_ERR, "%s:%d error getting message %u ",__FUNCTION__, __LINE__, retPoll);
        }
    }
    else
    {
        success = true;
        printf("REGISTER APP ACK TRUE \n");
    }

    printf("REGISTER_APP_ACK SIZE: %d\n", retBytes);
    return success;
}

void process_registerData(int32_t csocket)
{
    // send, only at boot
    enum registerData_params
    {
        CMD_ID                  = 2,
        LENGTH                  = 2,
        HOST_OS                 = 1,
        SRC_PROC_ID             = 4,
        SRC_APP_NAME            = 4,
        NUM_MPS                 = 4,
        PFP_VALUE               = 4,
        PTLT_TEMPERATURE        = 4,
        PTRT_TEMPERATURE        = 4,
        TCMP                    = 4,
        COP_PRESSURE            = 4,
        COP_FO_REAL             = 4,
        COP_FO_IMAG             = 4,
        COP_HO_REAL             = 4,
        COP_HO_IMAG             = 4,
        COP_SQ                  = 4,
        CRANK_FO_REAL           = 4,
        CRANK_FO_IMAG           = 4,
        CRANK_HO_REAL           = 4,
        CRANK_HO_IMAG           = 4,
        CRANK_SQ                = 4,
        CAM_SEC_1               = 4,
        CAM_NSEC_1              = 4,
        TURBO_REAL              = 4,
        TURBO_IMAG              = 4,
        TURBO_SQ                = 4,

        MSG_SIZE                =   CMD_ID + 
                                    LENGTH + 
                                    HOST_OS +
                                    SRC_PROC_ID +
                                    SRC_APP_NAME +
                                    NUM_MPS +
                                    PFP_VALUE +
                                    PTLT_TEMPERATURE +
                                    PTRT_TEMPERATURE +
                                    TCMP +
                                    COP_PRESSURE +
                                    COP_FO_REAL +
                                    COP_FO_IMAG +
                                    COP_HO_REAL +
                                    COP_HO_IMAG +
                                    COP_SQ +
                                    CRANK_FO_REAL +
                                    CRANK_FO_IMAG +
                                    CRANK_HO_REAL +
                                    CRANK_HO_IMAG +
                                    CRANK_SQ +
                                    CAM_SEC_1 +
                                    CAM_NSEC_1 +
                                    TURBO_REAL +
                                    TURBO_IMAG +
                                    TURBO_SQ,
    };

    bool success = true;
    int32_t sendBytes = 0;
    uint8_t *ptr , *msgLenPtr;
    uint32_t actualLength;
    uint8_t sendData[ MSG_SIZE ];
    int32_t test_val = 0;


    ptr = sendData;
    remote_set16( ptr , CMD_REGISTER_DATA);
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    *ptr = PETALINUX;
    ptr += HOST_OS;

    remote_set32( ptr , getpid() );
    ptr += SRC_PROC_ID;

    remote_set32( ptr , 1);
    ptr += SRC_APP_NAME;
 
    remote_set32( ptr , 2);
    ptr += NUM_MPS;

    remote_set32( ptr , 3);
    ptr += PFP_VALUE;

    remote_set32( ptr , 4);
    ptr += PTLT_TEMPERATURE;

    remote_set32( ptr , 5);
    ptr += PTRT_TEMPERATURE;

    remote_set32( ptr , 6);
    ptr += TCMP;

    remote_set32( ptr , 7);
    ptr += COP_PRESSURE;

    remote_set32( ptr , 8);
    ptr += COP_FO_REAL;

    remote_set32( ptr , 9);
    ptr += COP_FO_IMAG;

    remote_set32( ptr , 10);
    ptr += COP_HO_REAL;

    remote_set32( ptr , 11);
    ptr += COP_HO_IMAG;

    remote_set32( ptr , 12);
    ptr += COP_SQ;

    remote_set32( ptr , 13);
    ptr += CRANK_FO_REAL;

    remote_set32( ptr , 14);
    ptr += CRANK_FO_IMAG;

    remote_set32( ptr , 15);
    ptr += CRANK_HO_REAL;

    remote_set32( ptr , 16);
    ptr += CRANK_HO_IMAG;

    remote_set32( ptr , 17);
    ptr += CRANK_SQ;

    remote_set32( ptr , 18);
    ptr += CAM_SEC_1;

    remote_set32( ptr , 19);
    ptr += CAM_NSEC_1;

    remote_set32( ptr , 20);
    ptr += TURBO_REAL;

    remote_set32( ptr , 21);
    ptr += TURBO_IMAG;

    remote_set32( ptr , 22);
    ptr += TURBO_SQ;

    *ptr = 23;
    ptr += MSG_SIZE;

    // actual length
    actualLength = MSG_SIZE - CMD_ID - LENGTH;
    memcpy(msgLenPtr, &actualLength, sizeof(actualLength));

    // sending
    sendBytes = send( csocket, sendData, MSG_SIZE, 0 );

    if (MSG_SIZE != sendBytes)
    {
        success = false;
        syslog(LOG_ERR, "%s:%d insufficient message data %u != %u",__FUNCTION__, __LINE__, sendBytes, MSG_SIZE);
        printf("REGISTER_DATA ERROR\n");
    }
    else
    {
        success = true;
        printf("REGISTER_DATA TRUE\n");
    }
    printf("REGISTER_DATA SIZE: %d\n",sendBytes);
}

//
bool process_registerData_ack(int32_t csocket )
{
    enum registerApp_ack_params
    {
        CMD_ID                  = 2,
        LENGTH                  = 2,
        ERROR                   = 2,
        ERROR_PFP_VALUE         = 2,
        ERROR_PTLT_TEMPERATURE  = 2,
        ERROR_PTRT_TEMPERATURE  = 2,
        ERROR_TCMP              = 2,
        ERROR_COP_PRESSURE      = 2,
        ERROR_COP_FO_REAL       = 2,
        ERROR_COP_FO_IMAG       = 2,
        ERROR_COP_HO_REAL       = 2,
        ERROR_COP_HO_IMAG       = 2,
        ERROR_COP_SQ            = 2,
        ERROR_CRANK_FO_REAL     = 2,
        ERROR_CRANK_FO_IMAG     = 2,
        ERROR_CRANK_HO_REAL     = 2,
        ERROR_CRANK_HO_IMAG     = 2,
        ERROR_CRANK_SQ          = 2,
        ERROR_CAM_SEC_1         = 2,
        ERROR_CAM_NSEC_1        = 2,
        ERROR_TURBO_REAL        = 2,
        ERROR_TURBO_IMAG        = 2,
        ERROR_TURBO_SQ          = 2,
        MSG_SIZE                =   CMD_ID + 
                                    LENGTH + 
                                    ERROR +
                                    ERROR_PFP_VALUE +
                                    ERROR_PTLT_TEMPERATURE +
                                    ERROR_PTRT_TEMPERATURE +
                                    ERROR_TCMP +
                                    ERROR_COP_PRESSURE +
                                    ERROR_COP_FO_REAL +
                                    ERROR_COP_FO_IMAG +
                                    ERROR_COP_HO_REAL +
                                    ERROR_COP_HO_IMAG +
                                    ERROR_COP_SQ +
                                    ERROR_CRANK_FO_REAL +
                                    ERROR_CRANK_FO_IMAG +
                                    ERROR_CRANK_HO_REAL +
                                    ERROR_CRANK_HO_IMAG +
                                    ERROR_CRANK_SQ +
                                    ERROR_CAM_SEC_1 +
                                    ERROR_CAM_NSEC_1 +
                                    ERROR_TURBO_REAL +
                                    ERROR_TURBO_IMAG +
                                    ERROR_TURBO_SQ,
    };

    bool success = true;

    uint16_t command;

    uint32_t retBytes, errCnt, actualLength, calcLength;
    uint8_t retData[ MSG_SIZE ];
    uint8_t *ptr;

    uint16_t genErr, pfpErr, ptltErr, ptrtErr; 
    uint16_t tcmpErr, copErr, copFoRealErr, copFoImagErr; 
    uint16_t copHoRealErr, copHoImagErr, copSqErr, crankFoRealErr;
    uint16_t crankFoImagErr, crankHoRealErr, crankHoImagErr, crankSqErr;
    uint16_t camSecErr, camNSecErr, turboRealErr, turboImagErr, turboSqErr;

    errCnt = 0;

    struct pollfd myPoll[1];
    myPoll[0].fd = csocket;
    myPoll[0].events = POLLIN; 

    int32_t retPoll;
    struct timeval tv;

    // add timeout of ? 3 seconds here
    tv.tv_sec = 3;
    tv.tv_usec = 0;

    // WAIT TO RECEIVE RESPONSE ...
    retPoll = poll( myPoll, 1, -1 );
    if ( retPoll == -1 ) 
    {
        printf("ERROR WITH SELECT\n");
    }
    else if (retPoll == 0 ) 
    {
        printf("SELECT TIMEOUT ERROR\n");
    } 
    else
    {
        if (myPoll[0].revents & POLLIN)
        {
            retBytes = recv( csocket , retData , MSG_SIZE , 0 );

            command = remote_get16(retData);
            ptr = retData;
            ptr += CMD_ID;

            actualLength = remote_get16(ptr);
            ptr += LENGTH;

            genErr = remote_get16(ptr);
            if (0 != genErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR;

            pfpErr = remote_get16(ptr);
                if (0 != genErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_PFP_VALUE;

            ptltErr = remote_get16(ptr);
            if (0 != genErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_PTLT_TEMPERATURE;

            ptrtErr = remote_get16(ptr);
            if (0 != ptltErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_PTRT_TEMPERATURE;

            tcmpErr = remote_get16(ptr);
            if (0 != ptrtErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_TCMP;

            copErr = remote_get16(ptr);
            if (0 != copErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_COP_PRESSURE;

            copFoRealErr = remote_get16(ptr);
            if (0 != copFoRealErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_COP_FO_REAL;

            copFoImagErr = remote_get16(ptr);
            if (0 != copFoImagErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_COP_FO_IMAG;

            copHoRealErr = remote_get16(ptr);
            if (0 != copHoRealErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_COP_HO_REAL;

            copHoImagErr = remote_get16(ptr);
            if (0 != copHoImagErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_COP_HO_IMAG;

            copSqErr = remote_get16(ptr);
            if (0 != copSqErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_COP_SQ;

            crankFoRealErr = remote_get16(ptr);
            if (0 != crankFoRealErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_CRANK_FO_REAL;

            crankFoImagErr = remote_get16(ptr);
            if (0 != crankFoImagErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_CRANK_FO_IMAG;

            crankHoRealErr = remote_get16(ptr);
            if (0 != crankHoRealErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_CRANK_HO_REAL;

            crankHoImagErr = remote_get16(ptr);
            if (0 != crankHoImagErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_CRANK_HO_IMAG;

            crankSqErr = remote_get16(ptr);
            if (0 != crankSqErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_CRANK_SQ;

            camSecErr = remote_get16(ptr);
            if (0 != camSecErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_CAM_SEC_1;

            camNSecErr = remote_get16(ptr);
            if (0 != camNSecErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_CAM_NSEC_1;

            turboRealErr = remote_get16(ptr);
            if (0 != turboRealErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_TURBO_REAL;

            turboImagErr = remote_get16(ptr);
            if (0 != turboImagErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_TURBO_IMAG;

            turboSqErr = remote_get16(ptr);
            if (0 != turboSqErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_TURBO_SQ;


            // check: receive message size, error, or not the command ... 
            calcLength = MSG_SIZE - CMD_ID - LENGTH;
        }
    }

    if ( (retPoll <= 0) || (MSG_SIZE != retBytes) || ( errCnt > 0 ) || ( CMD_REGISTER_DATA_ACK != command ) || ( calcLength != actualLength ) )
    {
        success = false;
        printf("REGISTER_DATA_ACK ERROR\n");
        if ( (MSG_SIZE != retBytes) ) 
        {
            syslog(LOG_ERR, "%s:%d insufficient message data %u != %u", __FUNCTION__, __LINE__, retBytes, MSG_SIZE);
        }
        if ( ( errCnt > 0 ) ) 
        {
            syslog(LOG_ERR, "%s:%d MP error received %u", __FUNCTION__, __LINE__, errCnt);
        }
        if ( ( CMD_REGISTER_DATA_ACK != command ) ) 
        {
            syslog(LOG_ERR, "%s:%d invalid command %u", __FUNCTION__, __LINE__, command);
        }
        if ( ( calcLength != actualLength ) ) 
        {
            syslog(LOG_ERR, "%s:%d payload not equal to expected number of bytes %u != %u ",__FUNCTION__, __LINE__, calcLength, actualLength);
        }
        if ( (retPoll <= 0) ) 
        {
            syslog(LOG_ERR, "%s:%d error getting message %u ",__FUNCTION__, __LINE__, retPoll);
        }
    }
    else
    {
        success = true;
        printf("REGISTER_DATA_ACK TRUE\n");
    }
    printf("REGISTER_DATA_ACK SIZE: %d\n", retBytes);
    return success;
}


void process_openUDP( int32_t csocket , struct sockaddr_in addr_in )
{
    // send at boot
    enum openUDP_params
    {
        CMD_ID                  = 2,
        LENGTH                  = 2,
        APP_ID                  = 2,
        MSG_SIZE                =   CMD_ID + 
                                    LENGTH + 
                                    APP_ID,
    };

    int32_t sendBytes = 0;
    uint8_t *ptr , *msgLenPtr;
    uint32_t actualLength;
    uint8_t sendData[ MSG_SIZE ];

    socklen_t UDPaddr_size;

    ptr = sendData;
    remote_set16( ptr , CMD_OPEN);
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    *ptr = getpid();
    ptr += APP_ID;

    *ptr = MSG_SIZE;
    ptr += MSG_SIZE;

    // actual length
    actualLength =  MSG_SIZE - CMD_ID - LENGTH;
    memcpy(msgLenPtr, &actualLength, sizeof(actualLength));

    UDPaddr_size = sizeof(addr_in);

    // send
    sendBytes = sendto(csocket, sendData, MSG_SIZE, 0, (struct sockaddr *)&addr_in, UDPaddr_size);

    // check
    if ( (MSG_SIZE != sendBytes) )
    {
        syslog(LOG_ERR, "%s:%d insufficient message data %u != %u ", __FUNCTION__, __LINE__, sendBytes, MSG_SIZE);      
        printf("UDP_OPEN ERROR! \n");
    }
    else
    {
        printf("UDP_OPEN TRUE! \n");
    }
    printf("UDP_OPEN SIZE: %d\n" , sendBytes);
}


bool process_sysInit( int32_t csocket )
{

    enum sysinit_params
    {
        CMD_ID                  = 2,
        LENGTH                  = 2,
        MSG_SIZE                =   CMD_ID +
                                    LENGTH,
    };

    bool success = true;

    uint16_t command;

    uint32_t retBytes, i, errCnt, actualLength, calcLength;
    uint8_t retData[ MSG_SIZE ];
    uint8_t *ptr;

    socklen_t toRcvUDP_size;

    struct pollfd myPoll[1];
    myPoll[0].fd = csocket;
    myPoll[0].events = POLLIN; 

    int32_t retPoll;
    struct timeval tv;

    // add timeout of ? 3 seconds here
    tv.tv_sec = 3;
    tv.tv_usec = 0;

    // WAIT TO RECEIVE RESPONSE ...
    retPoll = poll( myPoll, 1, -1 );
    if ( retPoll == -1 ) 
    {
        printf("ERROR WITH SELECT\n");
    }
    else if (retPoll == 0 ) 
    {
        printf("SELECT TIMEOUT ERROR\n");
    } 
    else
    {
        if (myPoll[0].revents & POLLIN)
        {
            toRcvUDP_size = sizeof(toRcvUDP);
            retBytes = recvfrom(csocket , retData , MAXBUFSIZE , 0 , (struct sockaddr *)&toRcvUDP , &toRcvUDP_size);

            command = remote_get16(retData);
            ptr = retData;
            ptr += CMD_ID;

            actualLength = remote_get16(ptr);
            ptr += LENGTH;

            calcLength = MSG_SIZE - CMD_ID - LENGTH; // shoudl be zero
        }
    }


    // check message
    if ( (retPoll <= 0) || (MSG_SIZE != retBytes) || ( CMD_SYSINIT != command ) || ( calcLength != actualLength ) )
    {
        success = false;
        printf("SYS_INIT ERROR\n");
        if ( (MSG_SIZE != retBytes) )
        {
            printf("MSG_SIZE != retBytes\n");
            syslog(LOG_ERR, "%s:%d insufficient message data %u != %u", __FUNCTION__, __LINE__, retBytes, MSG_SIZE);
        }
        if ( ( CMD_SYSINIT != command ) )
        {
            printf("CMD_SYSINIT != command\n");
            // not sure if we really need to syslog this condition.  There will be a lot.
            syslog(LOG_ERR, "%s:%d invalid command %u", __FUNCTION__, __LINE__, command);
        }
        if ( ( calcLength != actualLength ) )
        {
            printf("calcLength != actualLength\n");
            syslog(LOG_ERR, "%s:%d payload not equal to expected number of bytes %u != %u ",__FUNCTION__, __LINE__, calcLength, actualLength);
        }
        if ( (retPoll <= 0) )
        {
            syslog(LOG_ERR, "%s:%d error getting message %u ",__FUNCTION__, __LINE__, retPoll);
        }
    }
    else
    {
        success = true;
        printf("SYS_INIT TRUE\n");
    }
    printf("SYS_INIT SIZE: %d\n\n", retBytes);
    return success;
}


// The MPs are hardcoded.  That is, they are not based on a subscription.  There is a block of comments (sys requirements) 
// pertaining to the validity of the MPs being published; however, that is only contingent on recceiving a subscription.
// 
// Not sure the best way to check for valid MPs being published without a subscription.    
void process_publish( int32_t csocket , struct sockaddr_in addr_in , int32_t LogMPs[] , int32_t time_secMP[] , int32_t time_nsecMP[] )
{
    // send
    enum publish_params
    {
        CMD_ID                  = 2,
        LENGTH                  = 2,
        TOPIC_ID                = 4,
        NUM_MPS                 = 4,
        SEQ_NUM                 = 2,
        MP                      = 4,
        MP_VAL                  = 4,
        MSG_SIZE                =   CMD_ID +
                                    LENGTH +
                                    TOPIC_ID +
                                    NUM_MPS +
                                    SEQ_NUM +
                                    5*(MP + MP_VAL),
    };

    bool success = true;
    int32_t sendBytes = 0;
    uint8_t *ptr , *msgLenPtr, *val_ptr;
    uint32_t i, k = 0;
    uint32_t actualLength;
    uint8_t sendData[ MAXBUFSIZE ];
    uint32_t cntTS = 0;
    uint32_t cntBytes = 0;
    int32_t cnt_true, cnt_false = 0;

    socklen_t toSendUDP_size;

    ptr = sendData;
    remote_set16( ptr , CMD_PUBLISH);
    ptr += CMD_ID;
    cntBytes += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;
    cntBytes += LENGTH;

    // this will change ...
    remote_set32( ptr , APP_FDL_ID);
    ptr += TOPIC_ID;
    cntBytes += TOPIC_ID;

    // this will change ...
    remote_set32( ptr , publishMe[0].numMPs);
    ptr += NUM_MPS;
    cntBytes += NUM_MPS;

    // ???
    remote_set16( ptr , 0);
    ptr += SEQ_NUM;
    cntBytes += SEQ_NUM;

    // published based on subscription (to include valid/invalid mps) 
    for (i = 0 ; i < publishMe[0].numMPs ; i++)
    {
        // re-incorporate this ... don't publish invalid MP conditions.  TODO
//      if ( false == subscribeMP[i].valid )
//      {
//          cnt_false++;
//          // invalid mp, don't publish ...
//          // break;
//      }
//      else
//      {
//          cnt_true++;
            // logicals ...
            if (true == publishMe[0].topicSubscription[i].logical)
            {
                remote_set32( ptr , publishMe[0].topicSubscription[i].mp);
                ptr += MP;
                cntBytes += MP;
                remote_set32( ptr , ( LogMPs[ publishMe[0].topicSubscription[i].mp - 1000 ] ));
                ptr += MP_VAL;
                cntBytes += MP_VAL;
            }
            // time stamps ...
            else
            {
                remote_set32( ptr , publishMe[0].topicSubscription[i].mp );
                ptr += MP;
                cntBytes += MP;
                if ( MP_CAM_SEC_1 == publishMe[0].topicSubscription[i].mp )
                {
                    for (k = 0; (k < 9) && (time_secMP[k] != 0); k++)
                    {
                        remote_set32( ptr , time_secMP[k]);
                        ptr += MP_VAL;
                        cntBytes += MP_VAL;
                    }
                }
                else if ( MP_CAM_NSEC_1 == publishMe[0].topicSubscription[i].mp )
                {
                    for(k = 0; (k < 9) && (time_nsecMP[k] != 0); k++)
                    {
                        remote_set32( ptr , time_nsecMP[k]);
                        ptr += MP_VAL;
                        cntBytes += MP_VAL;
                    }
                }
            }
//      }
    }


//  // MP 1
//  remote_set32( ptr , MP_PFP_VALUE);
//  ptr += MP;
//  cntBytes += MP;
//  remote_set32( ptr , LogMPs[0]);
//  ptr += MP_VAL;
//  cntBytes += MP_VAL;
//
//  // MP 2
//  remote_set32( ptr , MP_PTLT_TEMPERATURE);
//  ptr += MP;
//  cntBytes += MP;
//  remote_set32( ptr , LogMPs[1]);
//  ptr += MP_VAL;
//  cntBytes += MP_VAL;
//
//  // MP 3
//  remote_set32( ptr , MP_PTRT_TEMPERATURE);
//  ptr += MP;
//  cntBytes += MP;
//  remote_set32( ptr , LogMPs[2]);
//  ptr += MP_VAL;
//  cntBytes += MP_VAL;
//
//  // MP 4
//  remote_set32( ptr , MP_TCMP);
//  ptr += MP;
//  cntBytes += MP;
//  remote_set32( ptr , LogMPs[3]);
//  ptr += MP_VAL;
//  cntBytes += MP_VAL;
//
//  // MP 5
//  remote_set32( ptr , MP_CAM_SEC_1);
//  ptr += MP;
//  cntBytes += MP;
//  for( k = 0 ; (k < 9) && (time_secMP[k] != 0) ; k++ )
//  {
//      remote_set32( ptr , time_secMP[k]);
//      ptr += MP_VAL;
//      cntBytes += MP_VAL;
//  }
//
//  // MP 6
//  remote_set32( ptr , MP_CAM_NSEC_1);
//  ptr += MP;
//  cntBytes += MP;
//  for(k = 0; (k < 9) && (time_nsecMP[k] != 0); k++)
//  {
//      remote_set32( ptr , time_nsecMP[k]);
//      ptr += MP_VAL;
//      cntBytes += MP_VAL;
//  }
//
//  // MP 7
//  remote_set32( ptr , MP_COP_PRESSURE);
//  ptr += MP;
//  cntBytes += MP;
//  remote_set32( ptr , LogMPs[4]);
//  ptr += MP_VAL;
//  cntBytes += MP_VAL;

    *ptr = cntBytes;
    ptr += cntBytes;

    // actual length
    actualLength = cntBytes - CMD_ID - LENGTH;
    memcpy( msgLenPtr , &actualLength , sizeof(actualLength) );

    toSendUDP_size = sizeof(addr_in);
    sendBytes = sendto(csocket, sendData, cntBytes, 0, (struct sockaddr *)&addr_in, toSendUDP_size);
    //sendBytes = send( csocket , sendData , msgByteCnt , 0 );

    if ( cntBytes != sendBytes )
    {
        success = false;
        syslog(LOG_ERR, "%s:%d insufficient message data %u != %u", __FUNCTION__, __LINE__, sendBytes, cntBytes);
        printf("PUBLISH ERROR\n");
    }
    else
    {
        success = true;
        printf("PUBLISH TRUE\n");
    }
    printf( "PUBLISH SIZE: %d\n" , cntBytes );


}


bool process_subscribe( int32_t csocket )
{
    enum subscribe_params
    {
        CMD_ID                  = 2,
        LENGTH                  = 2,
        HOST_OS                 = 1,
        SRC_PROC_ID             = 4,
        SRC_APP_NAME            = 4,
        NUM_MPS                 = 4,
        SEQ_NUM                 = 2,
        MP                      = 4,
        MP_PER                  = 4,
        MP_NUM_SAMPLES          = 4,
        MSG_SIZE                =   CMD_ID +
                                    LENGTH +
                                    HOST_OS +
                                    SRC_PROC_ID +
                                    SRC_APP_NAME +
                                    NUM_MPS +
                                    SEQ_NUM +
                                    MP +
                                    MP_PER +
                                    MP_NUM_SAMPLES,
    };

    bool success = true;

    uint16_t command;

    uint32_t retBytes, i, errCnt, actualLength, calcLength;
    uint8_t retData[ MAXBUFSIZE ];
    uint8_t *ptr;

    uint8_t host_os;
    uint16_t seq_num;
    // uint32_t src_proc_id, src_app_name, cnt_retBytes, cntLogical_t, cntLogical_f;
    uint32_t src_proc_id, cnt_retBytes, cntLogical_t, cntLogical_f;

    cnt_retBytes = 0;

    socklen_t toRcvUDP_size;

    struct pollfd myPoll[1];
    myPoll[0].fd = csocket;
    myPoll[0].events = POLLIN; 

    int32_t retPoll;
    struct timeval tv;

    // add timeout of ? 3 seconds here
    tv.tv_sec = 3;
    tv.tv_usec = 0;

    // WAIT TO RECEIVE RESPONSE ...
    retPoll = poll( myPoll, 1, -1 );
    if ( retPoll == -1 ) 
    {
        printf("ERROR WITH SELECT\n");
    }
    else if (retPoll == 0 ) 
    {
        printf("SELECT TIMEOUT ERROR\n");
    } 
    else
    {
        if (myPoll[0].revents & POLLIN)
        {
            cntLogical_t = 0;
            cntLogical_f = 0;
            // don't know what exactly to get, so MAXBUFSIZE is used instead of MSG_SIZE like others
            //UDPaddr_size = sizeof(serverStorage_UDP);
            toRcvUDP_size = sizeof(toRcvUDP);
            retBytes = recvfrom(csocket , retData , MAXBUFSIZE , 0 , (struct sockaddr *)&toRcvUDP , &toRcvUDP_size);
            //retBytes = recv(csocket , retData , MAXBUFSIZE , 0 );
            printf("SUBSCRIBE - RECV: %d\n", retBytes);

            command = remote_get16(retData);
            ptr = retData;
            ptr += CMD_ID;
            cnt_retBytes += CMD_ID;

            actualLength = remote_get16(ptr);
            ptr += LENGTH;
            cnt_retBytes += LENGTH;

            memcpy(&host_os, ptr, sizeof(host_os));
            ptr += HOST_OS;
            cnt_retBytes += HOST_OS;

            src_proc_id = remote_get32(ptr);
            ptr += SRC_PROC_ID;
            cnt_retBytes += SRC_PROC_ID;

            src_app_name = remote_get32(ptr);
            ptr += SRC_APP_NAME;
            cnt_retBytes += SRC_APP_NAME;

            num_mps = remote_get32(ptr);
            ptr += NUM_MPS;
            cnt_retBytes += NUM_MPS;

            seq_num = remote_get16(ptr);
            ptr += SEQ_NUM;
            cnt_retBytes += SEQ_NUM;

            // realloc based on number of MPs ...
            // I know realloc will append if greater, but will it free if less?
        //  if ( new_num_mps < old_num_mps )
        //  {
        //      free(mp);
        //      free(mp_per);
        //      free(mp_num_samples);
        //  }

            publishMe[0].topic_id = 1000;
            //publishMe[0].period = PERIOD;
            publishMe[0].numMPs = num_mps;
            publishMe[0].topicSubscription = malloc(sizeof(MPinfo));
            publishMe[0].topicSubscription = realloc(publishMe[0].topicSubscription, sizeof(MPinfo)*publishMe[0].numMPs);

            printf("publishMe[0].numMPs: %d\n", publishMe[0].numMPs);
//          subscribeMP = (MPinfo*) malloc( sizeof(MPinfo) );
//          if ( subscribeMP == NULL )
//          {
//              printf("BAD MALLOC\n");
//          }
        //  printf("SIZE OF MALLOC subscribeMP: %lu\n", sizeof(subscribeMP));
        //  printf("SIZE OF MPinfo: %lu\n", sizeof(MPinfo));

//          subscribeMP = (MPinfo *)realloc( subscribeMP , sizeof(MPinfo)*num_mps );
//          if ( subscribeMP == NULL )
//          {
//              printf("BAD REALLOC\n");
//          }

            //subscribeMP = realloc( subscribeMP , num_mps * sizeof(MPinfo) );
        //  printf("SUBSCRIBE - REALLOC\n");
        //  printf("SIZE OF REALLOC subscribeMP: %lu\n", sizeof(subscribeMP));
        //  printf("SIZE OF MPinfo: %lu\n", sizeof(MPinfo));

        //  mp = realloc(mp, num_mps * sizeof(uint32_t));
        //  mp_per = realloc( mp_per , num_mps*sizeof(uint32_t) );
        //  mp_num_samples = realloc( mp_num_samples , num_mps*sizeof(uint32_t) );

            for ( i = 0 ; i < publishMe[0].numMPs ; i++ )
            {
                publishMe[0].topicSubscription[i].mp = remote_get32(ptr);
                ptr += MP;
                cnt_retBytes += MP;

                if ( (publishMe[0].topicSubscription[i].mp == MP_PFP_VALUE ) || 
                     (publishMe[0].topicSubscription[i].mp == MP_PTLT_TEMPERATURE ) || 
                     (publishMe[0].topicSubscription[i].mp == MP_PTRT_TEMPERATURE ) || 
                     (publishMe[0].topicSubscription[i].mp == MP_TCMP ) || 
                     (publishMe[0].topicSubscription[i].mp == MP_COP_PRESSURE ) ) 
                {
                    publishMe[0].topicSubscription[i].logical = true;
                    cntLogical_t++;
                }
                else
                {
                    publishMe[0].topicSubscription[i].logical = false;
                    cntLogical_f++;
                }

                publishMe[0].topicSubscription[i].period = remote_get32(ptr);
                ptr += MP_PER;
                cnt_retBytes += MP_PER;

                publishMe[0].topicSubscription[i].numSamples = remote_get32(ptr);
                ptr += MP_NUM_SAMPLES;
                cnt_retBytes += MP_NUM_SAMPLES;

            }
            printf("TRUE LOG: %d\n", cntLogical_t);
            printf("FALSE LOG: %d\n", cntLogical_f);
            calcLength = cnt_retBytes - CMD_ID - LENGTH;
        }
    }

    if ( (retPoll <= 0) || (cnt_retBytes != retBytes) || ( CMD_SUBSCRIBE != command ) || ( calcLength != actualLength ) )
    {
        success = false;
        printf("SUBSCRIBE ERROR\n");
        if ( (cnt_retBytes != retBytes) )
        {
            printf("cnt_retBytes != retBytes\n");
            syslog(LOG_ERR, "%s:%d insufficient message data %u != %u", __FUNCTION__, __LINE__, retBytes, cnt_retBytes);
        }
        if ( ( CMD_SUBSCRIBE != command ) )
        {
            printf("CMD_SUBSCRIBE != command\n");
            // not sure if we really need to syslog this condition.  There will be a lot.
            syslog(LOG_ERR, "%s:%d invalid command %u", __FUNCTION__, __LINE__, command);
        }
        if ( ( calcLength != actualLength ) )
        {
            printf("calcLength != actualLength\n");
            syslog(LOG_ERR, "%s:%d payload not equal to expected number of bytes %u != %u ",__FUNCTION__, __LINE__, calcLength, actualLength);
        }
        if ( (retPoll <= 0) )
        {
            syslog(LOG_ERR, "%s:%d error getting message %u ",__FUNCTION__, __LINE__, retPoll);
        }
    }
    else
    {
        success = true;
        printf("SUBSCRIBE TRUE\n");
    }
    printf("SUBSCRIBE SIZE: %d\n\n", retBytes);
//  printf("CALC LENGTH: %d\n",calcLength);
//  printf("ACTUAL LENGTH: %d\n",actualLength);
//  printf("CNT BYTES: %d\n",cnt_retBytes);
    return success;
}


//bool process_subscribe_ack( int32_t csocket )
bool process_subscribe_ack( int32_t csocket , struct sockaddr_in addr_in )
{

    enum subscribe_ack_params
    {
        CMD_ID                  = 2,
        LENGTH                  = 2,
        TOPIC_ID                = 4,
        ERROR                   = 2,
        ERROR_MP                = 2,
        MSG_SIZE                =   CMD_ID +
                                    LENGTH +
                                    TOPIC_ID +
                                    ERROR +
                                    ERROR_MP,
    };

    bool success = true;
    int32_t sendBytes = 0;
    uint8_t *ptr , *msgLenPtr, *msgErrPtr;
    uint32_t i = 0;
    uint32_t actualLength;
    uint8_t sendData[ MAXBUFSIZE ];
    int32_t cntBytes;
    int32_t genErr = 0;

    socklen_t toSendUDP_size;

    cntBytes = 0;

    ptr = sendData;
    remote_set16( ptr , CMD_SUBSCRIBE_ACK );
    ptr += CMD_ID;
    cntBytes += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;
    cntBytes += LENGTH;

    remote_set32( ptr , APP_FDL_ID );
    ptr += TOPIC_ID;
    cntBytes += TOPIC_ID;

    msgErrPtr = ptr;
    ptr += ERROR;
    cntBytes += ERROR;

    subPeriodToChk = publishMe[0].topicSubscription[0].period;
    for ( i = 0 ; i < publishMe[0].numMPs ; i++ ) 
    {
        if ( subPeriodToChk == publishMe[0].topicSubscription[i].period )
        {
            publishPeriod = subPeriodToChk;
            if (true == check_validMP(i))
            {
                remote_set16( ptr , GE_SUCCESS);
                genErr = genErr;
            }
            else
            {
                remote_set16( ptr , GE_INVALID_MP_NUMBER);
                genErr = GE_INVALID_MP_NUMBER;
                success = false;
            }
            ptr += ERROR_MP;
            goPublish = true;
        }
        else
        {
            publishPeriod = -1;
            printf("MP PERIODS NOT THE SAME\n");
            publishMe[0].topicSubscription[i].valid = false;
            remote_set16( ptr , GE_INVALID_MP_NUMBER);
            genErr = GE_INVALID_MP_NUMBER;
            success = false;
            goPublish = false;
            break;
        }
        cntBytes += ERROR_MP;
    }
    publishMe[0].period = publishPeriod;


    *ptr = MSG_SIZE;
    ptr += MSG_SIZE;

    actualLength = cntBytes - CMD_ID - LENGTH;

    memcpy(msgLenPtr, &actualLength, sizeof(cntBytes));
    memcpy(msgErrPtr, &genErr, sizeof(genErr));

    // send
    toSendUDP_size = sizeof(addr_in);
    sendBytes = sendto(csocket, sendData, cntBytes, 0, (struct sockaddr *)&addr_in, toSendUDP_size);

    // check message
    if ( (cntBytes != sendBytes) )
    {
        syslog(LOG_ERR, "%s:%d insufficient message data %u != %u ", __FUNCTION__, __LINE__, sendBytes, cntBytes);      
        printf("SUBSCRIBE_ACK ERROR! \n");
    }
    else
    {
        printf("SUBSCRIBE_ACK TRUE! \n");
    }
    printf("SUBSCRIBE_ACK SIZE: %d\n\n" , sendBytes);

    return success;
}


bool check_validMP( int32_t fromLoop )
{
    bool success = true;
    int32_t i, cnt;

    int32_t SIMMsubscriptionMP[MAX_SIMM_SUBSCRIPTION] = 
    {
        MP_PFP_VALUE,
        MP_PTLT_TEMPERATURE,
        MP_PTRT_TEMPERATURE,
        MP_TCMP,
        MP_CAM_SEC_1,
        MP_CAM_NSEC_1,
        MP_COP_PRESSURE
    };

    int32_t SIMMsubscriptionPeriod[ MAX_SIMM_SUBSCRIPTION ] =
    {
        MINPER_PFP_VALUE,
        MINPER_PTLT_TEMPERATURE,
        MINPER_PTRT_TEMPERATURE,
        MINPER_TCMP,
        MINPER_CAM_SEC_1,
        MINPER_CAM_NSEC_1,
        MINPER_COP_PRESSURE
    };

    for ( i = 0 ; i < MAX_SIMM_SUBSCRIPTION ; i++ ) 
    {
        if ( ( SIMMsubscriptionMP[i] == publishMe[0].topicSubscription[fromLoop].mp ) && ( ( SIMMsubscriptionPeriod[i] % publishMe[0].topicSubscription[fromLoop].period ) == 0 ) )
        {
            publishMe[0].topicSubscription[i].valid = true;
            // valid MP and valid MP period
            break;
        }
        else if ( ( SIMMsubscriptionMP[i] == publishMe[0].topicSubscription[fromLoop].mp ) && ( ( SIMMsubscriptionPeriod[i] % publishMe[0].topicSubscription[fromLoop].period ) != 0 ) ) 
        {
            // valid MP period and invalid MP period
            publishMe[0].topicSubscription[i].valid = true;
            success = false;
            break;
        }
        else
        {
            // keeping track of instanaces that don't meet above conditions
            publishMe[0].topicSubscription[i].valid = false;
            cnt++;
        }
    }
            if ( cnt == MAX_SIMM_SUBSCRIPTION ) 
    {
        // if MAX_SIMM_SUBSCRIPTION instances are reached, no valid MPs were sent
        success = false;
    }
    return success;
}

void remote_set16(uint8_t *ptr, uint16_t val) 
{
    /* The easiest way to do this would be to cast the buffer pointer as
     * a uint16_t, then assign the byteswap-d value to the cast buffer pointer.
     * But this isn't necessarily alignment-safe on all platforms.
     *
     * *((uint16_t*) ptr) = htobe16(val);
     *
     * Everything has to be hard so we do it our own way. */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    /* The local variable can be assigned to a byte array and can be
     * byte-accessed safely. */
    uint8_t *val_ptr = (uint8_t*) &val;

//  ptr[1] = val_ptr[0];
//  ptr[0] = val_ptr[1];
    ptr[0] = val_ptr[0];
    ptr[1] = val_ptr[1];
#else
    memcpy(ptr, &val, sizeof(val));
#endif
}


void remote_set32(uint8_t *ptr, uint32_t val) 
{
    /* The easiest way to do this would be to cast the buffer pointer as
     * a uint32_t, then assign the byteswap-d value to the cast buffer pointer.
     * But this isn't necessarily alignment-safe on all platforms.
     *
     * *((uint32_t*) ptr) = htobe32(val);
     *
     * Everything has to be hard so we do it our own way. */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    /* The local variable can be assigned to a byte array and can be
     * byte-accessed safely. */
    uint8_t *val_ptr = (uint8_t*) &val;

//  ptr[3] = val_ptr[0];
//  ptr[2] = val_ptr[1];
//  ptr[1] = val_ptr[2];
//  ptr[0] = val_ptr[3];
    ptr[0] = val_ptr[0];
    ptr[1] = val_ptr[1];
    ptr[2] = val_ptr[2];
    ptr[3] = val_ptr[3];
#else
    memcpy(ptr, &val, sizeof(val));
#endif
}

void remote_set64(uint8_t *ptr, uint64_t val) 
{
    /* The easiest way to do this would be to cast the buffer pointer as
     * a uint64_t, then assign the byteswap-d value to the cast buffer pointer.
     * But this isn't necessarily alignment-safe on all platforms.
     *
     * *((uint64_t*) ptr) = htobe64(val);
     *
     * Everything has to be hard so we do it our own way. */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    /* The local variable can be assigned to a byte array and can be
     * byte-accessed safely. */
    uint8_t *val_ptr = (uint8_t*) &val;

    ptr[0] = val_ptr[0];
    ptr[1] = val_ptr[1];
    ptr[2] = val_ptr[2];
    ptr[3] = val_ptr[3];
    ptr[4] = val_ptr[4];
    ptr[5] = val_ptr[5];
    ptr[6] = val_ptr[6];
    ptr[7] = val_ptr[7];
#else
    memcpy(ptr, &val, sizeof(val));
#endif
}

uint16_t remote_get16(uint8_t *ptr) 
{
    uint16_t val;

    /* The easiest way to do this would be to cast the buffer, and then byteswap
     * the resulting 16bit value.  But this isn't necessarily alignment-safe on
     * all platforms.
     *
     * val = be16toh(*((uint16_t*) ptr));
     *
     * Everything has to be hard so we do it our own way. */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    val = ptr[1];
    val <<= 8;
    val |= ptr[0];
#else
    memcpy(&val, ptr, sizeof(val));
#endif

    return val;
}

uint32_t remote_get32(uint8_t *ptr) 
{
    uint32_t val;
    /* The easiest way to do this would be to cast the buffer, and then byteswap
     * the resulting 16bit value.  But this isn't necessarily alignment-safe on
     * all platforms.
     *
     * val = be16toh(*((uint16_t*) ptr));
     *
     * Everything has to be hard so we do it our own way. */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    val = ptr[3];
    val <<= 8;
    val |= ptr[2];
    val <<= 8;
    val |= ptr[1];
    val <<= 8;
    val |= ptr[0];
#else
    memcpy(&val, ptr, sizeof(val));
#endif


    return val;
}

// return: true = time has not elapsed, false = time has elapsed
bool check_elapsedTime( struct timespec startTime , struct timespec stopTime , int32_t timeToChk )
{
    bool success = true;

    if ( (stopTime.tv_sec - startTime.tv_sec) < timeToChk )              
    {
        success = true;
    }
    else if ((stopTime.tv_sec - startTime.tv_sec) == timeToChk ) 
    {
        if (stopTime.tv_nsec < startTime.tv_nsec)  
        {
            success = true;
        }
        else
        {
            success = false;
        }
    }
    else
    {
        success = false;
    }

    return success;
}




