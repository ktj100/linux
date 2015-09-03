
/** @file fdl_functions.c
 * Functions used send, receive, and package fdl/FPGA data.
 * Enumerations configured per API ICD document.  
 *
 * Copyright (c) 2010, DornerWorks, Ltd.
 */

/****************
* INCLUDES
****************/
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
#include <math.h>
#include "fdl.h"


/****************
* GLOBALS
****************/
#define MAXBUFSIZE  1000

extern int32_t clientSocket;
struct sockaddr_storage serverStorage_UDP;
struct sockaddr_storage toRcvUDP;
struct sockaddr_storage toSendUDP;

int32_t num_mps; 
int32_t num_mps_fromPub;
int32_t MPnum;
int32_t *sub_mp;
int32_t *sub_mpPer;
uint32_t *sub_mpNumSamples;
int32_t src_app_name;

int32_t num_topics_total;
uint32_t num_topics_atCurrentRate;
uint32_t currentTopic;
int32_t maxPublishPeriod; 
int32_t prevPeriodChk;
int32_t nextPublishPeriod;
int32_t fromSubAckTopicID;


/**
 * Used to package data to be sent for registering the
 * application.
 *
 * @param[in] csocket TCP socket
 * @param[in] goTime Used to check 1 second requirement (must be
 *       sent within 1 second of establishing TCP connection)
 * @param[out] success true/false status of sending message
 *
 * @return true/false status of sending message
 */
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

    //bool lessOneSecond = true;
    int32_t sendBytes = 0;
    uint8_t *ptr;
    int16_t val16;
    int32_t val32; 
    uint8_t *msgLenPtr = 0;
    uint32_t actualLength = 0; 
    uint32_t secsToChk = 0;
    uint8_t sendData[ MSG_SIZE ];
    struct timespec endTime;
    
    ptr = sendData;
    val16 = CMD_REGISTER_APP;
    memcpy(ptr, &val16, sizeof(val16));
    //remote_set16( ptr , CMD_REGISTER_APP);
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    *ptr = PETALINUX;
    ptr += HOST_OS;

    val32 = getpid();
    memcpy(ptr, &val32, sizeof(val32));
    //remote_set32( ptr , getpid() );
    ptr += SRC_PROC_ID;

    val32 = 15137;
    memcpy(ptr, &val32, sizeof(val32));
    //remote_set32( ptr , 0);
    ptr += SRC_APP_NAME;

//  *ptr = MSG_SIZE;
//  ptr += MSG_SIZE;

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
        //printf("ELAPSED TIME GREATER THAN 1 SECOND!\n");
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


/**
 * Used to package data upon receiving acknowledgment that
 * application was registered.
 *
 * @param[in] csocket TCP socket
 * @param[out] success true/false status of received message
 *
 * @return true/false status of received message
 */
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

    uint16_t command = 0;
    uint32_t retBytes = 0; 
    uint8_t retData[ MSG_SIZE ];    // or whatever max is defined
    uint16_t appAckErr = 0;
    uint8_t *ptr;
    //int16_t val16;
    int32_t actualLength = 0;
    struct pollfd myPoll[1];
    int32_t retPoll = 0;

    int32_t calcLength = 0;


    myPoll[0].fd = csocket;
    myPoll[0].events = POLLIN; 


//  struct timeval tv;
//
//  // add timeout of ? 3 seconds here
//  tv.tv_sec = 3;
//  tv.tv_usec = 0;

    // WAIT TO RECEIVE RESPONSE ...
    retPoll = poll( myPoll, 1, -1 );
    if ( -1 == retPoll ) 
    {   
        syslog(LOG_ERR, "%s:%d ERROR! error from pollfd poll()",__FUNCTION__, __LINE__);
        //printf("ERROR WITH SELECT\n");
    }
    else if ( 0 == retPoll ) 
    {
        syslog(LOG_ERR, "%s:%d ERROR! timeout error from pollfd poll()",__FUNCTION__, __LINE__);
        //printf("SELECT TIMEOUT ERROR\n");
    } 
    else
    {
        if (myPoll[0].revents & POLLIN)
        {
            retBytes = recv(csocket, retData , MSG_SIZE , 0 );

            //command = remote_get16(retData);
            ptr = retData;
            memcpy(&command, ptr, sizeof(command));
            ptr += CMD_ID;

            memcpy(&actualLength, ptr, sizeof(actualLength));
            //actualLength = remote_get16(ptr);
            ptr += LENGTH;

            memcpy(&appAckErr, ptr, sizeof(appAckErr));
            //appAckErr = remote_get16(ptr);
            ptr += ERROR;

            calcLength = MSG_SIZE - CMD_ID - LENGTH;
        }
    }

    if ( (retPoll <= 0) || (retBytes != MSG_SIZE) || ( appAckErr != 0 ) || (CMD_REGISTER_APP_ACK != command) || (calcLength != actualLength) )
    {
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! register_app_ack error",__FUNCTION__, __LINE__);
        //printf("REGISTER APP ACK ERROR \n");
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

/**
 * Used to package data to be sent for registering the data that
 * can subscribed for.
 *
 * @param[in] csocket TCP socket
 * @param[out] success true/false status of sending message
 *
 * @return true/false status of sending message
 */
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
//      PFP_VALUE               = 4,
//      PTLT_TEMPERATURE        = 4,
//      PTRT_TEMPERATURE        = 4,
//      TCMP                    = 4,
//      COP_PRESSURE            = 4,
        //COP_FO_REAL             = 4,
        //COP_FO_IMAG             = 4,
        COP_FO_AMPLITUDE        = 4,
        COP_FO_ENERGY           = 4,
        COP_FO_PHASE            = 4,
        //COP_HO_REAL             = 4,
        //COP_HO_IMAG             = 4,
        COP_HO_AMPLITUDE        = 4,
        COP_HO_ENERGY           = 4,
        COP_HO_PHASE            = 4,
        //CRANK_FO_REAL           = 4,
        //CRANK_FO_IMAG           = 4,
        CRANK_FO_AMPLITUDE      = 4,
        CRANK_FO_ENERGY         = 4,
        CRANK_FO_PHASE          = 4,
        //CRANK_HO_REAL           = 4,
        //CRANK_HO_IMAG           = 4,
        CRANK_HO_AMPLITUDE      = 4,
        CRANK_HO_ENERGY         = 4,
        CRANK_HO_PHASE          = 4,
//      CAM_SEC_1               = 4,
//      CAM_NSEC_1              = 4,
//      CAM_SEC_2               = 4,
//      CAM_NSEC_2              = 4,
//      CAM_SEC_3               = 4,
//      CAM_NSEC_3              = 4,
//      CAM_SEC_4               = 4,
//      CAM_NSEC_4              = 4,
//      CAM_SEC_5               = 4,
//      CAM_NSEC_5              = 4,
//      CAM_SEC_6               = 4,
//      CAM_NSEC_6              = 4,
//      CAM_SEC_7               = 4,
//      CAM_NSEC_7              = 4,
//      CAM_SEC_8               = 4,
//      CAM_NSEC_8              = 4,
//      CAM_SEC_9               = 4,
//      CAM_NSEC_9              = 4,
        //TURBO_REAL              = 4,
        //TURBO_IMAG              = 4,
        TURBO_FO_AMPLITUDE      = 4,
        TURBO_FO_ENERGY         = 4,
        MSG_SIZE                =   CMD_ID + 
                                    LENGTH + 
                                    HOST_OS +
                                    SRC_PROC_ID +
                                    SRC_APP_NAME +
                                    NUM_MPS +
//                                  PFP_VALUE +
//                                  PTLT_TEMPERATURE +
//                                  PTRT_TEMPERATURE +
//                                  TCMP +
//                                  COP_PRESSURE +
                                    //COP_FO_REAL +
                                    //COP_FO_IMAG +
                                    COP_FO_AMPLITUDE +
                                    COP_FO_ENERGY +
                                    COP_FO_PHASE +
                                    //COP_HO_REAL +
                                    //COP_HO_IMAG +
                                    COP_HO_AMPLITUDE +
                                    COP_HO_ENERGY +
                                    COP_HO_PHASE +
                                    //CRANK_FO_REAL +
                                    //CRANK_FO_IMAG +
                                    CRANK_FO_AMPLITUDE +
                                    CRANK_FO_ENERGY +
                                    CRANK_FO_PHASE +
                                    //CRANK_HO_REAL +
                                    //CRANK_HO_IMAG +
                                    CRANK_HO_AMPLITUDE +
                                    CRANK_HO_ENERGY +
                                    CRANK_HO_PHASE +
//                                  CAM_SEC_1  +
//                                  CAM_NSEC_1 +
//                                  CAM_SEC_2  +
//                                  CAM_NSEC_2 +
//                                  CAM_SEC_3  +
//                                  CAM_NSEC_3 +
//                                  CAM_SEC_4  +
//                                  CAM_NSEC_4 +
//                                  CAM_SEC_5  +
//                                  CAM_NSEC_5 +
//                                  CAM_SEC_6  +
//                                  CAM_NSEC_6 +
//                                  CAM_SEC_7  +
//                                  CAM_NSEC_7 +
//                                  CAM_SEC_8  +
//                                  CAM_NSEC_8 +
//                                  CAM_SEC_9  +
//                                  CAM_NSEC_9,
                                    //TURBO_REAL +
                                    //TURBO_IMAG +
                                    TURBO_FO_AMPLITUDE +
                                    TURBO_FO_ENERGY,
    };

    bool success = true;
    int32_t sendBytes = 0;
    uint8_t *ptr;
    int16_t val16;
    int32_t val32;
    uint8_t *msgLenPtr;
    uint32_t actualLength = 0;
    uint8_t sendData[ MSG_SIZE ];
    //int32_t test_val = 0;

    ptr = sendData;
    val16 = CMD_REGISTER_DATA;
    memcpy(ptr, &val16, sizeof(val16));
    //remote_set16( ptr , CMD_REGISTER_DATA);
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    *ptr = PETALINUX;
    ptr += HOST_OS;

    val32 = getpid();
    memcpy(ptr, &val32, sizeof(val32));
    //remote_set32( ptr , getpid() );
    ptr += SRC_PROC_ID;

    val32 = 15137;
    memcpy(ptr, &val32, sizeof(val32));
    //remote_set32( ptr , 1);
    ptr += SRC_APP_NAME;

    val32 = MAX_fdl_TO_PUBLISH;
    memcpy(ptr, &val32, sizeof(val32));
    //remote_set32( ptr , MAX_fdl_TO_PUBLISH);
    ptr += NUM_MPS;

