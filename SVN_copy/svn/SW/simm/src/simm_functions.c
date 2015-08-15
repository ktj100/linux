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

bool process_registerApp( int32_t csocket , struct timespec goTime )
{
    bool success = true;

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
    uint32_t actualLength, secsToChk;
    uint8_t sendData[ MSG_SIZE ];
    uint32_t totalTime, finishTime, startTime, i;
    struct timespec endTime;
    
    ptr = sendData;
    remote_set16( ptr , CMD_REGISTER_APP);
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    *ptr = PETALINUX;
    ptr += HOST_OS;

    remote_set32( ptr , getpid() );
    ptr += SRC_PROC_ID;

    remote_set32( ptr , 0);
    ptr += SRC_APP_NAME;

    *ptr = MSG_SIZE;
    ptr += MSG_SIZE;

    secsToChk = 1;

    actualLength = MSG_SIZE - CMD_ID - LENGTH;
    memcpy(msgLenPtr, &actualLength, sizeof(actualLength));

    // used to check "within 1 second" requirement between server connect and REGISTER_APP
    clock_gettime(CLOCK_REALTIME, &endTime);

    if (false == numSecondsHaveElapsed( goTime , endTime , secsToChk ) )
    {
        // time did not elapse, so send
        sendBytes = send(csocket, sendData, MSG_SIZE, 0);
    }
    else
    {
        syslog(LOG_ERR, "%s:%d ERROR! failed to send REGISTER_APP to AACM within one second of connecting", __FUNCTION__, __LINE__);
        success = false;
        printf("ELAPSED TIME GREATER THAN 1 SECOND!\n");
    }

    if (MSG_SIZE != sendBytes)
    {
        syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u",__FUNCTION__, __LINE__, sendBytes, MSG_SIZE);
        success = false;
    }
    else
    {
        //printf("REGISTER_APP TRUE \n");
    }
    //printf("REGISTER_APP SIZE: %d\n", sendBytes);
    return success;
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
        syslog(LOG_ERR, "%s:%d ERROR! error from pollfd poll()",__FUNCTION__, __LINE__);
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
            syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u",__FUNCTION__, __LINE__, retBytes, MSG_SIZE);
        }
        if ( ( appAckErr != 0 ) ) 
        {
            syslog(LOG_ERR, "%s:%d ERROR! payload error received %u ",__FUNCTION__, __LINE__, appAckErr);
        } 
        if ( (CMD_REGISTER_APP_ACK != command) ) 
        {
            syslog(LOG_ERR, "%s:%d ERROR! invalid command %u ",__FUNCTION__, __LINE__, command);
        }
        if ( (calcLength != actualLength) ) 
        {
            syslog(LOG_ERR, "%s:%d ERROR! payload length not as expected %u != %u ",__FUNCTION__, __LINE__, calcLength, actualLength);
        }
        if ( (retPoll <= 0) ) 
        {
            syslog(LOG_ERR, "%s:%d ERROR! error getting message %u ",__FUNCTION__, __LINE__, retPoll);
        }
    }
    else
    {
        success = true;
        //printf("REGISTER APP ACK TRUE \n");
    }

    //printf("REGISTER_APP_ACK SIZE: %d\n", retBytes);
    return success;
}

bool process_registerData(int32_t csocket)
{
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
//      COP_FO_REAL             = 4,
//      COP_FO_IMAG             = 4,
//      COP_HO_REAL             = 4,
//      COP_HO_IMAG             = 4,
//      COP_SQ                  = 4,
//      CRANK_FO_REAL           = 4,
//      CRANK_FO_IMAG           = 4,
//      CRANK_HO_REAL           = 4,
//      CRANK_HO_IMAG           = 4,
//      CRANK_SQ                = 4,
        CAM_SEC_1               = 4,
        CAM_NSEC_1              = 4,
        CAM_SEC_2               = 4,
        CAM_NSEC_2              = 4,
        CAM_SEC_3               = 4,
        CAM_NSEC_3              = 4,
        CAM_SEC_4               = 4,
        CAM_NSEC_4              = 4,
        CAM_SEC_5               = 4,
        CAM_NSEC_5              = 4,
        CAM_SEC_6               = 4,
        CAM_NSEC_6              = 4,
        CAM_SEC_7               = 4,
        CAM_NSEC_7              = 4,
        CAM_SEC_8               = 4,
        CAM_NSEC_8              = 4,
        CAM_SEC_9               = 4,
        CAM_NSEC_9              = 4,
//      TURBO_REAL              = 4,
//      TURBO_IMAG              = 4,
//      TURBO_SQ                = 4,
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
//                                  COP_FO_REAL +
//                                  COP_FO_IMAG +
//                                  COP_HO_REAL +
//                                  COP_HO_IMAG +
//                                  COP_SQ +
//                                  CRANK_FO_REAL +
//                                  CRANK_FO_IMAG +
//                                  CRANK_HO_REAL +
//                                  CRANK_HO_IMAG +
//                                  CRANK_SQ +
                                    CAM_SEC_1  +
                                    CAM_NSEC_1 +
                                    CAM_SEC_2  +
                                    CAM_NSEC_2 +
                                    CAM_SEC_3  +
                                    CAM_NSEC_3 +
                                    CAM_SEC_4  +
                                    CAM_NSEC_4 +
                                    CAM_SEC_5  +
                                    CAM_NSEC_5 +
                                    CAM_SEC_6  +
                                    CAM_NSEC_6 +
                                    CAM_SEC_7  +
                                    CAM_NSEC_7 +
                                    CAM_SEC_8  +
                                    CAM_NSEC_8 +
                                    CAM_SEC_9  +
                                    CAM_NSEC_9,
//                                  TURBO_REAL +
//                                  TURBO_IMAG +
//                                  TURBO_SQ,
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
    remote_set32( ptr , MAX_SIMM_SUBSCRIPTION);
    ptr += NUM_MPS;
    remote_set32( ptr , MP_PFP_VALUE);
    ptr += PFP_VALUE;
    remote_set32( ptr , MP_PTLT_TEMPERATURE);
    ptr += PTLT_TEMPERATURE;
    remote_set32( ptr , MP_PTRT_TEMPERATURE);
    ptr += PTRT_TEMPERATURE;
    remote_set32( ptr , MP_TCMP);
    ptr += TCMP;
    remote_set32( ptr , MP_COP_PRESSURE);
    ptr += COP_PRESSURE;

//  remote_set32( ptr , 8);
//  ptr += COP_FO_REAL;
//
//  remote_set32( ptr , 9);
//  ptr += COP_FO_IMAG;
//
//  remote_set32( ptr , 10);
//  ptr += COP_HO_REAL;
//
//  remote_set32( ptr , 11);
//  ptr += COP_HO_IMAG;
//
//  remote_set32( ptr , 12);
//  ptr += COP_SQ;
//
//  remote_set32( ptr , 13);
//  ptr += CRANK_FO_REAL;
//
//  remote_set32( ptr , 14);
//  ptr += CRANK_FO_IMAG;
//
//  remote_set32( ptr , 15);
//  ptr += CRANK_HO_REAL;
//
//  remote_set32( ptr , 16);
//  ptr += CRANK_HO_IMAG;
//
//  remote_set32( ptr , 17);
//  ptr += CRANK_SQ;

    remote_set32( ptr , MP_CAM_SEC_1);
    ptr += CAM_SEC_1;
    remote_set32( ptr , MP_CAM_NSEC_1);
    ptr += CAM_NSEC_1;
    remote_set32( ptr , MP_CAM_SEC_2);
    ptr += CAM_SEC_2;
    remote_set32( ptr , MP_CAM_NSEC_2);
    ptr += CAM_NSEC_2;
    remote_set32( ptr , MP_CAM_SEC_3);
    ptr += CAM_SEC_3;
    remote_set32( ptr , MP_CAM_NSEC_3);
    ptr += CAM_NSEC_3;
    remote_set32( ptr , MP_CAM_SEC_4);
    ptr += CAM_SEC_4;
    remote_set32( ptr , MP_CAM_NSEC_4);
    ptr += CAM_NSEC_4;
    remote_set32( ptr , MP_CAM_SEC_5);
    ptr += CAM_SEC_5;
    remote_set32( ptr , MP_CAM_NSEC_5);
    ptr += CAM_NSEC_5;
    remote_set32( ptr , MP_CAM_SEC_6);
    ptr += CAM_SEC_6;
    remote_set32( ptr , MP_CAM_NSEC_6);
    ptr += CAM_NSEC_6;
    remote_set32( ptr , MP_CAM_SEC_7);
    ptr += CAM_SEC_7;
    remote_set32( ptr , MP_CAM_NSEC_7);
    ptr += CAM_NSEC_7;
    remote_set32( ptr , MP_CAM_SEC_8);
    ptr += CAM_SEC_8;
    remote_set32( ptr , MP_CAM_NSEC_8);
    ptr += CAM_NSEC_8;
    remote_set32( ptr , MP_CAM_SEC_9);
    ptr += CAM_SEC_9;
    remote_set32( ptr , MP_CAM_NSEC_9);
    ptr += CAM_NSEC_9;
    
//  remote_set32( ptr , 20);
//  ptr += TURBO_REAL;
//
//  remote_set32( ptr , 21);
//  ptr += TURBO_IMAG;
//
//  remote_set32( ptr , 22);
//  ptr += TURBO_SQ;

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
        syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u",__FUNCTION__, __LINE__, sendBytes, MSG_SIZE);
        printf("REGISTER_DATA ERROR\n");
    }
    else
    {
        success = true;
        //printf("REGISTER_DATA TRUE\n");
    }
    //printf("REGISTER_DATA SIZE: %d\n",sendBytes);
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
//      ERROR_COP_FO_REAL       = 2,
//      ERROR_COP_FO_IMAG       = 2,
//      ERROR_COP_HO_REAL       = 2,
//      ERROR_COP_HO_IMAG       = 2,
//      ERROR_COP_SQ            = 2,
//      ERROR_CRANK_FO_REAL     = 2,
//      ERROR_CRANK_FO_IMAG     = 2,
//      ERROR_CRANK_HO_REAL     = 2,
//      ERROR_CRANK_HO_IMAG     = 2,
//      ERROR_CRANK_SQ          = 2,
        ERROR_CAM_SEC_1         = 2,
        ERROR_CAM_NSEC_1        = 2,
        ERROR_CAM_SEC_2         = 2,
        ERROR_CAM_NSEC_2        = 2,
        ERROR_CAM_SEC_3         = 2,
        ERROR_CAM_NSEC_3        = 2,
        ERROR_CAM_SEC_4         = 2,
        ERROR_CAM_NSEC_4        = 2,
        ERROR_CAM_SEC_5         = 2,
        ERROR_CAM_NSEC_5        = 2,
        ERROR_CAM_SEC_6         = 2,
        ERROR_CAM_NSEC_6        = 2,
        ERROR_CAM_SEC_7         = 2,
        ERROR_CAM_NSEC_7        = 2,
        ERROR_CAM_SEC_8         = 2,
        ERROR_CAM_NSEC_8        = 2,
        ERROR_CAM_SEC_9         = 2,
        ERROR_CAM_NSEC_9        = 2,