//  val32 = MP_PFP_VALUE;
//  memcpy(ptr, &val32, sizeof(val32));
//  //remote_set32( ptr , MP_PFP_VALUE);
//  ptr += PFP_VALUE;
//
//  val32 = MP_PTLT_TEMPERATURE;
//  memcpy(ptr, &val32, sizeof(val32));
//  //remote_set32( ptr , MP_PTLT_TEMPERATURE);
//  ptr += PTLT_TEMPERATURE;
//
//  val32 = MP_PTRT_TEMPERATURE;
//  memcpy(ptr, &val32, sizeof(val32));
//  //remote_set32( ptr , MP_PTRT_TEMPERATURE);
//  ptr += PTRT_TEMPERATURE;
//
//  val32 = MP_TCMP;
//  memcpy(ptr, &val32, sizeof(val32));
//  //remote_set32( ptr , MP_TCMP);
//  ptr += TCMP;
//
//  val32 = MP_COP_PRESSURE;
//  memcpy(ptr, &val32, sizeof(val32));
//  //remote_set32( ptr , MP_COP_PRESSURE);
//  ptr += COP_PRESSURE;

    //remote_set32( ptr , MP_COP_FO_REAL);
    //ptr += COP_FO_REAL;

    //remote_set32( ptr , MP_COP_FO_IMAG);
    //ptr += COP_FO_IMAG;

    remote_set32( ptr , MP_COP_HALFORDER_AMPLITUDE);
    ptr += COP_HO_AMPLITUDE;

    remote_set32( ptr , MP_COP_HALFORDER_ENERGY);
    ptr += COP_HO_ENERGY;

    remote_set32( ptr , MP_COP_HALFORDER_PHASE);
    ptr += COP_HO_PHASE;

    remote_set32( ptr , MP_COP_FIRSTORDER_AMPLITUDE);
    ptr += COP_FO_AMPLITUDE;

    remote_set32( ptr , MP_COP_FIRSTORDER_ENERGY);
    ptr += COP_FO_ENERGY;

    remote_set32( ptr , MP_COP_FIRSTORDER_PHASE);
    ptr += COP_FO_PHASE;



    //remote_set32( ptr , MP_COP_HO_REAL);
    //ptr += COP_HO_REAL;

    //remote_set32( ptr , MP_COP_HO_IMAG);
    //ptr += COP_HO_IMAG;



    //remote_set32( ptr , MP_CRANK_FO_REAL);
    //ptr += CRANK_FO_REAL;

    //remote_set32( ptr , MP_CRANK_FO_IMAG);
    //ptr += CRANK_FO_IMAG;

    remote_set32( ptr , MP_CRANK_HALFORDER_AMPLITUDE);
    ptr += CRANK_HO_AMPLITUDE;

    remote_set32( ptr , MP_CRANK_HALFORDER_ENERGY);
    ptr += CRANK_HO_ENERGY;

    remote_set32( ptr , MP_CRANK_HALFORDER_PHASE);
    ptr += CRANK_HO_PHASE;

    remote_set32( ptr , MP_CRANK_FIRSTORDER_AMPLITUDE);
    ptr += CRANK_FO_AMPLITUDE;

    remote_set32( ptr , MP_CRANK_FIRSTORDER_ENERGY);
    ptr += CRANK_FO_ENERGY;

    remote_set32( ptr , MP_CRANK_FIRSTORDER_PHASE);
    ptr += CRANK_FO_PHASE;



    //remote_set32( ptr , MP_CRANK_HO_REAL);
    //ptr += CRANK_HO_REAL;

    //remote_set32( ptr , MP_CRANK_HO_IMAG);
    //ptr += CRANK_HO_IMAG;



//  val32 = MP_CAM_SEC_1;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_SEC_1;
//
//  val32 = MP_CAM_NSEC_1;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_NSEC_1;
//
//  val32 = MP_CAM_SEC_2;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_SEC_2;
//
//  val32 = MP_CAM_NSEC_2;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_NSEC_2;
//
//  val32 = MP_CAM_SEC_3;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_SEC_3;
//
//  val32 = MP_CAM_NSEC_3;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_NSEC_3;
//
//  val32 = MP_CAM_SEC_4;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_SEC_4;
//
//  val32 = MP_CAM_NSEC_4;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_NSEC_4;
//
//  val32 = MP_CAM_SEC_5;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_SEC_5;
//
//  val32 = MP_CAM_NSEC_5;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_NSEC_5;
//
//  val32 = MP_CAM_SEC_6;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_SEC_6;
//
//  val32 = MP_CAM_NSEC_6;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_NSEC_6;
//
//  val32 = MP_CAM_SEC_7;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_SEC_7;
//
//  val32 = MP_CAM_NSEC_7;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_NSEC_7;
//
//  val32 = MP_CAM_SEC_8;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_SEC_8;
//
//  val32 = MP_CAM_NSEC_8;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_NSEC_8;
//
//  val32 = MP_CAM_SEC_9;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_SEC_9;
//
//  val32 = MP_CAM_NSEC_9;
//  memcpy(ptr, &val32, sizeof(val32));
//  ptr += CAM_NSEC_9;

//  remote_set32( ptr , MP_CAM_SEC_1);
//  ptr += CAM_SEC_1;
//  remote_set32( ptr , MP_CAM_NSEC_1);
//  ptr += CAM_NSEC_1;
//  remote_set32( ptr , MP_CAM_SEC_2);
//  ptr += CAM_SEC_2;
//  remote_set32( ptr , MP_CAM_NSEC_2);
//  ptr += CAM_NSEC_2;
//  remote_set32( ptr , MP_CAM_SEC_3);
//  ptr += CAM_SEC_3;
//  remote_set32( ptr , MP_CAM_NSEC_3);
//  ptr += CAM_NSEC_3;
//  remote_set32( ptr , MP_CAM_SEC_4);
//  ptr += CAM_SEC_4;
//  remote_set32( ptr , MP_CAM_NSEC_4);
//  ptr += CAM_NSEC_4;
//  remote_set32( ptr , MP_CAM_SEC_5);
//  ptr += CAM_SEC_5;
//  remote_set32( ptr , MP_CAM_NSEC_5);
//  ptr += CAM_NSEC_5;
//  remote_set32( ptr , MP_CAM_SEC_6);
//  ptr += CAM_SEC_6;
//  remote_set32( ptr , MP_CAM_NSEC_6);
//  ptr += CAM_NSEC_6;
//  remote_set32( ptr , MP_CAM_SEC_7);
//  ptr += CAM_SEC_7;
//  remote_set32( ptr , MP_CAM_NSEC_7);
//  ptr += CAM_NSEC_7;
//  remote_set32( ptr , MP_CAM_SEC_8);
//  ptr += CAM_SEC_8;
//  remote_set32( ptr , MP_CAM_NSEC_8);
//  ptr += CAM_NSEC_8;
//  remote_set32( ptr , MP_CAM_SEC_9);
//  ptr += CAM_SEC_9;
//  remote_set32( ptr , MP_CAM_NSEC_9);
//  ptr += CAM_NSEC_9;
    
    //remote_set32( ptr , MP_TURBO_REAL);
    //ptr += TURBO_REAL;

    //remote_set32( ptr , MP_TURBO_IMAG);
    //ptr += TURBO_IMAG;

    remote_set32( ptr , MP_TURBO_OIL_FIRSTORDER_AMPLITUDE);
    ptr += TURBO_FO_AMPLITUDE;

    remote_set32( ptr , MP_TURBO_OIL_FIRSTORDER_ENERGY);
    ptr += TURBO_FO_ENERGY;

    //  *ptr = 23;
    //  ptr += MSG_SIZE;

    // actual length
    actualLength = MSG_SIZE - CMD_ID - LENGTH;
    memcpy(msgLenPtr, &actualLength, sizeof(actualLength));

    // sending
    sendBytes = send( csocket, sendData, MSG_SIZE, 0 );

    if (MSG_SIZE != sendBytes)
    {
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u",__FUNCTION__, __LINE__, sendBytes, MSG_SIZE);
        //printf("REGISTER_DATA ERROR\n");
    }
    else
    {
        success = true;
        //printf("REGISTER_DATA TRUE\n");
    }
    //printf("REGISTER_DATA SIZE: %d\n",sendBytes);

    return success;
}

/**
 * Used to package data upon receiving acknowledgment that
 * data was registered.
 *
 * @param[in] csocket TCP socket
 * @param[out] success true/false status of received message
 *
 * @return true/false status of received message
 */