//      ERROR_TURBO_REAL        = 2,
//      ERROR_TURBO_IMAG        = 2,
//      ERROR_TURBO_SQ          = 2,
        MSG_SIZE                =   CMD_ID + 
                                    LENGTH + 
                                    ERROR +
                                    ERROR_PFP_VALUE +
                                    ERROR_PTLT_TEMPERATURE +
                                    ERROR_PTRT_TEMPERATURE +
                                    ERROR_TCMP +
                                    ERROR_COP_PRESSURE +
//                                  ERROR_COP_FO_REAL +
//                                  ERROR_COP_FO_IMAG +
//                                  ERROR_COP_HO_REAL +
//                                  ERROR_COP_HO_IMAG +
//                                  ERROR_COP_SQ +
//                                  ERROR_CRANK_FO_REAL +
//                                  ERROR_CRANK_FO_IMAG +
//                                  ERROR_CRANK_HO_REAL +
//                                  ERROR_CRANK_HO_IMAG +
//                                  ERROR_CRANK_SQ +
                                    ERROR_CAM_SEC_1  +
                                    ERROR_CAM_NSEC_1 +
                                    ERROR_CAM_SEC_2  +
                                    ERROR_CAM_NSEC_2 +
                                    ERROR_CAM_SEC_3  +
                                    ERROR_CAM_NSEC_3 +
                                    ERROR_CAM_SEC_4  +
                                    ERROR_CAM_NSEC_4 +
                                    ERROR_CAM_SEC_5  +
                                    ERROR_CAM_NSEC_5 +
                                    ERROR_CAM_SEC_6  +
                                    ERROR_CAM_NSEC_6 +
                                    ERROR_CAM_SEC_7  +
                                    ERROR_CAM_NSEC_7 +
                                    ERROR_CAM_SEC_8  +
                                    ERROR_CAM_NSEC_8 +
                                    ERROR_CAM_SEC_9  +
                                    ERROR_CAM_NSEC_9,
//                                  ERROR_TURBO_REAL +
//                                  ERROR_TURBO_IMAG +
//                                  ERROR_TURBO_SQ,
    };

    bool success = true;

    uint16_t command;

    uint32_t retBytes, errCnt, actualLength, calcLength, i;
    uint8_t retData[ MSG_SIZE ];
    uint8_t *ptr;

    uint16_t genErr, pfpErr, ptltErr, ptrtErr; 
    uint16_t tcmpErr, copErr, copFoRealErr, copFoImagErr; 
    uint16_t copHoRealErr, copHoImagErr, copSqErr, crankFoRealErr;
    uint16_t crankFoImagErr, crankHoRealErr, crankHoImagErr, crankSqErr;
    uint16_t camSecErr[ MAX_TIMESTAMPS ];
    uint16_t camNSecErr[ MAX_TIMESTAMPS ]; 
    uint16_t turboRealErr, turboImagErr, turboSqErr;

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
        syslog(LOG_ERR, "%s:%d ERROR! error from pollfd poll()",__FUNCTION__, __LINE__);
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
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! genErr",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR;

            pfpErr = remote_get16(ptr);
                if (0 != pfpErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! pfpErr",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_PFP_VALUE;

            ptltErr = remote_get16(ptr);
            if (0 != ptltErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! ptltErr",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_PTLT_TEMPERATURE;

            ptrtErr = remote_get16(ptr);
            if (0 != ptrtErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! ptrtErr",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_PTRT_TEMPERATURE;

            tcmpErr = remote_get16(ptr);
            if (0 != tcmpErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! tcmpErr",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_TCMP;

            copErr = remote_get16(ptr);
            if (0 != copErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! copErr",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_COP_PRESSURE;

            for( i = 0 ; i < MAX_TIMESTAMPS ; i++ )
            {
                camSecErr[i] = remote_get16(ptr);
                if (0 != camSecErr[i]) 
                {
                    syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! camSecErr%d",__FUNCTION__, __LINE__, i);
                    errCnt++;
                }
                ptr += ERROR_CAM_SEC_1;

                camNSecErr[i] = remote_get16(ptr);
                if (0 != camNSecErr[i]) 
                {
                    syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! camNSecErr%d",__FUNCTION__, __LINE__, i);
                    errCnt++;
                }
                ptr += ERROR_CAM_NSEC_1;
            }

//          copFoRealErr = remote_get16(ptr);
//          if (0 != copFoRealErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_COP_FO_REAL;
//
//          copFoImagErr = remote_get16(ptr);
//          if (0 != copFoImagErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_COP_FO_IMAG;
//
//          copHoRealErr = remote_get16(ptr);
//          if (0 != copHoRealErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_COP_HO_REAL;
//
//          copHoImagErr = remote_get16(ptr);
//          if (0 != copHoImagErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_COP_HO_IMAG;
//
//          copSqErr = remote_get16(ptr);
//          if (0 != copSqErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_COP_SQ;
//
//          crankFoRealErr = remote_get16(ptr);
//          if (0 != crankFoRealErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_CRANK_FO_REAL;
//
//          crankFoImagErr = remote_get16(ptr);
//          if (0 != crankFoImagErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_CRANK_FO_IMAG;
//
//          crankHoRealErr = remote_get16(ptr);
//          if (0 != crankHoRealErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_CRANK_HO_REAL;
//
//          crankHoImagErr = remote_get16(ptr);
//          if (0 != crankHoImagErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_CRANK_HO_IMAG;
//
//          crankSqErr = remote_get16(ptr);
//          if (0 != crankSqErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_CRANK_SQ;
//
//          turboRealErr = remote_get16(ptr);
//          if (0 != turboRealErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_TURBO_REAL;
//
//          turboImagErr = remote_get16(ptr);
//          if (0 != turboImagErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_TURBO_IMAG;
//
//          turboSqErr = remote_get16(ptr);
//          if (0 != turboSqErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_TURBO_SQ;


            // check: receive message size, error, or not the command ... 
            calcLength = MSG_SIZE - CMD_ID - LENGTH;
        }
    }

    if ( (retPoll <= 0) || (MSG_SIZE != retBytes) || ( errCnt > 0 ) || ( CMD_REGISTER_DATA_ACK != command ) || ( calcLength != actualLength ) )
    {
        success = false;
        if ( (MSG_SIZE != retBytes) ) 
        {
            syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u", __FUNCTION__, __LINE__, retBytes, MSG_SIZE);
        }
        if ( ( errCnt > 0 ) ) 
        {
            syslog(LOG_ERR, "%s:%d ERROR! MP errors received: %u", __FUNCTION__, __LINE__, errCnt);
        }
        if ( ( CMD_REGISTER_DATA_ACK != command ) ) 
        {
            syslog(LOG_ERR, "%s:%d ERROR! invalid command %u", __FUNCTION__, __LINE__, command);
        }
        if ( ( calcLength != actualLength ) ) 
        {
            syslog(LOG_ERR, "%s:%d ERROR! payload not equal to expected number of bytes %u != %u ",__FUNCTION__, __LINE__, calcLength, actualLength);
        }
        if ( (retPoll <= 0) ) 
        {
            syslog(LOG_ERR, "%s:%d ERROR! error getting message %u ",__FUNCTION__, __LINE__, retPoll);
        }
    }
    else
    {
        success = true;
        //printf("REGISTER_DATA_ACK TRUE\n");
    }
    //printf("REGISTER_DATA_ACK SIZE: %d\n", retBytes);
    return success;
}


bool process_openUDP( int32_t csocket , struct sockaddr_in addr_in )
{
    bool success = true;

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
        syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u ", __FUNCTION__, __LINE__, sendBytes, MSG_SIZE);   
        success = false;   
    }
    else
    {
        //printf("UDP_OPEN TRUE! \n");
    }

    //printf("UDP_OPEN SIZE: %d\n" , sendBytes);

    return success;
}


bool process_sysInit( int32_t csocket )
{
    bool gotMsg = false;

    enum sysinit_params
    {
        CMD_ID                  = 2,
        LENGTH                  = 2,
        MSG_SIZE                =   CMD_ID +
                                    LENGTH,
    };

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
    while ( true != gotMsg )
    {
        retPoll = poll(myPoll, 1, -1);
        if ( retPoll == -1 ) 
        {
            syslog(LOG_ERR, "%s:%d ERROR! error from pollfd poll()",__FUNCTION__, __LINE__);
            gotMsg = false;
        }
        else if (retPoll == 0 ) 
        {
            gotMsg = false;
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
            gotMsg = false;
            if ( (MSG_SIZE != retBytes) )
            {
                syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u", __FUNCTION__, __LINE__, retBytes, MSG_SIZE);
            }
            if ( ( CMD_SYSINIT != command ) )
            {
                syslog(LOG_ERR, "%s:%d ERROR! invalid command %u", __FUNCTION__, __LINE__, command);
            }
            if ( ( calcLength != actualLength ) )
            {
                syslog(LOG_ERR, "%s:%d ERROR! payload not equal to expected number of bytes %u != %u ",__FUNCTION__, __LINE__, calcLength, actualLength);
            }
            if ( (retPoll <= 0) )
            {
                syslog(LOG_ERR, "%s:%d ERROR! error getting message %u ",__FUNCTION__, __LINE__, retPoll);
            }
        }
        else
        {
            gotMsg = true;
            //printf("SYS_INIT TRUE\n");
        }
        //printf("SYS_INIT SIZE: %d\n\n", retBytes);
    }
    return gotMsg;
}


// The MPs are hardcoded.  That is, they are not based on a subscription.  There is a block of comments (sys requirements) 
// pertaining to the validity of the MPs being published; however, that is only contingent on recceiving a subscription.
// 
// Not sure the best way to check for valid MPs being published without a subscription.    
void process_publish( int32_t csocket , struct sockaddr_in addr_in , int32_t LogMPs[] , int32_t time_secMP[] , int32_t time_nsecMP[], int32_t topic_to_pub )
{
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
    uint8_t *ptr , *msgLenPtr, *val_ptr;
    uint8_t sendData[ MAXBUFSIZE ];
    uint16_t actualLength;
    socklen_t toSendUDP_size;

    int32_t sendBytes   = 0;
    uint32_t i          = 0; 
    uint32_t k          = 0;
    uint32_t cntTS      = 0;
    uint32_t cntBytes   = 0;
    int32_t cnt_true    = 0;
    int32_t cnt_false   = 0;

    ptr = sendData;
    remote_set16( ptr , CMD_PUBLISH);
    ptr += CMD_ID;
    cntBytes += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;
    cntBytes += LENGTH;

    // this will change ...
    remote_set32( ptr , publishMe[ topic_to_pub ].topic_id);
    ptr += TOPIC_ID;
    cntBytes += TOPIC_ID;

    // this will change ...
    remote_set32( ptr , publishMe[ topic_to_pub ].numMPs);
    ptr += NUM_MPS;
    cntBytes += NUM_MPS;

    // ???
    remote_set16( ptr , 0);
    ptr += SEQ_NUM;
    cntBytes += SEQ_NUM;

    // published based on subscription (to include valid/invalid mps) 
    for( i = 0 ; i < publishMe[ topic_to_pub ].numMPs ; i++ )
    {
        // logicals ...
        if (true == publishMe[ topic_to_pub ].topicSubscription[i].logical)
        {
            remote_set32( ptr , publishMe[ topic_to_pub ].topicSubscription[i].mp);
            ptr += MP;
            cntBytes += MP;
            // for loop on num_samples and fill with one of the global MPs that Travis put together ... 
            // pfp_values
            // ptlt_values
            // ptrt_values
            // tcmp_values
            // cop_values

            remote_set32( ptr , ( LogMPs[ publishMe[ topic_to_pub ].topicSubscription[i].mp - 1000 ] ));
            ptr += MP_VAL;
            cntBytes += MP_VAL;
        }
        // time stamps ...
        else
        {
            remote_set32( ptr , publishMe[ topic_to_pub ].topicSubscription[i].mp );
            ptr += MP;
            cntBytes += MP;
            if ( MP_CAM_SEC_1 == publishMe[ topic_to_pub ].topicSubscription[i].mp )
            {
                for( k = 0 ; (k < 9) && (time_secMP[k] != 0) ; k++ )
                {
                    remote_set32( ptr , time_secMP[k]);
                    ptr += MP_VAL;
                    cntBytes += MP_VAL;
                }
            }
            else if ( MP_CAM_NSEC_1 == publishMe[ topic_to_pub ].topicSubscription[i].mp )
            {
                for( k = 0 ; (k < 9) && (time_nsecMP[k] != 0) ; k++ )
                {
                    remote_set32( ptr , time_nsecMP[k]);
                    ptr += MP_VAL;
                    cntBytes += MP_VAL;
                }
            }
        }
    }

    *ptr = cntBytes;
    ptr += cntBytes;

    // actual length
    actualLength = cntBytes - CMD_ID - LENGTH;
    memcpy( msgLenPtr , &actualLength , sizeof(uint16_t) );

    toSendUDP_size = sizeof(addr_in);
    sendBytes = sendto(csocket, sendData, cntBytes, 0, (struct sockaddr *)&addr_in, toSendUDP_size);
    //sendBytes = send( csocket , sendData , msgByteCnt , 0 );

    if ( cntBytes != sendBytes )
    {
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u", __FUNCTION__, __LINE__, sendBytes, cntBytes);
    }
    else
    {
        success = true;
        //printf("PUBLISH TRUE\n");
    }
    //printf( "PUBLISH SIZE: %d\n" , cntBytes );
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

    uint32_t retBytes, i, errCnt, actualLength, calcLength, calcMPs;
    uint8_t retData[ MAXBUFSIZE ];
    uint8_t *ptr;

    uint8_t host_os;
    uint16_t seq_num;
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
        syslog(LOG_ERR, "%s:%d ERROR! error from pollfd poll()",__FUNCTION__, __LINE__);
        success = false;
    }
    else if (retPoll == 0 ) 
    {
        syslog(LOG_ERR, "%s:%d ERROR! timeout error from pollfd poll()",__FUNCTION__, __LINE__);
        success = false;
    } 
    else
    {
        if (myPoll[0].revents & POLLIN)
        {
            toRcvUDP_size = sizeof(toRcvUDP);
            retBytes = recv(csocket , retData , MAXBUFSIZE , 0 );

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

            src_app_name = remote_get32(ptr);   // need it
            ptr += SRC_APP_NAME;
            cnt_retBytes += SRC_APP_NAME;

            num_mps = remote_get32(ptr);        // need it
            ptr += NUM_MPS;
            cnt_retBytes += NUM_MPS;

            seq_num = remote_get16(ptr);
            ptr += SEQ_NUM;
            cnt_retBytes += SEQ_NUM;

            calcMPs = ( actualLength - 15 ) / 12;
            if ( calcMPs != num_mps )
            {
                MPnum = 23;
            }
            else
            {
                MPnum = num_mps;
            }
//          sub_mp = realloc(sub_mp, sizeof(sub_mp)*num_mps);
//          sub_mpPer = realloc(sub_mpPer, sizeof(sub_mpPer)*num_mps);
//          sub_mpNumSamples = realloc(sub_mpNumSamples, sizeof(sub_mpNumSamples)*num_mps);
            sub_mp = realloc(sub_mp, sizeof(sub_mp)*MPnum);
            sub_mpPer = realloc(sub_mpPer, sizeof(sub_mpPer)*MPnum);
            sub_mpNumSamples = realloc(sub_mpNumSamples, sizeof(sub_mpNumSamples)*MPnum);

            //calcMPs = ( actualLength - 15 ) / 12;
            //if (calcMPs != num_mps)

            for( i = 0 ; i < MPnum ; i++ )
            {
                sub_mp[i] = remote_get32(ptr);
                ptr += MP;
                cnt_retBytes += MP;

                sub_mpPer[i] = remote_get32(ptr);
                ptr += MP_PER;
                cnt_retBytes += MP_PER;

                sub_mpNumSamples[i] = remote_get32(ptr);
                ptr += MP_NUM_SAMPLES;
                cnt_retBytes += MP_NUM_SAMPLES;
            }
            calcLength = cnt_retBytes - CMD_ID - LENGTH;
        }
    }

    if ( (retPoll <= 0) || (cnt_retBytes != retBytes) || ( CMD_SUBSCRIBE != command ) || ( calcLength != actualLength ) || (calcMPs != num_mps) )
    {
        success = false;
        if ( (cnt_retBytes != retBytes) )
        {
            syslog(LOG_ERR, "%s:%d ERROR! insufficient message data retBytes %u != cnt_retBytes %u", __FUNCTION__, __LINE__, retBytes, cnt_retBytes);
        }
        if ( ( CMD_SUBSCRIBE != command ) )
        {
            // not sure if we really need to syslog this condition.  There will be a lot.
            syslog(LOG_ERR, "%s:%d ERROR! invalid command %u", __FUNCTION__, __LINE__, command);
        }
        if ( ( calcLength != actualLength ) )
        {
            syslog(LOG_ERR, "%s:%d payload not equal to expected number of bytes calcLength %u != actualLength %u ",__FUNCTION__, __LINE__, calcLength, actualLength);
        }
        if ( (retPoll <= 0) )
        {
            syslog(LOG_ERR, "%s:%d ERROR! error getting message, retPoll %u ",__FUNCTION__, __LINE__, retPoll);
        }

        if ( (calcMPs != num_mps) )
        {
            syslog(LOG_ERR, "%s:%d ERROR! length does not coincide with number of MPs: calcMPs %u != num_mps %u", __FUNCTION__, __LINE__, calcMPs, num_mps);
        }
    }
    else
    {
        success = true;
        //printf("SUBSCRIBE TRUE\n");
    }
    //printf("SUBSCRIBE SIZE: %d\n\n", retBytes);
    return success;
}