bool process_registerData_ack(int32_t csocket )
{
    enum registerApp_ack_params
    {
        CMD_ID                      = 2,
        LENGTH                      = 2,
        ERROR                       = 2,
//      ERROR_PFP_VALUE             = 2,
//      ERROR_PTLT_TEMPERATURE      = 2,
//      ERROR_PTRT_TEMPERATURE      = 2,
//      ERROR_TCMP                  = 2,
//      ERROR_COP_PRESSURE          = 2,
        //ERROR_COP_FO_REAL           = 2,
        //ERROR_COP_FO_IMAG           = 2,
        ERROR_COP_FO_AMPLITUDE      = 2,
        ERROR_COP_FO_ENERGY         = 2,
        ERROR_COP_FO_PHASE          = 2,
        //ERROR_COP_HO_REAL           = 2,
        //ERROR_COP_HO_IMAG           = 2,
        ERROR_COP_HO_AMPLITUDE      = 2,
        ERROR_COP_HO_ENERGY         = 2,
        ERROR_COP_HO_PHASE          = 2,
        //ERROR_CRANK_FO_REAL         = 2,
        //ERROR_CRANK_FO_IMAG         = 2,
        ERROR_CRANK_FO_AMPLITUDE    = 2,
        ERROR_CRANK_FO_ENERGY       = 2,
        ERROR_CRANK_FO_PHASE        = 2,
        //ERROR_CRANK_HO_REAL         = 2,
        //ERROR_CRANK_HO_IMAG         = 2,
        ERROR_CRANK_HO_AMPLITUDE    = 2,
        ERROR_CRANK_HO_ENERGY       = 2,
        ERROR_CRANK_HO_PHASE        = 2,
//      ERROR_CAM_SEC_1         = 2,
//      ERROR_CAM_NSEC_1        = 2,
//      ERROR_CAM_SEC_2         = 2,
//      ERROR_CAM_NSEC_2        = 2,
//      ERROR_CAM_SEC_3         = 2,
//      ERROR_CAM_NSEC_3        = 2,
//      ERROR_CAM_SEC_4         = 2,
//      ERROR_CAM_NSEC_4        = 2,
//      ERROR_CAM_SEC_5         = 2,
//      ERROR_CAM_NSEC_5        = 2,
//      ERROR_CAM_SEC_6         = 2,
//      ERROR_CAM_NSEC_6        = 2,
//      ERROR_CAM_SEC_7         = 2,
//      ERROR_CAM_NSEC_7        = 2,
//      ERROR_CAM_SEC_8         = 2,
//      ERROR_CAM_NSEC_8        = 2,
//      ERROR_CAM_SEC_9         = 2,
//      ERROR_CAM_NSEC_9        = 2,
        //ERROR_TURBO_REAL            = 2,
        //ERROR_TURBO_IMAG            = 2,
        ERROR_TURBO_AMPLITUDE       = 2,
        ERROR_TURBO_ENERGY          = 2,
        MSG_SIZE                =   CMD_ID                      + 
                                    LENGTH                      + 
                                    ERROR                       +
//                                    ERROR_PFP_VALUE +
//                                    ERROR_PTLT_TEMPERATURE +
//                                    ERROR_PTRT_TEMPERATURE +
//                                    ERROR_TCMP +
//                                    ERROR_COP_PRESSURE +
                                    //ERROR_COP_FO_REAL           +
                                    //ERROR_COP_FO_IMAG           +
                                    ERROR_COP_FO_AMPLITUDE      +
                                    ERROR_COP_FO_ENERGY         +
                                    ERROR_COP_FO_PHASE          +
                                    //ERROR_COP_HO_REAL           +
                                    //ERROR_COP_HO_IMAG           +
                                    ERROR_COP_HO_AMPLITUDE      +
                                    ERROR_COP_HO_ENERGY         +
                                    ERROR_COP_HO_PHASE          +
                                    //ERROR_CRANK_FO_REAL         +
                                    //ERROR_CRANK_FO_IMAG         +
                                    ERROR_CRANK_FO_AMPLITUDE    +
                                    ERROR_CRANK_FO_ENERGY       +
                                    ERROR_CRANK_FO_PHASE        +
                                    //ERROR_CRANK_HO_REAL         +
                                    //ERROR_CRANK_HO_IMAG         +
                                    ERROR_CRANK_HO_AMPLITUDE    +
                                    ERROR_CRANK_HO_ENERGY       +
                                    ERROR_CRANK_HO_PHASE        +
//                                    ERROR_CAM_SEC_1  +
//                                    ERROR_CAM_NSEC_1 +
//                                    ERROR_CAM_SEC_2  +
//                                    ERROR_CAM_NSEC_2 +
//                                    ERROR_CAM_SEC_3  +
//                                    ERROR_CAM_NSEC_3 +
//                                    ERROR_CAM_SEC_4  +
//                                    ERROR_CAM_NSEC_4 +
//                                    ERROR_CAM_SEC_5  +
//                                    ERROR_CAM_NSEC_5 +
//                                    ERROR_CAM_SEC_6  +
//                                    ERROR_CAM_NSEC_6 +
//                                    ERROR_CAM_SEC_7  +
//                                    ERROR_CAM_NSEC_7 +
//                                    ERROR_CAM_SEC_8  +
//                                    ERROR_CAM_NSEC_8 +
//                                    ERROR_CAM_SEC_9  +
//                                    ERROR_CAM_NSEC_9,
                                    //ERROR_TURBO_REAL             +
                                    //ERROR_TURBO_IMAG             +
                                    ERROR_TURBO_AMPLITUDE        +
                                    ERROR_TURBO_ENERGY,
    };

    bool success = true;

    uint32_t retBytes = 0; 
    uint32_t errCnt = 0; 
    uint32_t actualLength = 0; 
    uint32_t calcLength = 0; 
    //uint32_t i = 0;
    uint8_t retData[ MSG_SIZE ];
    uint8_t *ptr;

    uint16_t command = 0;
    uint16_t genErr = 0; 
//  uint16_t pfpErr = 0;
//  uint16_t ptltErr = 0;
//  uint16_t ptrtErr = 0;
//  uint16_t tcmpErr = 0;
//  uint16_t copErr = 0;
    //uint16_t copFoRealErr = 0;
    //uint16_t copFoImagErr = 0;
    uint16_t copFoAmplitudeErr = 0; 
    uint16_t copFoPhaseErr = 0; 
    uint16_t copFoEnergyErr = 0; 
    //uint16_t copHoRealErr = 0;
    //uint16_t copHoImagErr = 0;
    uint16_t copHoAmplitudeErr = 0; 
    uint16_t copHoPhaseErr = 0; 
    uint16_t copHoEnergyErr = 0; 
    //uint16_t crankFoRealErr = 0;
    //uint16_t crankFoImagErr = 0;
    uint16_t crankFoAmplitudeErr = 0; 
    uint16_t crankFoPhaseErr = 0; 
    uint16_t crankFoEnergyErr = 0; 
    //uint16_t crankHoRealErr = 0;
    //uint16_t crankHoImagErr = 0;
    uint16_t crankHoAmplitudeErr = 0; 
    uint16_t crankHoPhaseErr = 0; 
    uint16_t crankHoEnergyErr = 0; 
//  uint16_t camSecErr[ MAX_TIMESTAMPS ];
//  uint16_t camNSecErr[ MAX_TIMESTAMPS ];
    //uint16_t turboRealErr = 0;
    //uint16_t turboImagErr = 0;
    uint16_t turboAmplitudeErr = 0;
    uint16_t turboEnergyErr = 0;

    struct pollfd myPoll[1];
    int32_t retPoll;

    errCnt = 0;

    myPoll[0].fd = csocket;
    myPoll[0].events = POLLIN; 

//  struct timeval tv;
//
//  // add timeout of ? 3 seconds here
//  tv.tv_sec = 3;
//  tv.tv_usec = 0;

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
            retBytes = recv( csocket , retData , MSG_SIZE , 0 );

            //command = remote_get16(retData);
            ptr = retData;
            memcpy(&command, ptr, sizeof(command));
            ptr += CMD_ID;

            memcpy(&actualLength, ptr, sizeof(actualLength));
            //actualLength = remote_get16(ptr);
            ptr += LENGTH;

            memcpy(&genErr, ptr, sizeof(genErr));
            //genErr = remote_get16(ptr);
            if (0 != genErr) 
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! genErr",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR;

//          memcpy(&pfpErr, ptr, sizeof(pfpErr));
//          //pfpErr = remote_get16(ptr);
//          if (0 != pfpErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! pfpErr",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_PFP_VALUE;
//
//          memcpy(&ptltErr, ptr, sizeof(ptltErr));
//          //ptltErr = remote_get16(ptr);
//          if (0 != ptltErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! ptltErr",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_PTLT_TEMPERATURE;
//
//          memcpy(&ptrtErr, ptr, sizeof(ptrtErr));
//          //ptrtErr = remote_get16(ptr);
//          if (0 != ptrtErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! ptrtErr",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_PTRT_TEMPERATURE;
//
//          memcpy(&tcmpErr, ptr, sizeof(tcmpErr));
//          //tcmpErr = remote_get16(ptr);
//          if (0 != tcmpErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! tcmpErr",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_TCMP;
//
//          memcpy(&copErr, ptr, sizeof(copErr));
//          //copErr = remote_get16(ptr);
//          if (0 != copErr)
//          {
//              syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! copErr",__FUNCTION__, __LINE__);
//              errCnt++;
//          }
//          ptr += ERROR_COP_PRESSURE;
//
//          for( i = 0 ; i < MAX_TIMESTAMPS ; i++ )
//          {
//              memcpy(&camSecErr[i], ptr, sizeof(camSecErr[i]));
//              //camSecErr[i] = remote_get16(ptr);
//              if (0 != camSecErr[i])
//              {
//                  syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! camSecErr%d",__FUNCTION__, __LINE__, i);
//                  errCnt++;
//              }
//              ptr += ERROR_CAM_SEC_1;
//
//              memcpy(&camNSecErr[i], ptr, sizeof(camNSecErr[i]));
//              //camNSecErr[i] = remote_get16(ptr);
//              if (0 != camNSecErr[i])
//              {
//                  syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! camNSecErr%d",__FUNCTION__, __LINE__, i);
//                  errCnt++;
//              }
//              ptr += ERROR_CAM_NSEC_1;
//          }

            // COP
           // memcpy(&copFoRealErr, ptr, sizeof(copFoRealErr));
           // //copFoRealErr = remote_get16(ptr);
           // if (0 != copFoRealErr)
           // {
           //     syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copFoRealErr!",__FUNCTION__, __LINE__);
           //     errCnt++;
           // }
           // ptr += ERROR_COP_FO_REAL;

          //memcpy(&copFoImagErr, ptr, sizeof(copFoImagErr));
          ////copFoImagErr = remote_get16(ptr);
          //if (0 != copFoImagErr)
          //{
          //    syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copFoImagErr!",__FUNCTION__, __LINE__);
          //    errCnt++;
          //}
          //ptr += ERROR_COP_FO_IMAG;

            memcpy(&copFoAmplitudeErr, ptr, sizeof(copFoAmplitudeErr));
            //copFoAmplitudeErr = remote_get16(ptr);
            if (0 != copFoAmplitudeErr)
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copFoAmplitudeErr!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_COP_FO_AMPLITUDE;

            memcpy(&copFoEnergyErr, ptr, sizeof(copFoEnergyErr));
            //copFoEnergyErr = remote_get16(ptr);
            if (0 != copFoEnergyErr)
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copFoEnergyErr!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_COP_FO_ENERGY;

            memcpy(&copFoPhaseErr, ptr, sizeof(copFoPhaseErr));
            //copFoPhaseErr = remote_get16(ptr);
            if (0 != copFoPhaseErr)
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copFoPhaseErr!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_COP_FO_PHASE;

          //memcpy(&copHoRealErr, ptr, sizeof(copHoRealErr));
          ////copHoRealErr = remote_get16(ptr);
          //if (0 != copHoRealErr)
          //{
          //    syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copHoRealErr!",__FUNCTION__, __LINE__);
          //    errCnt++;
          //}
          //ptr += ERROR_COP_HO_REAL;

          //memcpy(&copHoImagErr, ptr, sizeof(copHoImagErr));
          ////copHoImagErr = remote_get16(ptr);
          //if (0 != copHoImagErr)
          //{
          //    syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copHoImagErr!",__FUNCTION__, __LINE__);
          //    errCnt++;
          //}
          //ptr += ERROR_COP_HO_IMAG;

            memcpy(&copHoAmplitudeErr, ptr, sizeof(copHoAmplitudeErr));
            //copHoAmplitudeErr = remote_get16(ptr);
            if (0 != copHoAmplitudeErr)
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copHoAmplitudeErr!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_COP_HO_AMPLITUDE;

            memcpy(&copHoEnergyErr, ptr, sizeof(copHoEnergyErr));
            //copHoEnergyErr = remote_get16(ptr);
            if (0 != copHoEnergyErr)
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copHoEnergyErr!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_COP_HO_ENERGY;

            memcpy(&copHoPhaseErr, ptr, sizeof(copHoPhaseErr));
            //copHoPhaseErr = remote_get16(ptr);
            if (0 != copHoPhaseErr)
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copHoPhaseErr!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_COP_HO_PHASE;


            // CRANK
          //  memcpy(&crankFoRealErr, ptr, sizeof(crankFoRealErr));
          //  //crankFoRealErr = remote_get16(ptr);
          //  if (0 != crankFoRealErr)
          //  {
          //      syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankFoRealErr!",__FUNCTION__, __LINE__);
          //      errCnt++;
          //  }
          //  ptr += ERROR_CRANK_FO_REAL;

          //  memcpy(&crankFoImagErr, ptr, sizeof(crankFoImagErr));
          //  //crankFoImagErr = remote_get16(ptr);
          //  if (0 != crankFoImagErr)
          //  {
          //      syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankFoImagErr!",__FUNCTION__, __LINE__);
          //      errCnt++;
          //  }
          //  ptr += ERROR_CRANK_FO_IMAG;

            memcpy(&crankFoAmplitudeErr, ptr, sizeof(crankFoAmplitudeErr));
            //crankFoAmplitudeErr = remote_get16(ptr);
            if (0 != crankFoAmplitudeErr)
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankFoAmplitudeErr!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_CRANK_FO_AMPLITUDE;

            memcpy(&crankFoEnergyErr, ptr, sizeof(crankFoEnergyErr));
            //crankFoEnergyErr = remote_get16(ptr);
            if (0 != crankFoEnergyErr)
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankFoEnergyErr!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_CRANK_FO_ENERGY;

            memcpy(&crankFoPhaseErr, ptr, sizeof(crankFoPhaseErr));
            //crankFoPhaseErr = remote_get16(ptr);
            if (0 != crankFoPhaseErr)
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankFoPhaseErr!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_CRANK_FO_PHASE;

          //memcpy(&crankHoRealErr, ptr, sizeof(crankHoRealErr));
          ////crankHoRealErr = remote_get16(ptr);
          //if (0 != crankHoRealErr)
          //{
          //    syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankHoRealErr!",__FUNCTION__, __LINE__);
          //    errCnt++;
          //}
          //ptr += ERROR_CRANK_HO_REAL;

          //memcpy(&crankHoImagErr, ptr, sizeof(crankHoImagErr));
          ////crankHoImagErr = remote_get16(ptr);
          //if (0 != crankHoImagErr)
          //{
          //    syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankHoImagErr!",__FUNCTION__, __LINE__);
          //    errCnt++;
          //}
          //ptr += ERROR_CRANK_HO_IMAG;

            memcpy(&crankHoAmplitudeErr, ptr, sizeof(crankHoAmplitudeErr));
            //crankHoAmplitudeErr = remote_get16(ptr);
            if (0 != crankHoAmplitudeErr)
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankHoAmplitudeErr!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_CRANK_HO_AMPLITUDE;

            memcpy(&crankHoEnergyErr, ptr, sizeof(crankHoEnergyErr));
            //crankHoEnergyErr = remote_get16(ptr);
            if (0 != crankHoEnergyErr)
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankHoEnergyErr!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_CRANK_HO_ENERGY;

            memcpy(&crankHoPhaseErr, ptr, sizeof(crankHoPhaseErr));
            //crankHoPhaseErr = remote_get16(ptr);
            if (0 != crankHoPhaseErr)
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankHoPhaseErr!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_CRANK_HO_PHASE;

            // TURBO
          //memcpy(&turboRealErr, ptr, sizeof(turboRealErr));
          ////turboRealErr = remote_get16(ptr);
          //if (0 != turboRealErr)
          //{
          //    syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
          //    errCnt++;
          //}
          //ptr += ERROR_TURBO_REAL;

          //memcpy(&turboImagErr, ptr, sizeof(turboImagErr));
          ////turboImagErr = remote_get16(ptr);
          //if (0 != turboImagErr)
          //{
          //    syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR!",__FUNCTION__, __LINE__);
          //    errCnt++;
          //}
          //ptr += ERROR_TURBO_IMAG;

            memcpy(&turboAmplitudeErr, ptr, sizeof(turboAmplitudeErr));
            //turboAmplitudeErr = remote_get16(ptr);
            if (0 != turboAmplitudeErr)
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, turboAmplitudeErr!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_TURBO_AMPLITUDE;

            memcpy(&turboEnergyErr, ptr, sizeof(turboEnergyErr));
            //turboEnergyErr = remote_get16(ptr);
            if (0 != turboEnergyErr)
            {
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, turboEnergyErr!",__FUNCTION__, __LINE__);
                errCnt++;
            }
            ptr += ERROR_TURBO_ENERGY;


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


/**
 * Used to package data for sending open UDP message.
 *
 * @param[in] csocket UDP socket
 * @param[in] addr_in UDP address to be sent
 * @param[out] success true/false status of sending message
 *
 * @return true/false status of sending message
 */
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
    uint8_t *ptr; 
    int16_t val16;
    uint8_t *msgLenPtr;
    uint32_t actualLength = 0;
    uint8_t sendData[ MSG_SIZE ];

    socklen_t UDPaddr_size;

    ptr = sendData;
    val16 = CMD_OPEN;
    memcpy(ptr, &val16, sizeof(val16));
    //remote_set16( ptr , CMD_OPEN);
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    *ptr = getpid();
    ptr += APP_ID;

//  *ptr = MSG_SIZE;
//  ptr += MSG_SIZE;

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


/**
 * Used to package data for receiving sys init complete message.
 *
 * @param[in] csocket UDP socket
 * @param[out] success true/false status of receiving message
 *
 * @return true/false status of receiving message
 */
bool process_sysInit( int32_t csocket )
{
    bool gotMsg             = false;

    enum sysinit_params
    {
        CMD_ID                  = 2,
        LENGTH                  = 2,
        MSG_SIZE                =   CMD_ID +
                                    LENGTH,
    };

    uint16_t command        = 0;
    uint32_t retBytes       = 0; 
    uint32_t actualLength   = 0; 
    uint32_t calcLength     = 0;
    int32_t retPoll         = 0;
    uint8_t retData[ MSG_SIZE ];
    uint8_t *ptr;
    socklen_t toRcvUDP_size;
    struct pollfd myPoll[1];
    
    myPoll[0].fd            = csocket;
    myPoll[0].events        = POLLIN; 

//  struct timeval tv;
//
//  // add timeout of ? 3 seconds here
//  tv.tv_sec = 3;
//  tv.tv_usec = 0;

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
                retBytes = recvfrom(csocket , retData , MSG_SIZE , 0 , (struct sockaddr *)&toRcvUDP , &toRcvUDP_size);
                
                //command = remote_get16(retData);
                ptr = retData;
                
                memcpy(&command, ptr, sizeof(command));
                ptr += CMD_ID;

                memcpy(&actualLength, ptr, sizeof(actualLength));
                //actualLength = remote_get16(ptr);
                ptr += LENGTH;

                calcLength = MSG_SIZE - CMD_ID - LENGTH; // shoudl be zero
            }
        }

        // check message
        if ( (0 >= retPoll) || (MSG_SIZE != retBytes) || ( CMD_SYSINIT != command ) || ( calcLength != actualLength ) )
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
            if ( (0 >= retPoll) )
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