//bool process_subscribe_ack( int32_t csocket )
bool process_subscribe_ack( int32_t csocket , struct sockaddr_in addr_in )
{
    bool success = true;

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

    uint8_t *ptr , *msgLenPtr, *msgErrPtr, *topicIDptr;
    uint8_t sendData[ MAXBUFSIZE ];
    uint16_t actualLength;
    socklen_t toSendUDP_size;

    int32_t cntBytes    = 0;
    int32_t sendBytes   = 0;
    uint32_t i          = 0;
    uint32_t tID        = 1000;
    int16_t genErr      = GE_SUCCESS;

    ptr = sendData;
    remote_set16( ptr , CMD_SUBSCRIBE_ACK );
    ptr += CMD_ID;
    cntBytes += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;
    cntBytes += LENGTH;

    topicIDptr = ptr;
    ptr += TOPIC_ID;
    cntBytes += TOPIC_ID;

    msgErrPtr = ptr;
    ptr += ERROR;
    cntBytes += ERROR;

    for( i = 0 ; i < publishMe[ currentTopic ].numMPs ; i++ ) 
    {
        if ( true == publishMe[ currentTopic ].topicSubscription[ i ].valid )
        {
            remote_set16( ptr , GE_SUCCESS);
        }
        else
        {
            remote_set16( ptr , GE_INVALID_MP_NUMBER);
            genErr = GE_INVALID_MP_NUMBER;
            success = false;
        }
        ptr += ERROR_MP;
        cntBytes += ERROR_MP;
    }

    *ptr = MSG_SIZE;
    ptr += MSG_SIZE;

    actualLength = cntBytes - CMD_ID - LENGTH;
    
    // why do I get errors when I rearrange these?  e.g. memcpy topic ID first, then length, then error.
    // I get the right stuff when I memcpy in the order they were assigned above ... why?  
    memcpy(msgLenPtr,   &actualLength,                          sizeof(uint16_t));
    memcpy(topicIDptr,  &publishMe[ currentTopic ].topic_id,    sizeof(uint32_t));
    memcpy(msgErrPtr,   &genErr,                                sizeof(int16_t));

    // send
    toSendUDP_size  = sizeof(addr_in);
    sendBytes       = send(csocket, sendData, cntBytes, 0);

    // check message
    if ( (cntBytes != sendBytes) || ( GE_INVALID_MP_NUMBER == genErr ) )
    {
        success = false;
        if ( cntBytes != sendBytes )
        {
            syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u ", __FUNCTION__, __LINE__, sendBytes, cntBytes);
        }

        if ( GE_INVALID_MP_NUMBER == genErr )
        {
            syslog(LOG_ERR, "%s:%d ERROR! MP subscription error: %u", __FUNCTION__, __LINE__, genErr );
        }
    }
    else
    {
        //printf("SUBSCRIBE_ACK TRUE! \n");
    }

    return success;
}