/**
 * Used to package data for sending publish message
 *
 * @param[in] csocket UDP socket
 * @param[in] addr_in UDP address to send
 * @param[in] topic_to_pub topic id (per subscription) to
 *       publish
 * @param[out] void
 *
 * @return void
 */
void process_sendPublish( int32_t csocket , struct sockaddr_in addr_in , int32_t topic_to_pub )
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

    //bool success = true;
    uint8_t *ptr; 
    int16_t val16;
    int32_t val32;
    uint8_t *msgLenPtr; 
    uint8_t sendData[ MAXBUFSIZE ];
    uint16_t actualLength = 0;
    socklen_t toSendUDP_size;

    uint32_t i          = 0;
    uint32_t j          = 0;     
    int32_t cntBytes    = 0;
    int32_t sendBytes   = 0;

    
    ptr = sendData;
    val16 = CMD_PUBLISH;
    memcpy(ptr, &val16, sizeof(val16));
    //remote_set16( ptr , CMD_PUBLISH);
    ptr += CMD_ID;
    cntBytes += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;
    cntBytes += LENGTH;

    // this will change ...
    val32 = publishMe[ topic_to_pub ].topic_id;
    memcpy(ptr, &val32, sizeof(val32));
    //remote_set32( ptr , publishMe[ topic_to_pub ].topic_id);
    ptr += TOPIC_ID;
    cntBytes += TOPIC_ID;

    // this will change ...
    val32 = publishMe[ topic_to_pub ].numMPs;
    memcpy(ptr, &val32, sizeof(val32));
    //remote_set32( ptr , publishMe[ topic_to_pub ].numMPs);
    ptr += NUM_MPS;
    cntBytes += NUM_MPS;

    // ???
    val16 = 0;
    memcpy(ptr, &val16, sizeof(val16));
    //remote_set16( ptr , 0);
    ptr += SEQ_NUM;
    cntBytes += SEQ_NUM;

    printf("FROM PROCESS PUBLISH, publishMe[ %d ].numMPs: %d\n", topic_to_pub, publishMe[ topic_to_pub ].numMPs);
    //printf("FROM PUBLISH PROCESS, publishMe[ %d ].numMPs: %d\n",topic_to_pub, publishMe[ topic_to_pub ].numMPs);
    for( i = 0 ; i < publishMe[ topic_to_pub ].numMPs ; i++ )
    {
        // printf("1 FROM PUBLISH PROCESS, publishMe[ %d ].topicSubscription[ %d ].mp: %d\n", topic_to_pub, i, publishMe[ topic_to_pub ].topicSubscription[ i ].mp);
        val32 = publishMe[ topic_to_pub ].topicSubscription[ i ].mp;
        memcpy(ptr, &val32, sizeof(val32));
        //remote_set32( ptr , publishMe[ topic_to_pub ].topicSubscription[ i ].mp);
        ptr += MP;
        cntBytes += MP;
        // printf("2 ROM PUBLISH PROCESS, publishMe[ %d ].topicSubscription[ %d ].numSamples: %d\n", topic_to_pub, i, publishMe[ topic_to_pub ].topicSubscription[ i ].numSamples);
        for ( j = 0 ; j < publishMe[ topic_to_pub ].topicSubscription[ i ].numSamples ; j++ )
        {
            // logicals
            if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_COP_HALFORDER_AMPLITUDE )
            {
                val32 = 77;
                memcpy(ptr, &val32, sizeof(val32));
                //remote_set32(ptr, pfp_values[j]);
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_COP_HALFORDER_ENERGY )
            {
                val32 = 77;
                memcpy(ptr, &val32, sizeof(val32));
                //remote_set32(ptr, ptlt_values[j]);
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_COP_HALFORDER_PHASE )
            {
                val32 = 77;
                memcpy(ptr, &val32, sizeof(val32));
                //remote_set32(ptr, ptrt_values[j]);
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_COP_FIRSTORDER_AMPLITUDE )
            {
                val32 = 77;
                memcpy(ptr, &val32, sizeof(val32));
                //remote_set32(ptr, pfp_values[j]);
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_COP_FIRSTORDER_ENERGY )
            {
                val32 = 77;
                memcpy(ptr, &val32, sizeof(val32));
                //remote_set32(ptr, ptlt_values[j]);
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_COP_FIRSTORDER_PHASE )
            {
                val32 = 77;
                memcpy(ptr, &val32, sizeof(val32));
                //remote_set32(ptr, ptrt_values[j]);
            }
            if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CRANK_HALFORDER_AMPLITUDE )
            {
                val32 = 77;
                memcpy(ptr, &val32, sizeof(val32));
                //remote_set32(ptr, pfp_values[j]);
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CRANK_HALFORDER_ENERGY )
            {
                val32 = 77;
                memcpy(ptr, &val32, sizeof(val32));
                //remote_set32(ptr, ptlt_values[j]);
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CRANK_HALFORDER_PHASE )
            {
                val32 = 77;
                memcpy(ptr, &val32, sizeof(val32));
                //remote_set32(ptr, ptrt_values[j]);
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CRANK_FIRSTORDER_AMPLITUDE )
            {
                val32 = 77;
                memcpy(ptr, &val32, sizeof(val32));
                //remote_set32(ptr, pfp_values[j]);
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CRANK_FIRSTORDER_ENERGY )
            {
                val32 = 77;
                memcpy(ptr, &val32, sizeof(val32));
                //remote_set32(ptr, ptlt_values[j]);
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CRANK_FIRSTORDER_PHASE )
            {
                val32 = 77;
                memcpy(ptr, &val32, sizeof(val32));
                //remote_set32(ptr, ptrt_values[j]);
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_TURBO_OIL_FIRSTORDER_AMPLITUDE )
            {
                val32 = 77;
                memcpy(ptr, &val32, sizeof(val32));
                //remote_set32(ptr, cam_secs_chk[j*9+1]);//cam_secs_chk[i*9+1];
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_TURBO_OIL_FIRSTORDER_ENERGY )
            {
                val32 = 77;
                memcpy(ptr, &val32, sizeof(val32));
                //remote_set32(ptr, cam_secs_chk[j*9+2]);//cam_secs_chk[i*9+2];
            }
//          else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_SEC_4 )
//          {
//              val32 = recvFDL[0].turbo[j];
//              memcpy(ptr, &val32, sizeof(val32));
//              //remote_set32(ptr, cam_secs_chk[j*9+3]);//cam_secs_chk[i*9+3];
//          }
//          else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_SEC_5 )
//          {
//              val32 = 777777;
//              memcpy(ptr, &val32, sizeof(val32));
//              //remote_set32(ptr, cam_secs_chk[j*9+4]);//cam_secs_chk[i*9+4];
//          }
//          else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_SEC_6 )
//          {
//              val32 = 777777;
//              memcpy(ptr, &val32, sizeof(val32));
//              //remote_set32(ptr, cam_secs_chk[j*9+5]);//cam_secs_chk[i*9+5];
//          }
//          else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_SEC_7 )
//          {
//              val32 = 777777;
//              memcpy(ptr, &val32, sizeof(val32));
//              //remote_set32(ptr, cam_secs_chk[j*9+6]);//cam_secs_chk[i*9+6];
//          }
//          else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_SEC_8 )
//          {
//              val32 = 777777;
//              memcpy(ptr, &val32, sizeof(val32));
//              //remote_set32(ptr, cam_secs_chk[j*9+7]);//cam_secs_chk[i*9+7];
//          }
//          else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_SEC_9 )
//          {
//              val32 = 777777;
//              memcpy(ptr, &val32, sizeof(val32));
//              //remote_set32(ptr, cam_secs_chk[j*9+8]);//cam_secs_chk[i*9+8];
//          }
//          else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_1)
//          {
//              val32 = 777777;
//              memcpy(ptr, &val32, sizeof(val32));
//              //remote_set32(ptr, cam_nsecs_chk[j*9+0]);//cam_nsecs_chk[i*9+0];
//          }
//          else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_2 )
//          {
//              val32 = 777777;
//              memcpy(ptr, &val32, sizeof(val32));
//              //remote_set32(ptr, cam_nsecs_chk[j*9+1]);//cam_nsecs_chk[i*9+1];
//          }
//          else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_3 )
//          {
//              val32 = 777777;
//              memcpy(ptr, &val32, sizeof(val32));
//              //remote_set32(ptr, cam_nsecs_chk[j*9+2]);//cam_nsecs_chk[i*9+2];
//          }
//          else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_4 )
//          {
//              val32 = 777777;
//              memcpy(ptr, &val32, sizeof(val32));
//              //remote_set32(ptr, cam_nsecs_chk[j*9+3]);//cam_nsecs_chk[i*9+3];
//          }
//          else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_5 )
//          {
//              val32 = 777777;
//              memcpy(ptr, &val32, sizeof(val32));
//              //remote_set32(ptr, cam_nsecs_chk[j*9+4]);//cam_nsecs_chk[i*9+4];
//          }
//          else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_6 )
//          {
//              val32 = 777777;
//              memcpy(ptr, &val32, sizeof(val32));
//              //remote_set32(ptr, cam_nsecs_chk[j*9+5]);//cam_nsecs_chk[i*9+5];
//          }
//          else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_7 )
//          {
//              val32 = 777777;
//              memcpy(ptr, &val32, sizeof(val32));
//              //remote_set32(ptr, cam_nsecs_chk[j*9+6]);//cam_nsecs_chk[i*9+6];
//          }
//          else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_8 )
//          {
//              val32 = 777777;
//              memcpy(ptr, &val32, sizeof(val32));
//              //remote_set32(ptr, cam_nsecs_chk[j*9+7]);//cam_nsecs_chk[i*9+7];
//          }
//          else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_9 )
//          {
//              val32 = 777777;
//              memcpy(ptr, &val32, sizeof(val32));
//              //remote_set32(ptr, cam_nsecs_chk[j*9+8]);//cam_nsecs_chk[i*9+8];
//          }
//          // else if ... else if ... else if ... oh ... wait ... I'm done already?
            ptr += MP_VAL;
            cntBytes += MP_VAL;
        }
    }
    *ptr = cntBytes;
    ptr += cntBytes;


    // actual length
    actualLength = cntBytes - CMD_ID - LENGTH;
    memcpy( msgLenPtr , &actualLength , sizeof(uint16_t) );

    toSendUDP_size = sizeof(addr_in);
    sendBytes = sendto(csocket, sendData, cntBytes, 0, (struct sockaddr *)&addr_in, toSendUDP_size);
    printf("FROM PUBLISH, sendBytes: %d\n", sendBytes);
    //sendBytes = send( csocket , sendData , msgByteCnt , 0 );

    if ( cntBytes != sendBytes )
    {
        //success = false;
        syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u", __FUNCTION__, __LINE__, sendBytes, cntBytes);
    }
    else
    {
        //success = true;
        //printf("PUBLISH TRUE\n");
    }
    //printf( "PUBLISH SIZE: %d\n" , cntBytes );
}























/**
 * Used to package data for receiving subscribe message
 *
 * @param[in] csocket TCP socket
 * @param[out] success true/false status
 *
 * @return success true/false status
 */
bool process_getSubscribe( int32_t csocket )
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

    uint16_t command = 0;

    //uint32_t retBytes, i, errCnt, actualLength, calcLength, calcMPs;
    uint32_t retBytes = 0;
    int32_t  i = 0; 
    uint32_t  actualLength = 0; 
    uint32_t  calcLength = 0; 
    int32_t  calcMPs = 0;
    uint8_t retData[ MAXBUFSIZE ];
    uint8_t *ptr;

    uint8_t host_os;
    //uint16_t seq_num;
    //uint32_t src_proc_id; 
    uint32_t cnt_retBytes; 

    //socklen_t toRcvUDP_size;
    struct pollfd myPoll[1];
    int32_t retPoll;

    cnt_retBytes = 0;

    myPoll[0].fd = csocket;
    myPoll[0].events = POLLIN; 


//  struct timeval tv;
//
//  // add timeout of ? 3 seconds here
//  tv.tv_sec = 3;
//  tv.tv_usec = 0;

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
            //toRcvUDP_size = sizeof(toRcvUDP);
            retBytes = recv(csocket , retData , MAXBUFSIZE , 0 );

            ptr = retData;
            memcpy(&command, ptr, sizeof(command));
            printf("FROM SUBSCRIBE, command: %d\n", command);
            ptr += CMD_ID;
            cnt_retBytes += CMD_ID;

            memcpy(&actualLength, ptr, sizeof(actualLength));
            printf("FROM SUBSCRIBE, actualLength: %d\n", actualLength);
            //actualLength = remote_get16(ptr);
            ptr += LENGTH;
            cnt_retBytes += LENGTH;

            memcpy(&host_os, ptr, sizeof(host_os));
            printf("FROM SUBSCRIBE, host_os: %d\n", host_os);
            ptr += HOST_OS;
            cnt_retBytes += HOST_OS;

            //src_proc_id = remote_get32(ptr);
            //printf("FROM SUBSCRIBE, num_mps: %d\n", num_mps);
            ptr += SRC_PROC_ID;
            cnt_retBytes += SRC_PROC_ID;

            memcpy(&src_app_name, ptr, sizeof(src_app_name));
            printf("FROM SUBSCRIBE, src_app_name: %d\n", src_app_name);
            //src_app_name = remote_get32(ptr);   // need it
            ptr += SRC_APP_NAME;
            cnt_retBytes += SRC_APP_NAME;

            memcpy(&num_mps, ptr, sizeof(num_mps));
            printf("FROM SUBSCRIBE, num_mps: %d\n", num_mps);
            //num_mps = remote_get32(ptr);        // need it
            ptr += NUM_MPS;
            cnt_retBytes += NUM_MPS;

            //seq_num = remote_get16(ptr);
            ptr += SEQ_NUM;
            cnt_retBytes += SEQ_NUM;

            calcMPs = ( actualLength - 15 ) / 12;
            if ( calcMPs != num_mps )
            {
                MPnum = 14;
            }
            else
            {
                MPnum = num_mps;
            }