// This set of data (publishMe) is built based on the current subscription.
// After a SUBSCRIBE() and before SUBSCRIBE_ACK(), this function is called.  
bool buildPublishData()
{
    bool success = true;
    bool MPmatches = false;
    int32_t i, k, cnt, periodSumTotal, periodAvg, periodVar, periodChk, numPeriodsMatching, numMPsMatching, numSamplesToChk;

    int32_t SIMMsubscriptionMP[ MAX_SIMM_SUBSCRIPTION ] = 
    {
        MP_PFP_VALUE,
        MP_PTLT_TEMPERATURE,
        MP_PTRT_TEMPERATURE,
        MP_TCMP,
        MP_CAM_SEC_1,
        MP_CAM_NSEC_1,
        MP_CAM_SEC_2,
        MP_CAM_NSEC_2,
        MP_CAM_SEC_3,
        MP_CAM_NSEC_3,
        MP_CAM_SEC_4,
        MP_CAM_NSEC_4,
        MP_CAM_SEC_5,
        MP_CAM_NSEC_5,
        MP_CAM_SEC_6,
        MP_CAM_NSEC_6,
        MP_CAM_SEC_7,
        MP_CAM_NSEC_7,
        MP_CAM_SEC_8,
        MP_CAM_NSEC_8,
        MP_CAM_SEC_9,
        MP_CAM_NSEC_9,
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
        MINPER_CAM_SEC_2,
        MINPER_CAM_NSEC_2,
        MINPER_CAM_SEC_3,
        MINPER_CAM_NSEC_3,
        MINPER_CAM_SEC_4,
        MINPER_CAM_NSEC_4,
        MINPER_CAM_SEC_5,
        MINPER_CAM_NSEC_5,
        MINPER_CAM_SEC_6,
        MINPER_CAM_NSEC_6,
        MINPER_CAM_SEC_7,
        MINPER_CAM_NSEC_7,
        MINPER_CAM_SEC_8,
        MINPER_CAM_NSEC_8,
        MINPER_CAM_SEC_9,
        MINPER_CAM_NSEC_9,
        MINPER_COP_PRESSURE
    };

    publishMe[ currentTopic].app_name  = src_app_name;
    //publishMe[ currentTopic ].numMPs   = num_mps;
    publishMe[ currentTopic ].numMPs   = MPnum;
    
    publishMe = realloc(publishMe, sizeof(topicToPublish)*num_topics_total);
    if (NULL == publishMe)
    {
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! BAD realloc()",__FUNCTION__, __LINE__);
    }

    publishMe[ currentTopic ].topicSubscription = malloc(sizeof(MPinfo)*publishMe[ currentTopic ].numMPs);
    if (NULL == publishMe[ currentTopic ].topicSubscription)
    {
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! BAD malloc()",__FUNCTION__, __LINE__);
    }


    periodSumTotal      = 0;
    numPeriodsMatching  = 0;
    numMPsMatching      = 0;

    for( k = 0 ; k < publishMe[ currentTopic ].numMPs ; k++ )
    {
        publishMe[ currentTopic ].topicSubscription[ k ].mp             = sub_mp[ k ];
        publishMe[ currentTopic ].topicSubscription[ k ].period         = sub_mpPer[ k ];
        publishMe[ currentTopic ].topicSubscription[ k ].numSamples     = sub_mpNumSamples[ k ];

        // determine if MP is logical or timestamp
        if ( (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_PFP_VALUE ) ||
             (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_PTLT_TEMPERATURE ) ||
             (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_PTRT_TEMPERATURE ) ||
             (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_TCMP ) ||
             (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_COP_PRESSURE ) )
        {
            publishMe[ currentTopic ].topicSubscription[ k ].logical = true;
        }
        else
        {
            publishMe[ currentTopic ].topicSubscription[ k ].logical = false;
        }

        numMPsMatching = 0;
        for( i = 0 ; i < MAX_SIMM_SUBSCRIPTION ; i++ )
        {
            // latch
            if( SIMMsubscriptionMP[ i ] == publishMe[ currentTopic ].topicSubscription[ k ].mp )
            {
                numMPsMatching++;
            }
        }

        // checks numer of samples requested based on period requested and whatever the minimum period is configured.  
        if ( ( publishMe[currentTopic].topicSubscription[k].period % MINPER ) == 0 )
        {
            numSamplesToChk = publishMe[currentTopic].topicSubscription[k].numSamples * MINPER;
            if ( numSamplesToChk != publishMe[currentTopic].topicSubscription[k].period )
            {
                syslog(LOG_ERR, "%s:%d INVALID SUBSCRIPTION: MP number of samples doesn't correspond to the period requested",__FUNCTION__, __LINE__);
                numSamplesToChk = 0;
            }
        }
        else
        {
            syslog(LOG_ERR, "%s:%d INVALID SUBSCRIPTION: MP PERIOD not integer multiple of minimum period allowed",__FUNCTION__, __LINE__);
            numSamplesToChk = 0;
        }

        // determines valid and invalid MPs based on (1) if MP is schedulable and (2) number of samples and period
        if ( ( 0 == numMPsMatching ) || ( 0 == numSamplesToChk ) )   
        {
            publishMe[ currentTopic ].topicSubscription[ k ].valid = false;
        }
        else
        {
            publishMe[ currentTopic ].topicSubscription[ k ].valid = true;
        }
    }

    periodVar = getVarPeriod();
    
    for( i = 0 ; i < publishMe[currentTopic].numMPs ; i++ )
    {
        if ( (0 == periodVar) ) // all periods the same?
        {
            publishMe[ currentTopic ].period = publishMe[ currentTopic ].topicSubscription[ 0 ].period;
            publishMe[ currentTopic ].topic_id  = 1000 + currentTopic; // + getTopicId( publishMe[ currentTopic ].app_name );

            // determine max publish period
            if (publishMe[ currentTopic ].topicSubscription[ 0 ].period > prevPeriodChk)
            {
                maxPublishPeriod = publishMe[ currentTopic ].topicSubscription[ 0 ].period;
            }

            // set all 1 sec publish periods to true for "ready to publish"
            if (1000 == publishMe[ currentTopic ].topicSubscription[ 0 ].period )
            {
                publishMe[ currentTopic ].publishReady = true;
            }
            else
            {
                publishMe[ currentTopic ].publishReady = false;
            }
        }
        else
        {
            publishMe[ currentTopic ].topicSubscription[ i ].valid = false;
            syslog(LOG_ERR, "%s:%d INVALID SUBSCRIPTION",__FUNCTION__, __LINE__);
            //success = false;
            publishMe[ currentTopic ].period    = 0;
            publishMe[ currentTopic ].topic_id  = -1;
        }
    }
   
    prevPeriodChk  = publishMe[ currentTopic ].topicSubscription[ 0 ].period;

    return success;
}