//          sub_mp = realloc(sub_mp, sizeof(sub_mp)*num_mps);
//          sub_mpPer = realloc(sub_mpPer, sizeof(sub_mpPer)*num_mps);
//          sub_mpNumSamples = realloc(sub_mpNumSamples, sizeof(sub_mpNumSamples)*num_mps);
            sub_mp              = realloc(sub_mp, sizeof(sub_mp)*MPnum);
            sub_mpPer           = realloc(sub_mpPer, sizeof(sub_mpPer)*MPnum);
            sub_mpNumSamples    = realloc(sub_mpNumSamples, sizeof(sub_mpNumSamples)*MPnum);

            //calcMPs = ( actualLength - 15 ) / 12;
            //if (calcMPs != num_mps)

            for( i = 0 ; i < MPnum ; i++ )
            {
                memcpy(&sub_mp[i], ptr, sizeof(sub_mp[i]));
                //printf("FROM SUBSCRIBE, sub_mp[%d]: %d\n", i, sub_mp[i]);
                //sub_mp[i] = remote_get32(ptr);
                ptr += MP;
                cnt_retBytes += MP;

                memcpy(&sub_mpPer[i], ptr, sizeof(sub_mpPer[i]));
                //printf("FROM SUBSCRIBE, sub_mpPer[%d]: %d\n", i, sub_mpPer[i]);
                //sub_mpPer[i] = remote_get32(ptr);
                ptr += MP_PER;
                cnt_retBytes += MP_PER;

                memcpy(&sub_mpNumSamples[i], ptr, sizeof(sub_mpNumSamples[i]));
                //printf("FROM SUBSCRIBE, sub_mpNumSamples[%d]: %d\n", i, sub_mpNumSamples[i]);
                //sub_mpNumSamples[i] = remote_get32(ptr);
                ptr += MP_NUM_SAMPLES;
                cnt_retBytes += MP_NUM_SAMPLES;
            }
            calcLength = cnt_retBytes - CMD_ID - LENGTH;
        }
    }

    if ( (retPoll <= 0) || (cnt_retBytes != retBytes) || ( CMD_SUBSCRIBE != command ) || ( calcLength != actualLength ) || (calcMPs != MPnum) )
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

        if ( (calcMPs != MPnum) )
        {
            syslog(LOG_ERR, "%s:%d ERROR! length does not coincide with number of MPs: calcMPs %u != MPnum %u", __FUNCTION__, __LINE__, calcMPs, MPnum);
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



/**
 * Used to package data for sending a subscription.  FDL
 * subscribes for 10 MPs in order to calcuate amplitude, energy,
 * and phase related to the cam, crank, and turbo.  
 *
 * @param[in] csocket TCP socket
 * @param[out] success true/false status
 *
 * @return success true/false status
 */
bool process_sendSubscribe( int32_t csocket )
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
                                    MAX_fdl_SUBSCRIPTION * (MP +
                                    MP_PER +
                                    MP_NUM_SAMPLES),
    };

    bool success            = true;

    int32_t sendBytes       = 0;
    int32_t i               = 0;
    uint8_t *ptr;
    int16_t val16;
    int32_t val32;
    uint8_t *msgLenPtr;
    uint32_t actualLength   = 0;
    uint8_t sendData[ MSG_SIZE ];
    //int32_t test_val = 0;

    ptr = sendData;
    val16 = CMD_SUBSCRIBE;
    memcpy(ptr, &val16, sizeof(val16));
    //remote_set16( ptr , CMD_REGISTER_DATA);
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    *ptr = PETALINUX;
    ptr += HOST_OS;

    val32 = getpid();
    memcpy(ptr, &val32, sizeof(val32));
    //remote_set32( ptr , getpid() );
    ptr += SRC_PROC_ID;

    val32 = 15137;
    memcpy(ptr, &val32, sizeof(val32));
    //remote_set32( ptr , 1);
    ptr += SRC_APP_NAME;

    val32 = MAX_fdl_SUBSCRIPTION;
    memcpy(ptr, &val32, sizeof(val32));
    //remote_set32( ptr , MAX_fdl_SUBSCRIPTION);
    ptr += NUM_MPS;

    val32 = 0;
    memcpy(ptr, &val32, sizeof(val32));
    //remote_set32( ptr , MAX_fdl_SUBSCRIPTION);
    ptr += SEQ_NUM;

    // MPs, periods, and number of samples ... I'm guessing 1 second ...
    // consider breaking out, or doing something to identify which ones are requested other than integers
    for ( i = 0 ; i < MAX_fdl_SUBSCRIPTION ; i++ )
    {
        val32 = 1007 + i;   // MP
        memcpy(ptr, &val32, sizeof(val32));
        ptr += 4;
        val32 = 1000;       // MP period
        memcpy(ptr, &val32, sizeof(val32));
        ptr += 4;
        val32 = 1;          // MP number samples
        memcpy(ptr, &val32, sizeof(val32));
        ptr += 4;
    }

    // actual length
    actualLength = MSG_SIZE - CMD_ID - LENGTH;
    memcpy(msgLenPtr, &actualLength, sizeof(actualLength));

    // sending
    sendBytes = send( csocket, sendData, MSG_SIZE, 0 );

    if (MSG_SIZE != sendBytes)
    {
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u",__FUNCTION__, __LINE__, sendBytes, MSG_SIZE);
        //printf("REGISTER_DATA ERROR\n");
    }
    else
    {
        success = true;
        //printf("REGISTER_DATA TRUE\n");
    }
    //printf("REGISTER_DATA SIZE: %d\n",sendBytes);

    return success;
}


/**
 * Used to receive subscribe acknowledgment message
 *
 * @param[in] csocket UDP socket
 * @param[in] addr_in UDP address to send
 * @param[out] success true/false status
 *
 * @return success true/false status
 */
//bool process_subscribe_ack( int32_t csocket , struct sockaddr_in addr_in )
bool process_getSubscribe_ack( int32_t csocket )
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
                                    (MAX_fdl_SUBSCRIPTION * ERROR_MP),
    };

    uint8_t *ptr; // , *msgLenPtr, *msgErrPtr, *topicIDptr;
    //int16_t val16;
    uint8_t retData[ MSG_SIZE ];
    uint32_t retBytes = 0;

    //socklen_t toSendUDP_size;

    //int32_t cntBytes        = 0;
    //int32_t sendBytes       = 0;
    uint32_t i              = 0;

    uint16_t command        = 0;
    uint16_t actualLength   = 0;
    uint16_t calcLength     = 0; 
    //uint32_t topicID        = 0;
    int16_t genErr          = 0;
    int16_t msgErr          = 0;
    int32_t errCnt;

    struct pollfd myPoll[1];
    int32_t retPoll;

    errCnt = 0;

    myPoll[0].fd        = csocket;
    myPoll[0].events    = POLLIN; 

//  struct timeval tv;
//
//  // add timeout of ? 3 seconds here
//  tv.tv_sec = 3;
//  tv.tv_usec = 0;

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
            retBytes = recv( csocket , retData , MSG_SIZE , 0 );
            
            if ((MSG_SIZE == retBytes))
            {
                ptr = retData;
                memcpy(&command, ptr, sizeof(command));
                ptr += CMD_ID;

                memcpy(&actualLength, ptr, sizeof(actualLength));
                //actualLength = remote_get16(ptr);
                ptr += LENGTH;

                memcpy(&fromSubAckTopicID, ptr, sizeof(fromSubAckTopicID));
                ptr += TOPIC_ID;

                memcpy(&genErr, ptr, sizeof(genErr));
                //genErr = remote_get16(ptr);
                if (0 != genErr) 
                {
                    syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! genErr",__FUNCTION__, __LINE__);
                    errCnt++;
                }
                ptr += ERROR;

                for ( i = 0 ; i < MAX_fdl_SUBSCRIPTION ; i++ )
                {
                    memcpy(&msgErr, ptr, sizeof(msgErr));
                    ptr     += ERROR_MP;
                    errCnt  = errCnt + msgErr;
                }

                calcLength = MSG_SIZE - CMD_ID - LENGTH;
            }
        }
    }


    if ( (retPoll <= 0) || (MSG_SIZE != retBytes) || ( errCnt > 0 ) || ( CMD_SUBSCRIBE_ACK != command ) || ( calcLength != actualLength ) )
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
        if ( ( CMD_SUBSCRIBE_ACK != command ) ) 
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

/**
 * Used to package data for receiving MPs. 
 *
 * @param[in] csocket UDP socket
 * @param[in] addr_in UDP address to send
 * @param[in] topic_to_pub topic id (per subscription) to
 *       publish
 * @param[out] void
 *
 * @return void
 */
bool process_getPublish( int32_t csocket )
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
                                    10*(MP + MP_VAL),
    };

    bool success = true;
    
    
        
    //uint32_t retBytes, i, errCnt, actualLength, calcLength, calcMPs;
    uint32_t retBytes       = 0;
    int32_t i               = 0; 
    int32_t cntMPiteration  = 0;
    int32_t MPnum_fromPub   = 0;

    uint16_t command        = 0;
    uint16_t actualLength   = 0; 
    int32_t topicID         = 0;
    uint16_t seqNum         = 0;

    uint32_t calcLength     = 0; 
    int32_t calcMPs         = 0;
    uint8_t retData[ MSG_SIZE ];
    uint8_t *ptr;

    uint32_t cnt_retBytes; 

    //socklen_t toRcvUDP_size;
    struct pollfd myPoll[1];
    int32_t retPoll;

    cnt_retBytes = 0;

    myPoll[0].fd = csocket;
    myPoll[0].events = POLLIN; 


//  struct timeval tv;
//
//  // add timeout of ? 3 seconds here
//  tv.tv_sec = 3;
//  tv.tv_usec = 0;

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
            //toRcvUDP_size = sizeof(toRcvUDP);
            retBytes = recv(csocket , retData , MSG_SIZE , 0 );
            
            ptr = retData;
            memcpy(&command, ptr, sizeof(command));
            ptr += CMD_ID;
            cnt_retBytes += CMD_ID;

            memcpy(&actualLength, ptr, sizeof(actualLength));
            ptr += LENGTH;
            cnt_retBytes += LENGTH;

            // check if this topic ID mathces the one from sub_ack ...
            memcpy(&topicID, ptr, sizeof(topicID));
            ptr += TOPIC_ID;
            cnt_retBytes += TOPIC_ID;

            memcpy(&num_mps_fromPub, ptr, sizeof(num_mps_fromPub));
            ptr += NUM_MPS;
            cnt_retBytes += NUM_MPS;

            memcpy(&seqNum, ptr, sizeof(seqNum));
            ptr += SEQ_NUM;
            cnt_retBytes += SEQ_NUM;
 
            calcMPs = ( actualLength - 4 - 4 - 2)/8;
            if ( calcMPs != num_mps_fromPub )
            {
                MPnum_fromPub = 10;
            }
            else
            {
                MPnum_fromPub = num_mps_fromPub;
            }

            for( i = 0 ; i < 4 ; i++ )
            {
                memcpy(&recvFDL[0].mp[i], ptr, sizeof(recvFDL[0].mp[i]));
                printf("GET PUB recvFDL[0].mp[%d]: %d\n", i, recvFDL[0].mp[i]);
                ptr += MP;
                cnt_retBytes += MP;
                memcpy(&recvFDL[0].cop[i], ptr, sizeof(recvFDL[0].cop[i]));
                printf("GET PUB recvFDL[0].cop[%d]: %d\n", i, recvFDL[0].cop[i]);
                ptr += MP;
                cnt_retBytes += MP;
                cntMPiteration++;
            }

            for( i = 0 ; i < 4 ; i++ )
            {
                memcpy(&recvFDL[0].mp[i + 4], ptr, sizeof(recvFDL[0].mp[i + 4]));
                printf("GET PUB recvFDL[0].mp[%d]: %d\n", i + 4, recvFDL[0].mp[i + 4]);
                ptr += MP;
                cnt_retBytes += MP;
                memcpy(&recvFDL[0].crank[i], ptr, sizeof(recvFDL[0].crank[i]));
                printf("GET PUB recvFDL[0].crank[%d]: %d\n", i, recvFDL[0].crank[i]);
                ptr += MP;
                cnt_retBytes += MP;
                cntMPiteration++;
            }

            for( i = 0 ; i < 2 ; i++ )
            {
                memcpy(&recvFDL[0].mp[i + 8], ptr, sizeof(recvFDL[0].mp[i + 8]));
                printf("GET PUB recvFDL[0].mp[%d]: %d\n", i + 8, recvFDL[0].mp[i + 8]);
                ptr += MP;
                cnt_retBytes += MP;
                memcpy(&recvFDL[0].turbo[i], ptr, sizeof(recvFDL[0].turbo[i]));
                printf("GET PUB recvFDL[0].turbo[%d]: %d\n", i, recvFDL[0].turbo[i]);
                ptr += MP;
                cnt_retBytes += MP;
                cntMPiteration++;
            }

            calcLength = cnt_retBytes - CMD_ID - LENGTH;


            printf("GET PUB topicID: %d\n", topicID);
            printf("GET PUB fromSubAckTopicID: %d\n", fromSubAckTopicID);

        }
    }

    if ( (retPoll <= 0) || (cnt_retBytes != retBytes) || ( CMD_PUBLISH != command ) || ( calcLength != actualLength ) || (calcMPs != MPnum_fromPub) || (cntMPiteration != MPnum_fromPub) || ( topicID != fromSubAckTopicID ) )
    {
        printf("ERROR, FROM GET PUB?\n");
        success = false;
        if ( (cnt_retBytes != retBytes) )
        {
            syslog(LOG_ERR, "%s:%d ERROR! insufficient message data retBytes %u != cnt_retBytes %u", __FUNCTION__, __LINE__, retBytes, cnt_retBytes);
        }
        if ( ( CMD_PUBLISH != command ) )
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

        if ( (calcMPs != MPnum_fromPub) )
        {
            syslog(LOG_ERR, "%s:%d ERROR! length does not coincide with number of MPs: calcMPs %u != MPnum_fromPub %u", __FUNCTION__, __LINE__, calcMPs, MPnum_fromPub);
        }

        if ( (cntMPiteration != MPnum_fromPub) )
        {
            syslog(LOG_ERR, "%s:%d ERROR! MPs received dont' match: cntMPiteration %u != MPnum_fromPub %u", __FUNCTION__, __LINE__, cntMPiteration, MPnum_fromPub);
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





/**
 * Used to package data for sending subscribe acknowledgment
 * message
 *
 * @param[in] csocket UDP socket
 * @param[in] addr_in UDP address to send
 * @param[out] success true/false status
 *
 * @return success true/false status
 */
//bool process_subscribe_ack( int32_t csocket , struct sockaddr_in addr_in )
bool process_sendSubscribe_ack( int32_t csocket )
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
    int16_t val16;
    uint8_t sendData[ MAXBUFSIZE ];
    uint16_t actualLength;
    //socklen_t toSendUDP_size;

    int32_t cntBytes    = 0;
    int32_t sendBytes   = 0;
    uint32_t i          = 0;
    //uint32_t tID        = 1000;
    int16_t genErr      = GE_SUCCESS;


    ptr = sendData;
    val16 = CMD_SUBSCRIBE_ACK;
    memcpy(ptr, &val16, sizeof(val16));
    //remote_set16( ptr , CMD_SUBSCRIBE_ACK );
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

    printf("SENDING SUB ACK, publishMe[ %d ].numMPs: %d\n", currentTopic, publishMe[ currentTopic ].numMPs);
    for( i = 0 ; i < publishMe[ currentTopic ].numMPs ; i++ ) 
    {
        if ( true == publishMe[ currentTopic ].topicSubscription[ i ].valid )
        {
            val16 = GE_SUCCESS;
            memcpy(ptr, &val16, sizeof(val16));
            //remote_set16( ptr , GE_SUCCESS);
        }
        else
        {
            val16 = GE_INVALID_MP_NUMBER;
            memcpy(ptr, &val16, sizeof(val16));
            //remote_set16( ptr , GE_INVALID_MP_NUMBER);
            genErr = GE_INVALID_MP_NUMBER;
            success = false;
        }
        ptr += ERROR_MP;
        cntBytes += ERROR_MP;
    }

//  *ptr = MSG_SIZE;
//  ptr += MSG_SIZE;

    actualLength = cntBytes - CMD_ID - LENGTH;
    
    // why do I get errors when I rearrange these?  e.g. memcpy topic ID first, then length, then error.
    // I get the right stuff when I memcpy in the order they were assigned above ... why?  
    memcpy(msgLenPtr,   &actualLength,                          sizeof(uint16_t));
    memcpy(topicIDptr,  &publishMe[ currentTopic ].topic_id,    sizeof(uint32_t));
    memcpy(msgErrPtr,   &genErr,                                sizeof(int16_t));

    // send
    //toSendUDP_size  = sizeof(addr_in);
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
//  else
//  {
//      printf("SUBSCRIBE_ACK TRUE! \n");
//  }

    return success;
}



/**
 * Used to package and send heartbeat signal
 *
 * @param[in] csocket TCP socket
 * @param[out] success true/false status
 *
 * @return success true/false status
 */
bool process_HeartBeat( int32_t csocket, int32_t HeartBeat )
{
    bool success = true;

    enum heartbeat_params
    {
        CMD_ID                  = 2,
        LENGTH                  = 2,
        HRTBT_CNT               = 4,
        MSG_SIZE                =   CMD_ID +
                                    LENGTH +
                                    HRTBT_CNT,
    };

    uint8_t *ptr;
    int16_t val16;
    int32_t val32;
    uint8_t *msgLenPtr;
    uint8_t sendData[ MSG_SIZE ];
    uint16_t actualLength;
    int32_t cntBytes    = 0;
    int32_t sendBytes   = 0;

    ptr = sendData;
    val16 = CMD_HEARTBEAT;
    memcpy(ptr, &val16, sizeof(val16));
    //remote_set16( ptr , CMD_HEARTBEAT );
    ptr += CMD_ID;
    cntBytes += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;
    cntBytes += LENGTH;

    val32 = HeartBeat;
    memcpy(ptr, &val32, sizeof(val32));
    //remote_set32( ptr , HeartBeat );
    ptr += HRTBT_CNT;
    cntBytes += HRTBT_CNT;

    actualLength = cntBytes - CMD_ID - LENGTH;
    memcpy(msgLenPtr, &actualLength, sizeof(uint16_t));
    sendBytes = send(csocket, sendData, cntBytes, 0);

    // check message
    if ( (cntBytes != sendBytes) )
    {
        success = false;
        if ( cntBytes != sendBytes )
        {
            syslog(LOG_ERR, "%s:%d ERROR! heartbeat, insufficient message data %u != %u ", __FUNCTION__, __LINE__, sendBytes, cntBytes);
        }
    }

    return success;

}


/**
 * This set of data (publishMe) is built based on the
 * current subscription.  After a SUBSCRIBE() and before
 * SUBSCRIBE_ACK(), this function is called.
 * 
 * This will change with multiple subscribe rates for phase II
 * 
 * @param[in] void
 * @param[out] success true/false status
 *
 * @return success true/false status
 */
bool buildPublishData(void)
{
    bool success = true;
    //bool MPmatches = false;
    uint32_t i;
    uint32_t  k;
    //int32_t  cnt;
    int32_t periodVar; 
    int32_t numMPsMatching;
    int32_t numSamplesToChk;

//  //uint32_t fdlsubscriptionMP[ MAX_fdl_SUBSCRIPTION ]; // = {MP_PFP_VALUE,MP_PTLT_TEMPERATURE,MP_PTRT_TEMPERATURE,MP_TCMP,MP_CAM_SEC_1,MP_CAM_NSEC_1,MP_CAM_SEC_2,MP_CAM_NSEC_2,MP_CAM_SEC_3,MP_CAM_NSEC_3,MP_CAM_SEC_4,MP_CAM_NSEC_4,MP_CAM_SEC_5,MP_CAM_NSEC_5,MP_CAM_SEC_6,MP_CAM_NSEC_6,MP_CAM_SEC_7,MP_CAM_NSEC_7,MP_CAM_SEC_8,MP_CAM_NSEC_8,MP_CAM_SEC_9,MP_CAM_NSEC_9,MP_COP_PRESSURE};
//  uint32_t fdlsubscriptionMP[ MAX_fdl_SUBSCRIPTION ] =
//  {
//      MP_COP_HO_REAL, //    *float   COP half order real value
//      MP_COP_HO_IMAG, //    *float   COP half order imaginary  value
//      MP_COP_FO_REAL, //    *Float   COP full order real value
//      MP_COP_FO_IMAG, //    *float   COP full order imaginary  value
//      MP_CRANK_HO_REAL, //    *Float   CRANK half order real value
//      MP_CRANK_HO_IMAG, //    *Float   CRANK half order imaginary  value
//      MP_CRANK_FO_REAL, //    *Float   CRANK full order real value
//      MP_CRANK_FO_IMAG, //    *float   CRANK full order imaginary  value
//      MP_TURBO_REAL, //    *float   MPTurbo oil let sensor � real part
//      MP_TURBO_IMAG
//  };

    // 1017-1030
    uint32_t fdlsubscriptionMP[ MAX_fdl_TO_PUBLISH ] = 
    {
        MP_COP_HALFORDER_AMPLITUDE,
        MP_COP_HALFORDER_ENERGY,
        MP_COP_HALFORDER_PHASE,
        MP_COP_FIRSTORDER_AMPLITUDE,
        MP_COP_FIRSTORDER_ENERGY,
        MP_COP_FIRSTORDER_PHASE,
        MP_CRANK_HALFORDER_AMPLITUDE,
        MP_CRANK_HALFORDER_ENERGY,
        MP_CRANK_HALFORDER_PHASE,
        MP_CRANK_FIRSTORDER_AMPLITUDE,
        MP_CRANK_FIRSTORDER_ENERGY,
        MP_CRANK_FIRSTORDER_PHASE,
        MP_TURBO_OIL_FIRSTORDER_AMPLITUDE,
        MP_TURBO_OIL_FIRSTORDER_ENERGY
    };



//  int32_t fdlsubscriptionPeriod[ MAX_fdl_SUBSCRIPTION ] =
//  {
//      MINPER_PFP_VALUE,
//      MINPER_PTLT_TEMPERATURE,
//      MINPER_PTRT_TEMPERATURE,
//      MINPER_TCMP,
//      MINPER_CAM_SEC_1,
//      MINPER_CAM_NSEC_1,
//      MINPER_CAM_SEC_2,
//      MINPER_CAM_NSEC_2,
//      MINPER_CAM_SEC_3,
//      MINPER_CAM_NSEC_3,
//      MINPER_CAM_SEC_4,
//      MINPER_CAM_NSEC_4,
//      MINPER_CAM_SEC_5,
//      MINPER_CAM_NSEC_5,
//      MINPER_CAM_SEC_6,
//      MINPER_CAM_NSEC_6,
//      MINPER_CAM_SEC_7,
//      MINPER_CAM_NSEC_7,
//      MINPER_CAM_SEC_8,
//      MINPER_CAM_NSEC_8,
//      MINPER_CAM_SEC_9,
//      MINPER_CAM_NSEC_9,
//      MINPER_COP_PRESSURE
//  };

    publishMe[ currentTopic].app_name  = src_app_name;
    publishMe[ currentTopic ].numMPs   = MPnum;
    
    // for new topic/subscription
    publishMe = realloc(publishMe, sizeof(topicToPublish)*num_topics_total);
    if (NULL == publishMe)
    {
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! BAD realloc()",__FUNCTION__, __LINE__);
    }
    else
    {
        // for MPs
        publishMe[ currentTopic ].topicSubscription = malloc(sizeof(MPinfo)*publishMe[ currentTopic ].numMPs);
        if (NULL == publishMe[ currentTopic ].topicSubscription)
        {
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! BAD malloc()",__FUNCTION__, __LINE__);
        }
    }

    publishMe[ currentTopic ].topicSubscription = malloc(sizeof(MPinfo)*publishMe[ currentTopic ].numMPs);
    if (NULL == publishMe[ currentTopic ].topicSubscription)
    {
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! BAD malloc()",__FUNCTION__, __LINE__);
    }


    numMPsMatching      = 0;

    //printf("FROM BUILD PUBLISH DATA, publishMe[ currentTopic ].numMPs: %d\n", publishMe[ currentTopic ].numMPs);
    for( k = 0 ; k < publishMe[ currentTopic ].numMPs ; k++ )
    {
        publishMe[ currentTopic ].topicSubscription[ k ].mp             = sub_mp[ k ];
        publishMe[ currentTopic ].topicSubscription[ k ].period         = sub_mpPer[ k ];
        publishMe[ currentTopic ].topicSubscription[ k ].numSamples     = sub_mpNumSamples[ k ];

        // if MP is logical 


        if (    (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_COP_HO_REAL )    || 
                (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_COP_HO_IMAG )    ||  
                (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_COP_FO_REAL )    ||  
                (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_COP_FO_IMAG )    ||   
                (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CRANK_HO_REAL )  ||  
                (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CRANK_HO_IMAG )  ||
                (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CRANK_FO_REAL )  ||  
                (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CRANK_FO_IMAG )  || 
                (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_TURBO_REAL )     || 
                (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_TURBO_IMAG ) )
//      if ( (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_PFP_VALUE ) ||
//           (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_PTLT_TEMPERATURE ) ||
//           (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_PTRT_TEMPERATURE ) ||
//           (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_TCMP ) ||
//           (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_COP_PRESSURE ) )
        {
            publishMe[ currentTopic ].topicSubscription[ k ].logical = true;

            publishMe[ currentTopic ].topicSubscription[ k ].mp_val_float = malloc(sizeof(float)*sub_mpNumSamples[ k ]);
            if (NULL == publishMe[ currentTopic ].topicSubscription[ k ].mp_val_float)
            {
                success = false;
                syslog(LOG_ERR, "%s:%d ERROR! BAD malloc()",__FUNCTION__, __LINE__);
            }
            else 
            {
                memset(publishMe[ currentTopic ].topicSubscription[ k ].mp_val_float, 0, sizeof(float)*sub_mpNumSamples[ k ]);
            }

            // since this function is only called once per subscribe, this is not correct.  
//          for ( i = 0 ; i < sub_mpNumSamples[ k ] ; i++ )
//          {
//              if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_PFP_VALUE )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_float[i] = 777777;
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_PTLT_TEMPERATURE )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_float[i] = 777777;
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_PTRT_TEMPERATURE )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_float[i] = 777777;
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_TCMP )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_float[i] =  777777;
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_COP_PRESSURE )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_float[i] = 777777;
//              }
//
//              //printf("publishMe[ currentTopic ].topicSubscription[ k ].mp: %d\n", publishMe[currentTopic].topicSubscription[k].mp);
//              //printf("publishMe[ currentTopic ].topicSubscription[ k ].mp_val_float[i]: %d\n", publishMe[ currentTopic ].topicSubscription[ k ].mp_val_float[i]);
//
//          }
        }
        else    // else timestamp
        {
            publishMe[ currentTopic ].topicSubscription[ k ].logical = false;

            publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long = malloc(sizeof(uint32_t)*sub_mpNumSamples[ k ]);  // 18 = 9 seconds, 9 nanoseconds.  This is the max potential number of timestamps per second. 
            if (NULL == publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long)
            {
                success = false;
                syslog(LOG_ERR, "%s:%d ERROR! BAD malloc()",__FUNCTION__, __LINE__);
            }
            else
            {
                memset(publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long, 0, sizeof(uint32_t)*sub_mpNumSamples[ k ]);
            }

//          //printf("FROM BUILD PUB DATA, sub_mpNumSamples[ %d ]: %d\n", k, sub_mpNumSamples[ k ]);
//          for (i = 0 ; i < sub_mpNumSamples[ k ] ; i++)
//          {
//              // a lot, I know ... timestamps are in two arrays, but MPs separate them invidually ... not sure if there is soemthing better than this
//              if  (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_SEC_1)
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 1;//cam_secs_chk[i*9+0];
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_SEC_2 )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 2;//cam_secs_chk[i*9+1];
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_SEC_3 )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 3;//cam_secs_chk[i*9+2];
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_SEC_4 )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 4;//cam_secs_chk[i*9+3];
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_SEC_5 )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 5;//cam_secs_chk[i*9+4];
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_SEC_6 )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 6;//cam_secs_chk[i*9+5];
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_SEC_7 )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 7;//cam_secs_chk[i*9+6];
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_SEC_8 )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 8;//cam_secs_chk[i*9+7];
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_SEC_9 )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 9;//cam_secs_chk[i*9+8];
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_NSEC_1)
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 10;//cam_nsecs_chk[i*9+0];
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_NSEC_2 )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 11;//cam_nsecs_chk[i*9+1];
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_NSEC_3 )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 12;//cam_nsecs_chk[i*9+2];
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_NSEC_4 )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 13;//cam_nsecs_chk[i*9+3];
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_NSEC_5 )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 14;//cam_nsecs_chk[i*9+4];
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_NSEC_6 )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 15;//cam_nsecs_chk[i*9+5];
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_NSEC_7 )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 16;//cam_nsecs_chk[i*9+6];
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_NSEC_8 )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 17;//cam_nsecs_chk[i*9+7];
//              }
//              else if (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_CAM_NSEC_9 )
//              {
//                  publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i] = 18;//cam_nsecs_chk[i*9+8];
//              }
//              //printf("publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i]: %d\n", publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long[i]);
//          }
        }

        numMPsMatching = 0;
        for( i = 0 ; i < MAX_fdl_TO_PUBLISH ; i++ )
        {
            //printf("Does fdlsubscriptionMP[ %d ] (%d) == publishMe[ currentTopic ].topicSubscription[ %d ].mp (%d)\n", i, fdlsubscriptionMP[ i ], k, publishMe[ currentTopic ].topicSubscription[ k ].mp);
            if( fdlsubscriptionMP[ i ] == publishMe[ currentTopic ].topicSubscription[ k ].mp )
            {
                numMPsMatching++;
            }
        }
        //printf("FROM BUILD PUBLISH DATA, numMPsMatching: %d\n", numMPsMatching);

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
        // (0 == numMPsMatching) should be (numMPs == numMPsMatching), right?
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