int32_t getVarPeriod(void)
{
    int32_t i, avg, sum, var, diff, sq_diff;

    avg = 0;
    sum = 0;
    var = 0;
    diff = 0;
    sq_diff = 0;

    for( i = 0 ; i < publishMe[currentTopic].numMPs ; i++ )
    {
        sum = sum + publishMe[ currentTopic ].topicSubscription[ i ].period;
    }

    avg = sum / publishMe[currentTopic].numMPs;

    for( i = 0 ; i < publishMe[currentTopic].numMPs ; i++ )
    {
        diff    = publishMe[ currentTopic ].topicSubscription[ i ].period - avg;
        sq_diff = diff * diff;
        var     = var + sq_diff;
    }

    var = var / ( publishMe[currentTopic].numMPs - 1 );
    return var;
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


// true = seconds (numSeconds) have elapsed
bool numSecondsHaveElapsed( struct timespec startTime , struct timespec stopTime , int32_t numSeconds )
//bool check_elapsedTime( struct timespec startTime , struct timespec stopTime , int32_t timeToChk )
{
    bool success = false;
    if ( (stopTime.tv_sec - startTime.tv_sec) < numSeconds )              
    {
        success = false;
    }
    else if ((stopTime.tv_sec - startTime.tv_sec) == numSeconds ) 
    {
        if (stopTime.tv_nsec < startTime.tv_nsec)  
        {
            success = false;
        }
        else
        {
            success = true;
        }
    }
    else
    {
        success = true;
    }
    return success;
}

int32_t getTopicId(uint32_t subAppName)
{
    // for this phase, there's just one topic.  
    int32_t i, new_offset;
    bool chkApp = false;

    // check if app name exists
    for( i = 0 ; i < num_topics_total ; i++ ) 
    {
        if ( subAppName == publishMe[i].app_name )
        {
            chkApp = true;
            new_offset = i;
            break;
        }
        else
        {
            chkApp = false;
        }
    }

    // if the app does not exist, realloc space for new id
    if (false == chkApp)
    {
        //publishMe[num_topics_total].topic_id = 1000 + num_topics_total;
        new_offset = num_topics_total - 1;
    }

    return new_offset;
} 


int32_t publishManager()
{
    // determines next group of subscriptions to publish based on next possible time to publish (every second)
    // publishMe holds the subscription data to publish

    int32_t numToPub = 0;
    int32_t i;

    // only happens once.  
    if ( 0 == nextPublishPeriod )
    {
        nextPublishPeriod = 1000;
    }

    

    for( i = 0 ; i < num_topics_total ; i++ )
    {
        if ( ( nextPublishPeriod % publishMe[i].period ) == 0 )
        {
            publishMe[i].publishReady = true;
            numToPub++;
        }
        else
        {
            publishMe[i].publishReady = false;
        }
    }

//  if (maxPublishPeriod == nextPublishPeriod)
//  {
//      nextPublishPeriod = 1000;
//  }

    return numToPub;
}