/**
 * Used to determine variance of subscribed periods.  Since they
 * should be the same, variance should equal 0.
 * 
 * 
 * @param[in] void
 * @param[out] var variance of subscribed periods
 *
 * @return variance of subscribed periods
 */
int32_t getVarPeriod(void)
{
    uint32_t i;
    int32_t avg; 
    int32_t sum; 
    int32_t var;
    int32_t diff;
    int32_t sq_diff;

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



/**
 * Byte-swap if needed uint16_t val.  
 * 
 * @param[in] ptr address where data resides
 * @param[in] val 16 bit value to swapped
 * @param[out] void
 *
 * @return void
 */
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


/**
 * Byte-swap if needed uint32_t val.  
 * 
 * @param[in] ptr address where data resides
 * @param[in] val 32 bit value to swapped
 * @param[out] void
 *
 * @return void
 */
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

/**
 * Byte-swap if needed uint64_t val.  
 * 
 * @param[in] ptr address where data resides
 * @param[in] val 64 bit value to swapped
 * @param[out] void
 *
 * @return void
 */
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

/**
 * Byte-swap and store (if needed) uint16_t val.  
 * 
 * @param[in] ptr address where data resides
 * @param[out] void
 *
 * @return void
 */
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

/**
 * Byte-swap and store (if needed) uint32_t val.  
 * 
 * @param[in] ptr address where data resides
 * @param[out] void
 *
 * @return void
 */
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


/**
 * Determines if numSeconds have elapsed.  If numSeconds have
 * elapsed, success is true.  Else, success if false.  
 * 
 * @param[in] startTime when time started
 * @param[in] stopTime when time stopped
 * @param[in] numSeconds number of seconds to check
 * @param[out] success If numSeconds have
 * elapsed, success is true.  Else, success if false.
 *
 * @return success If numSeconds have
 * elapsed, success is true.  Else, success if false.
 */
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


/**
 * Returns a topic ID for each subscription.  If subscription
 * exists, returns existing topic ID. If one does not exist,
 * generates new one (increments for now).
 * 
 * For phase II, this isn't used.  That is, each subscroption is
 * assigned a unique topic ID.  
 * 
 * @param[in] subAppName app name for a subscribe message
 * @param[out] new_offset generates new topic ID if one does not
 * exist.
 *
 * @return topic ID of existing subscription of new topic ID fo
 *         new subscription.
 */
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

/**
 * Returns number of topics to publish during next 1 second
 * interval.  Flags the next set of available topics to publish.
 * 
 * @param[in] void
 * @param[out] numToPub Returns number of topics to publish
 * during next 1 second interval.
 *
 * @return Returns number of topics to publish during next 1 second
 * interval.  
 */
int32_t publishManager(void)
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

    return numToPub;
}


float getAmplitude(int32_t realVal, int32_t imagVal)
{
    float realSq;
    float imagSq;
    float sumRI;
    float sumRIsq;

    realSq  = pow(realVal , 2);
    imagSq  = pow(imagVal , 2);
    sumRI   = realSq + imagSq;
    sumRIsq = sqrtf( sumRI ); 

    return sumRIsq;
}

float getPhase(int32_t realVal, int32_t imagVal)
{
    float phase;
    float atanParam;

    atanParam = imagVal/realVal;
        
    phase = atan(atanParam) * (180/3.1415); // degrees
    return phase;
}

//float getPower(int32_t realVal, int32_t imagVal)
//{
//    float phase;
//    float atanParam;
//
//    atanParam = imagVal/realVal;
//
//    phase = atan(atanParam) * (180/3.1415); // degrees
//    return phase;
//}


