
/** @file fdl_functions.c
 * Functions used send, receive, and package fdl/FPGA data.
 * Enumerations configured per API ICD document.
 *
 * Copyright (c) 2015, DornerWorks, Ltd.
 */

/****************
* INCLUDES
****************/
#include <sys/types.h>
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

uint32_t num_topics_total = 0;
uint32_t num_topics_atCurrentRate;
uint32_t currentTopic = 0;
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

    int32_t sendBytes = 0;
    uint8_t *ptr;
    int16_t val16;
    uint8_t *msgLenPtr = 0;
    uint16_t actualLength = 0;
    uint32_t secsToChk = 0;
    uint8_t sendData[ MSG_SIZE ];
    struct timespec endTime;

    ptr = sendData;
    val16 = CMD_REGISTER_APP;
    memcpy(ptr, &val16, sizeof(val16));
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    *ptr = PETALINUX;
    ptr += HOST_OS;

    memcpy(ptr, &fdlPid, SRC_PROC_ID);
    ptr += SRC_PROC_ID;

    memcpy(ptr, &fdlAppName, SRC_APP_NAME);
    ptr += SRC_APP_NAME;

    secsToChk = 1;

    actualLength = MSG_SIZE - CMD_ID - LENGTH;
    memcpy(msgLenPtr, &actualLength, LENGTH);

    // used to check "within 1 second" requirement between server connect and REGISTER_APP
    clock_gettime(CLOCK_REALTIME, &endTime);

    if (false == numSecondsHaveElapsed( goTime , endTime , secsToChk ) )
    {
        // time did not elapse, so send
        sendBytes = send(csocket, sendData, MSG_SIZE, 0);
    }
    else
    {
        printf("ERROR! REGISTER APP: failed to send REGISTER_APP to AACM within one second of connecting \n");
        syslog(LOG_ERR, "%s:%d ERROR! failed to send REGISTER_APP to AACM within one second of connecting", __FUNCTION__, __LINE__);
        success = false;
    }

    if (MSG_SIZE != sendBytes)
    {
        printf("ERROR! REGISTER APP: bytes sent doesn't equal message size \n");
        syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u",__FUNCTION__, __LINE__, sendBytes, MSG_SIZE);
        success = false;
    }
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
    ssize_t retBytes = 0;
    uint8_t retData[ MSG_SIZE ];    // or whatever max is defined
    uint16_t appAckErr = 0;
    uint8_t *ptr;
    int32_t actualLength = 0;

    int32_t calcLength = 0;

    retBytes = recv(csocket, retData , MSG_SIZE , 0 );
    if (retBytes == MSG_SIZE)
    {
        ptr = retData;
        memcpy(&command, ptr, sizeof(command));
        ptr += CMD_ID;

        memcpy(&actualLength, ptr, LENGTH);
        ptr += LENGTH;

        memcpy(&appAckErr, ptr, sizeof(appAckErr));
        ptr += ERROR;

        calcLength = MSG_SIZE - CMD_ID - LENGTH;
    }

    if ( (retBytes != MSG_SIZE) || ( appAckErr != 0 ) || (CMD_REGISTER_APP_ACK != command) || (calcLength != actualLength) )
    {
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! register_app_ack error",__FUNCTION__, __LINE__);
        if ( (retBytes != MSG_SIZE) )
        {
            printf("ERROR! REGISTER APP ACKNOWLEDGE: bytes received don't equal message size \n");
            syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %zd != %u",__FUNCTION__, __LINE__, retBytes, MSG_SIZE);
        }
        if ( ( appAckErr != 0 ) )
        {
            printf("ERROR! REGISTER APP ACKNOWLEDGE: error received in message payload \n");
            syslog(LOG_ERR, "%s:%d ERROR! payload error received %u ",__FUNCTION__, __LINE__, appAckErr);
        }
        if ( (CMD_REGISTER_APP_ACK != command) )
        {
            printf("ERROR! REGISTER APP ACKNOWLEDGE: invalid command in payload \n");
            syslog(LOG_ERR, "%s:%d ERROR! invalid command %u ",__FUNCTION__, __LINE__, command);
        }
        if ( (calcLength != actualLength) )
        {
            printf("ERROR! REGISTER APP ACKNOWLEDGE: payload length incorrect \n");
            syslog(LOG_ERR, "%s:%d ERROR! payload length not as expected %u != %u ",__FUNCTION__, __LINE__, calcLength, actualLength);
        }
    }
    else
    {
        success = true;
    }

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
    uint16_t actualLength = 0;
    uint8_t sendData[ MSG_SIZE ];

    ptr = sendData;
    val16 = CMD_REGISTER_DATA;
    memcpy(ptr, &val16, sizeof(val16));
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    *ptr = PETALINUX;
    ptr += HOST_OS;

    memcpy(ptr, &fdlPid, SRC_PROC_ID);
    ptr += SRC_PROC_ID;

    memcpy(ptr, &fdlAppName, SRC_APP_NAME);
    ptr += SRC_APP_NAME;

    val32 = MAX_fdl_TO_PUBLISH;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += NUM_MPS;

    val32 = MP_COP_HALFORDER_AMPLITUDE;
    memcpy(ptr , &val32, sizeof(val32));
    ptr += COP_HO_AMPLITUDE;

    val32 = MP_COP_HALFORDER_ENERGY;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += COP_HO_ENERGY;

    val32 = MP_COP_HALFORDER_PHASE;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += COP_HO_PHASE;

    val32 = MP_COP_FIRSTORDER_AMPLITUDE;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += COP_FO_AMPLITUDE;

    val32 = MP_COP_FIRSTORDER_ENERGY;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += COP_FO_ENERGY;

    val32 = MP_COP_FIRSTORDER_PHASE;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += COP_FO_PHASE;

    val32 = MP_CRANK_HALFORDER_AMPLITUDE;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CRANK_HO_AMPLITUDE;

    val32 = MP_CRANK_HALFORDER_ENERGY;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CRANK_HO_ENERGY;

    val32 = MP_CRANK_HALFORDER_PHASE;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CRANK_HO_PHASE;

    val32 = MP_CRANK_FIRSTORDER_AMPLITUDE;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CRANK_FO_AMPLITUDE;

    val32 = MP_CRANK_FIRSTORDER_ENERGY;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CRANK_FO_ENERGY;

    val32 = MP_CRANK_FIRSTORDER_PHASE;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CRANK_FO_PHASE;

    val32 = MP_TURBO_OIL_FIRSTORDER_AMPLITUDE;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += TURBO_FO_AMPLITUDE;

    val32 = MP_TURBO_OIL_FIRSTORDER_ENERGY;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += TURBO_FO_ENERGY;

    // actual length
    actualLength = MSG_SIZE - CMD_ID - LENGTH;
    memcpy(msgLenPtr, &actualLength, LENGTH);

    // sending
    sendBytes = send( csocket, sendData, MSG_SIZE, 0 );

    if (MSG_SIZE != sendBytes)
    {
        printf("ERROR! REGISTER DATA: bytes sent don't equal message size \n");
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u",__FUNCTION__, __LINE__, sendBytes, MSG_SIZE);
    }
    else
    {
        success = true;
    }

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

    ssize_t retBytes = 0;
    uint32_t errCnt = 0;
    uint16_t actualLength = 0;
    uint32_t calcLength = 0;
    //uint32_t i = 0;
    uint8_t retData[ MSG_SIZE ];
    uint8_t *ptr;

    uint16_t command = 0;
    uint16_t genErr = 0;
    uint16_t copFoAmplitudeErr = 0;
    uint16_t copFoPhaseErr = 0;
    uint16_t copFoEnergyErr = 0;
    uint16_t copHoAmplitudeErr = 0;
    uint16_t copHoPhaseErr = 0;
    uint16_t copHoEnergyErr = 0;
    uint16_t crankFoAmplitudeErr = 0;
    uint16_t crankFoPhaseErr = 0;
    uint16_t crankFoEnergyErr = 0;
    uint16_t crankHoAmplitudeErr = 0;
    uint16_t crankHoPhaseErr = 0;
    uint16_t crankHoEnergyErr = 0;
    uint16_t turboAmplitudeErr = 0;
    uint16_t turboEnergyErr = 0;

    errCnt = 0;

    retBytes = recv(csocket, retData , MSG_SIZE , 0 );
    if (retBytes == MSG_SIZE)
    {   
        ptr = retData;
        memcpy(&command, ptr, sizeof(command));
        ptr += CMD_ID;

        memcpy(&actualLength, ptr, LENGTH);
        ptr += LENGTH;

        memcpy(&genErr, ptr, sizeof(genErr));
        if (0 != genErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP genErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! genErr",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR;

        memcpy(&copFoAmplitudeErr, ptr, sizeof(copFoAmplitudeErr));
        if (0 != copFoAmplitudeErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP copFoAmplitudeErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copFoAmplitudeErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_COP_FO_AMPLITUDE;

        memcpy(&copFoEnergyErr, ptr, sizeof(copFoEnergyErr));
        if (0 != copFoEnergyErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP copFoEnergyErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copFoEnergyErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_COP_FO_ENERGY;

        memcpy(&copFoPhaseErr, ptr, sizeof(copFoPhaseErr));
        if (0 != copFoPhaseErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP copFoPhaseErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copFoPhaseErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_COP_FO_PHASE;

        memcpy(&copHoAmplitudeErr, ptr, sizeof(copHoAmplitudeErr));
        if (0 != copHoAmplitudeErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP copHoAmplitudeErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copHoAmplitudeErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_COP_HO_AMPLITUDE;

        memcpy(&copHoEnergyErr, ptr, sizeof(copHoEnergyErr));
        if (0 != copHoEnergyErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP copHoEnergyErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copHoEnergyErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_COP_HO_ENERGY;

        memcpy(&copHoPhaseErr, ptr, sizeof(copHoPhaseErr));
        if (0 != copHoPhaseErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP copHoPhaseErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copHoPhaseErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_COP_HO_PHASE;

        memcpy(&crankFoAmplitudeErr, ptr, sizeof(crankFoAmplitudeErr));
        if (0 != crankFoAmplitudeErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP crankFoAmplitudeErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankFoAmplitudeErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_CRANK_FO_AMPLITUDE;

        memcpy(&crankFoEnergyErr, ptr, sizeof(crankFoEnergyErr));
        if (0 != crankFoEnergyErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP crankFoEnergyErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankFoEnergyErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_CRANK_FO_ENERGY;

        memcpy(&crankFoPhaseErr, ptr, sizeof(crankFoPhaseErr));
        if (0 != crankFoPhaseErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP crankFoPhaseErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankFoPhaseErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_CRANK_FO_PHASE;

        memcpy(&crankHoAmplitudeErr, ptr, sizeof(crankHoAmplitudeErr));
        if (0 != crankHoAmplitudeErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP crankHoAmplitudeErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankHoAmplitudeErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_CRANK_HO_AMPLITUDE;

        memcpy(&crankHoEnergyErr, ptr, sizeof(crankHoEnergyErr));
        if (0 != crankHoEnergyErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP crankHoEnergyErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankHoEnergyErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_CRANK_HO_ENERGY;

        memcpy(&crankHoPhaseErr, ptr, sizeof(crankHoPhaseErr));
        if (0 != crankHoPhaseErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP crankHoPhaseErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankHoPhaseErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_CRANK_HO_PHASE;

        memcpy(&turboAmplitudeErr, ptr, sizeof(turboAmplitudeErr));
        if (0 != turboAmplitudeErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP turboAmplitudeErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, turboAmplitudeErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_TURBO_AMPLITUDE;

        memcpy(&turboEnergyErr, ptr, sizeof(turboEnergyErr));
        if (0 != turboEnergyErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP turboEnergyErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, turboEnergyErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_TURBO_ENERGY;

        calcLength = MSG_SIZE - CMD_ID - LENGTH;
    }

    if ( (MSG_SIZE != retBytes) || ( errCnt > 0 ) || ( CMD_REGISTER_DATA_ACK != command ) || ( calcLength != actualLength ) )
    {
        success = false;
        if ( (MSG_SIZE != retBytes) )
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: received bytes don't equal message size expected \n");
            syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %zd != %u", __FUNCTION__, __LINE__, retBytes, MSG_SIZE);
        }
        if ( ( errCnt > 0 ) )
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP errors detected \n");
            syslog(LOG_ERR, "%s:%d ERROR! MP errors received: %u", __FUNCTION__, __LINE__, errCnt);
        }
        if ( ( CMD_REGISTER_DATA_ACK != command ) )
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: invalid command in payload \n");
            syslog(LOG_ERR, "%s:%d ERROR! invalid command %u", __FUNCTION__, __LINE__, command);
        }
        if ( ( calcLength != actualLength ) )
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: payload length incorrect \n");
            syslog(LOG_ERR, "%s:%d ERROR! payload not equal to expected number of bytes %u != %u ",__FUNCTION__, __LINE__, calcLength, actualLength);
        }
    }
    else
    {
        success = true;
    }
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
        APP_ID                  = 4,
        MSG_SIZE                =   CMD_ID +
                                    LENGTH +
                                    APP_ID,
    };

    int32_t sendBytes = 0;
    uint8_t *ptr;
    int16_t val16;
    uint8_t *msgLenPtr;
    uint16_t actualLength = 0;
    uint8_t sendData[ MSG_SIZE ];

    socklen_t UDPaddr_size;

    ptr = sendData;
    val16 = CMD_OPEN;
    memcpy(ptr, &val16, sizeof(val16));
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    memcpy(ptr, &fdlPid, APP_ID);
    ptr += APP_ID;

    // actual length
    actualLength =  MSG_SIZE - CMD_ID - LENGTH;
    memcpy(msgLenPtr, &actualLength, LENGTH);

    UDPaddr_size = sizeof(addr_in);

    // send
    sendBytes = sendto(csocket, sendData, MSG_SIZE, 0, (struct sockaddr *)&addr_in, UDPaddr_size);

    // check
    if ( (MSG_SIZE != sendBytes) )
    {
        printf("ERROR! OPEN UDP MESSAGE: sent bytes don't equal message size \n");
        syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u ", __FUNCTION__, __LINE__, sendBytes, MSG_SIZE);
        success = false;
    }

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
    ssize_t retBytes       = 0;
    uint16_t actualLength   = 0;
    uint16_t calcLength     = 0;
    uint8_t retData[ MAXBUFSIZE ];
    uint8_t *ptr;
    socklen_t toRcvUDP_size;

    toRcvUDP_size = sizeof(toRcvUDP);
    
    while (true != gotMsg)
    {
        retBytes = recvfrom(csocket , retData , MAXBUFSIZE , 0 , (struct sockaddr *)&toRcvUDP , &toRcvUDP_size);

        if (retBytes >= MSG_SIZE)
        {
            ptr = retData;

            memcpy(&command, ptr, sizeof(command));
            ptr += CMD_ID;

            memcpy(&actualLength, ptr, LENGTH);
            ptr += LENGTH;

            calcLength = MSG_SIZE - CMD_ID - LENGTH; // should be zero
        }

        if ( (CMD_OPEN == command) && (4 == actualLength) )
        {
            gotMsg = false;
        }
        // check message
        else if ( (MSG_SIZE != retBytes) || ( CMD_SYSINIT != command ) || ( calcLength != actualLength ) )
        {
            gotMsg = false;

            if ( (MSG_SIZE != retBytes) )
            {
                printf("ERROR! SYS INIT message: sent bytes don't equal message size \n");
                syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %zd != %u", __FUNCTION__, __LINE__, retBytes, MSG_SIZE);
            }
            if ( ( CMD_SYSINIT != command ) )
            {
                printf("ERROR! SYS INIT message: invalid command in payload \n");
                syslog(LOG_ERR, "%s:%d ERROR! invalid command %u", __FUNCTION__, __LINE__, command);
            }
            if ( ( calcLength != actualLength ) )
            {
                printf("ERROR! SYS INIT message: payload length incorrect \n");
                syslog(LOG_ERR, "%s:%d ERROR! payload not equal to expected number of bytes %u != %u ",__FUNCTION__, __LINE__, calcLength, actualLength);
            }
        }
        else
        {
            gotMsg = true;
        }
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

    float copAmplitudeHO = 0;
    float copPhaseHO = 0;
    float copPowerHO = 0;
    float copAmplitudeFO = 0;
    float copPhaseFO = 0;
    float copPowerFO = 0;

    float crankAmplitudeHO = 0;
    float crankPhaseHO = 0;
    float crankPowerHO = 0;
    float crankAmplitudeFO = 0;
    float crankPhaseFO = 0;
    float crankPowerFO = 0;

    float turboAmplitudeFO = 0;
    float turboPowerFO = 0;

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
    ptr += CMD_ID;
    cntBytes += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;
    cntBytes += LENGTH;

    val32 = publishMe[ topic_to_pub ].topic_id;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += TOPIC_ID;
    cntBytes += TOPIC_ID;

    val32 = publishMe[ topic_to_pub ].numMPs;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += NUM_MPS;
    cntBytes += NUM_MPS;

    val16 = 0;
    memcpy(ptr, &val16, sizeof(val16));
    ptr += SEQ_NUM;
    cntBytes += SEQ_NUM;

    printf("FROM PROCESS PUBLISH, publishMe[ %d ].numMPs: %d\n", topic_to_pub, publishMe[ topic_to_pub ].numMPs);
    for( i = 0 ; i < publishMe[ topic_to_pub ].numMPs ; i++ )
    {
        val32 = publishMe[ topic_to_pub ].topicSubscription[ i ].mp;
        memcpy(ptr, &val32, sizeof(val32));
        ptr += MP;
        cntBytes += MP;
        for ( j = 0 ; j < publishMe[ topic_to_pub ].topicSubscription[ i ].numSamples ; j++ )
        {
            // logicals
            if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_COP_HALFORDER_AMPLITUDE )
            {
                copAmplitudeHO = getAmplitude(recvFDL[0].cop[0], recvFDL[0].cop[1]);
                memcpy(ptr, &copAmplitudeHO, sizeof(copAmplitudeHO));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_COP_HALFORDER_ENERGY )
            {
                copPowerHO  = getPower(copAmplitudeHO);
                memcpy(ptr, &copPowerHO, sizeof(copPowerHO));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_COP_HALFORDER_PHASE )
            {
                copPhaseHO = getPhase(recvFDL[0].cop[0], recvFDL[0].cop[1]);
                memcpy(ptr, &copPhaseHO, sizeof(copPhaseHO));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_COP_FIRSTORDER_AMPLITUDE )
            {
                copAmplitudeFO = getAmplitude(recvFDL[0].cop[2], recvFDL[0].cop[3]);
                memcpy(ptr, &copAmplitudeFO, sizeof(copAmplitudeFO));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_COP_FIRSTORDER_ENERGY )
            {
                copPowerFO = getPower(copAmplitudeFO);
                memcpy(ptr, &copPowerFO, sizeof(copPowerFO));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_COP_FIRSTORDER_PHASE )
            {
                copPhaseFO = getPhase(recvFDL[0].cop[2], recvFDL[0].cop[3]);
                memcpy(ptr, &copPhaseFO, sizeof(copPhaseFO));
            }
            if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CRANK_HALFORDER_AMPLITUDE )
            {
                crankAmplitudeHO = getAmplitude(recvFDL[0].crank[0], recvFDL[0].crank[1]);
                memcpy(ptr, &crankAmplitudeHO, sizeof(crankAmplitudeHO));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CRANK_HALFORDER_ENERGY )
            {
                crankPowerHO = getPower(crankAmplitudeHO);
                memcpy(ptr, &crankPowerHO, sizeof(crankPowerHO));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CRANK_HALFORDER_PHASE )
            {
                crankPhaseHO = getPhase(recvFDL[0].crank[0], recvFDL[0].crank[1]);
                memcpy(ptr, &crankPhaseHO, sizeof(crankPhaseHO));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CRANK_FIRSTORDER_AMPLITUDE )
            {
                crankAmplitudeFO = getAmplitude(recvFDL[0].crank[2], recvFDL[0].crank[3]);
                memcpy(ptr, &crankAmplitudeFO, sizeof(crankAmplitudeFO));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CRANK_FIRSTORDER_ENERGY )
            {
                crankPowerFO = getPower(crankAmplitudeFO);
                memcpy(ptr, &crankPowerFO, sizeof(crankPowerFO));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CRANK_FIRSTORDER_PHASE )
            {
                crankPhaseFO = getPhase(recvFDL[0].crank[2], recvFDL[0].crank[3]);
                memcpy(ptr, &crankPhaseFO, sizeof(crankPhaseFO));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_TURBO_OIL_FIRSTORDER_AMPLITUDE )
            {
                turboAmplitudeFO = getAmplitude(recvFDL[0].turbo[0], recvFDL[0].turbo[1]);
                memcpy(ptr, &turboAmplitudeFO, sizeof(turboAmplitudeFO));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_TURBO_OIL_FIRSTORDER_ENERGY )
            {
                turboPowerFO = getPower(turboAmplitudeFO);
                memcpy(ptr, &turboPowerFO, sizeof(turboPowerFO));
            }
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

    if ( cntBytes != sendBytes )
    {
        printf("ERROR! PUBLISH: sent bytes don't equal message size\n");
        syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u", __FUNCTION__, __LINE__, sendBytes, cntBytes);
    }
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

        HDR_SIZE                = CMD_ID +
                                  LENGTH +
                                  HOST_OS +
                                  SRC_PROC_ID +
                                  SRC_APP_NAME +
                                  NUM_MPS +
                                  SEQ_NUM,


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
    bool mallocChk = true;

    uint16_t command = 0;

    ssize_t retBytes = 0;
    int32_t  i = 0;
    uint16_t  actualLength = 0;
    uint16_t  calcLength = 0;
    int32_t  calcMPs = 0;
    uint8_t retData[ MAXBUFSIZE ];
    uint8_t *ptr;

    uint8_t host_os;
    uint32_t cnt_retBytes;

    cnt_retBytes = 0;

    errno = 0;

    retBytes = recv(csocket , retData , MAXBUFSIZE , 0 );    
    if (retBytes >= HDR_SIZE)
    {
        ptr = retData;
        memcpy(&command, ptr, sizeof(command));
        ptr += CMD_ID;
        cnt_retBytes += CMD_ID;

        memcpy(&actualLength, ptr, LENGTH);
        ptr += LENGTH;
        cnt_retBytes += LENGTH;

        if (CMD_SUBSCRIBE == command)
        {
            memcpy(&host_os, ptr, sizeof(host_os));
            ptr += HOST_OS;
            cnt_retBytes += HOST_OS;

            ptr += SRC_PROC_ID;
            cnt_retBytes += SRC_PROC_ID;

            memcpy(&src_app_name, ptr, sizeof(src_app_name));
            ptr += SRC_APP_NAME;
            cnt_retBytes += SRC_APP_NAME;

            memcpy(&num_mps, ptr, sizeof(num_mps));
            ptr += NUM_MPS;
            cnt_retBytes += NUM_MPS;

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

            if (true == mallocChk)
            {
                sub_mp = realloc(sub_mp, sizeof(sub_mp)*MPnum);
                if (NULL == sub_mp)
                {
                    mallocChk = false;
                    syslog(LOG_ERR, "%s:%d ERROR! BAD realloc() - sub_mp",__FUNCTION__, __LINE__);
                }
            }

            if (true == mallocChk)
            {
                sub_mpPer = realloc(sub_mpPer, sizeof(sub_mpPer) * MPnum);
                if (NULL == sub_mpPer)
                {
                    mallocChk = false;
                    syslog(LOG_ERR, "%s:%d ERROR! BAD realloc() - sub_mpPer",__FUNCTION__, __LINE__);
                }
            }

            if (true == mallocChk)
            {
                sub_mpNumSamples = realloc(sub_mpNumSamples, sizeof(sub_mpNumSamples) * MPnum);
                if (NULL == sub_mpNumSamples)
                {
                    mallocChk = false;
                    syslog(LOG_ERR, "%s:%d ERROR! BAD realloc() - sub_mpNumSamples",__FUNCTION__, __LINE__);
                }
            }

            if (true == mallocChk)
            {
                for( i = 0 ; i < MPnum ; i++ )
                {
                    memcpy(&sub_mp[i], ptr, sizeof(sub_mp[i]));
                    ptr += MP;
                    cnt_retBytes += MP;

                    memcpy(&sub_mpPer[i], ptr, sizeof(sub_mpPer[i]));
                    ptr += MP_PER;
                    cnt_retBytes += MP_PER;

                    memcpy(&sub_mpNumSamples[i], ptr, sizeof(sub_mpNumSamples[i]));
                    ptr += MP_NUM_SAMPLES;
                    cnt_retBytes += MP_NUM_SAMPLES;
                }
            }
            calcLength = cnt_retBytes - CMD_ID - LENGTH;
        }
    }

    if ( (cnt_retBytes != (uint32_t)retBytes) || ( CMD_SUBSCRIBE != command ) || ( calcLength != actualLength ) || (calcMPs != MPnum) || ( false == mallocChk ) )
    {
        success = false;
        if ( (cnt_retBytes != (uint32_t)retBytes) )
        {
            printf("ERROR! getSUBSCRIBE: bytes received don't equal message size \n");
            syslog(LOG_ERR, "%s:%d ERROR! insufficient message data retBytes %zd != cnt_retBytes %u", __FUNCTION__, __LINE__, retBytes, cnt_retBytes);
        }
        if ( ( CMD_SUBSCRIBE != command ) )
        {
            printf("ERROR! getSUBSCRIBE: invalid command in payload \n");
            syslog(LOG_ERR, "%s:%d ERROR! invalid command %u", __FUNCTION__, __LINE__, command);
        }
        if ( ( calcLength != actualLength ) )
        {
            printf("ERROR! getSUBSCRIBE: payload length incorrect \n");
            syslog(LOG_ERR, "%s:%d payload not equal to expected number of bytes calcLength %u != actualLength %u ",__FUNCTION__, __LINE__, calcLength, actualLength);
        }
        if ( (calcMPs != MPnum) )
        {
            printf("ERROR! getSUBSCRIBE: MPs to receive don't correspond to payload length \n");
            syslog(LOG_ERR, "%s:%d ERROR! length does not coincide with number of MPs: calcMPs %u != MPnum %u", __FUNCTION__, __LINE__, calcMPs, MPnum);
        }
        if ( ( false == mallocChk ) )
        {
            printf("ERROR! BAD realloc() for MPs \n");
            syslog(LOG_ERR, "%s:%d ERROR! BAD realloc() for MPs",__FUNCTION__, __LINE__);
        }
    }
    else
    {
        success = true;
    }
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
    uint16_t actualLength   = 0;
    uint8_t sendData[ MSG_SIZE ];

    ptr = sendData;
    val16 = CMD_SUBSCRIBE;
    memcpy(ptr, &val16, sizeof(val16));
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    *ptr = PETALINUX;
    ptr += HOST_OS;

    memcpy(ptr, &fdlPid, SRC_PROC_ID);
    ptr += SRC_PROC_ID;

    memcpy(ptr, &fdlAppName, SRC_APP_NAME);
    ptr += SRC_APP_NAME;

    val32 = MAX_fdl_SUBSCRIPTION;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += NUM_MPS;

    val32 = 0;
    memcpy(ptr, &val32, sizeof(val32));
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
    memcpy(msgLenPtr, &actualLength, LENGTH);

    // sending
    sendBytes = send( csocket, sendData, MSG_SIZE, 0 );

    if (MSG_SIZE != sendBytes)
    {
        printf("ERROR! sendSUBSCRIBE: sent bytes don't equal message size\n");
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u",__FUNCTION__, __LINE__, sendBytes, MSG_SIZE);
    }
    else
    {
        success = true;
    }

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
    uint8_t retData[ MSG_SIZE ];
    ssize_t retBytes = 0;

    uint32_t i              = 0;

    uint16_t command        = 0;
    uint16_t actualLength   = 0;
    uint16_t calcLength     = 0;
    int16_t genErr          = 0;
    int16_t msgErr          = 0;
    int32_t errCnt;

    errCnt = 0;

    retBytes = recv( csocket , retData , MSG_SIZE , 0 );

    if ((MSG_SIZE == retBytes))
    {
        ptr = retData;
        memcpy(&command, ptr, sizeof(command));
        ptr += CMD_ID;

        memcpy(&actualLength, ptr, LENGTH);
        ptr += LENGTH;

        memcpy(&fromSubAckTopicID, ptr, sizeof(fromSubAckTopicID));
        ptr += TOPIC_ID;

        memcpy(&genErr, ptr, sizeof(genErr));
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

    if ( (MSG_SIZE != retBytes) || ( errCnt > 0 ) || ( CMD_SUBSCRIBE_ACK != command ) || ( calcLength != actualLength ) )
    {
        success = false;
        if ( (MSG_SIZE != retBytes) )
        {
            printf("ERROR! getSUBSCRIBE ACK: bytes received don't equal message size \n");
            syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %zd != %u", __FUNCTION__, __LINE__, retBytes, MSG_SIZE);
        }
        if ( ( errCnt > 0 ) )
        {
            printf("ERROR! getSUBSCRIBE ACK: error detected in payload \n");
            syslog(LOG_ERR, "%s:%d ERROR! MP errors received: %u", __FUNCTION__, __LINE__, errCnt);
        }
        if ( ( CMD_SUBSCRIBE_ACK != command ) )
        {
            printf("ERROR! getSUBSCRIBE ACK: invalid command in payload \n");
            syslog(LOG_ERR, "%s:%d ERROR! invalid command %u", __FUNCTION__, __LINE__, command);
        }
        if ( ( calcLength != actualLength ) )
        {
            printf("ERROR! getSUBSCRIBE ACK: payload length incorrect \n");
            syslog(LOG_ERR, "%s:%d ERROR! payload not equal to expected number of bytes %u != %u ",__FUNCTION__, __LINE__, calcLength, actualLength);
        }
    }
    else
    {
        success = true;
    }
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

        HDR_SIZE                = CMD_ID + LENGTH,

        TOPIC_ID                = 4,
        NUM_MPS                 = 4,
        SEQ_NUM                 = 2,
        
        MP                      = 4,
        MP_VAL                  = 4,

        MSG_SIZE                =   HDR_SIZE +
                                    TOPIC_ID +
                                    NUM_MPS +
                                    SEQ_NUM +
                                    10*(MP + MP_VAL),
    };

    bool success = true;

    ssize_t retBytes       = 0;
    int32_t i               = 0;
    int32_t cntMPiteration  = 0;
    int32_t MPnum_fromPub   = 0;

    uint16_t command        = 0;
    uint16_t actualLength   = 0;
    int32_t topicID         = 0;
    uint16_t seqNum         = 0;

    uint16_t calcLength     = 0;
    int32_t calcMPs         = 0;
    uint8_t retData[ MAXBUFSIZE ];
    uint8_t *ptr;

    uint32_t cnt_retBytes;

    cnt_retBytes = 0;

    retBytes = recv(csocket , retData , MAXBUFSIZE , 0 );
    if (retBytes == MSG_SIZE)
    {
        ptr = retData;
        memcpy(&command, ptr, sizeof(command));
        ptr += CMD_ID;
        cnt_retBytes += CMD_ID;

        memcpy(&actualLength, ptr, LENGTH);
        ptr += LENGTH;
        cnt_retBytes += LENGTH;

        if (CMD_PUBLISH == command)
        {
            // check if this topic ID mathces the one from sub_ack ...
            memcpy(&topicID, ptr, sizeof(topicID));
            ptr += TOPIC_ID;
            cnt_retBytes += TOPIC_ID;

            /* Don't bother parsing the message if it isn't the PUBLISH we are expecting */
            if ( topicID == fromSubAckTopicID )
            {
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

                calcLength = cnt_retBytes - HDR_SIZE;

                printf("GET PUB topicID: %d\n", topicID);
                printf("GET PUB fromSubAckTopicID: %d\n", fromSubAckTopicID);

                if ( (cnt_retBytes != (uint32_t)retBytes) || ( calcLength != actualLength ) || (calcMPs != MPnum_fromPub) || (cntMPiteration != MPnum_fromPub) )
                {
                    success = false;
                    if ( (cnt_retBytes != (uint32_t)retBytes) )
                    {
                        printf("ERROR! getPUBLISH: bytes received don't equal message size \n");
                        syslog(LOG_ERR, "%s:%d ERROR! insufficient message data retBytes %zd != cnt_retBytes %u", __FUNCTION__, __LINE__, retBytes, cnt_retBytes);
                    }
                    if ( ( calcLength != actualLength ) )
                    {
                        printf("ERROR! getPUBLISH: payload length incorrect \n");
                        syslog(LOG_ERR, "%s:%d payload not equal to expected number of bytes calcLength %u != actualLength %u ",__FUNCTION__, __LINE__, calcLength, actualLength);
                    }

                    if ( (calcMPs != MPnum_fromPub) )
                    {
                        printf("ERROR! getPUBLISH: MPs to receive and calculated message size dont' match \n");
                        syslog(LOG_ERR, "%s:%d ERROR! length does not coincide with number of MPs: calcMPs %u != MPnum_fromPub %u", __FUNCTION__, __LINE__, calcMPs, MPnum_fromPub);
                    }

                    if ( (cntMPiteration != MPnum_fromPub) )
                    {
                        printf("ERROR! getPUBLISH: MPs to receive and MP iteration don't match \n");
                        syslog(LOG_ERR, "%s:%d ERROR! MPs received dont' match: cntMPiteration %u != MPnum_fromPub %u", __FUNCTION__, __LINE__, cntMPiteration, MPnum_fromPub);
                    }
                }
                else
                {
                    success = true;
                }
            }
        }
    }

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

    int32_t cntBytes    = 0;
    int32_t sendBytes   = 0;
    uint32_t i          = 0;
    int16_t genErr      = GE_SUCCESS;


    ptr = sendData;
    val16 = CMD_SUBSCRIBE_ACK;
    memcpy(ptr, &val16, sizeof(val16));
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
        }
        else
        {
            val16 = GE_INVALID_MP_NUMBER;
            memcpy(ptr, &val16, sizeof(val16));
            genErr = GE_INVALID_MP_NUMBER;
            success = false;
        }
        ptr += ERROR_MP;
        cntBytes += ERROR_MP;
    }

    actualLength = cntBytes - CMD_ID - LENGTH;

    // why do I get errors when I rearrange these?  e.g. memcpy topic ID first, then length, then error.
    // I get the right stuff when I memcpy in the order they were assigned above ... why?
    memcpy(msgLenPtr,   &actualLength,                          sizeof(uint16_t));
    memcpy(topicIDptr,  &publishMe[ currentTopic ].topic_id,    sizeof(uint32_t));
    memcpy(msgErrPtr,   &genErr,                                sizeof(int16_t));

    // send
    sendBytes       = send(csocket, sendData, cntBytes, 0);

    // check message
    if ( (cntBytes != sendBytes) || ( GE_INVALID_MP_NUMBER == genErr ) )
    {
        success = false;
        if ( cntBytes != sendBytes )
        {
            printf("ERROR! sendSUBSCRIBE ACK: sent bytes don't equal message size\n");
            syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u ", __FUNCTION__, __LINE__, sendBytes, cntBytes);
        }

        if ( GE_INVALID_MP_NUMBER == genErr )
        {
            printf("ERROR! sendSUBSCRIBE ACK: invalid MP \n");
            syslog(LOG_ERR, "%s:%d ERROR! MP subscription error: %u", __FUNCTION__, __LINE__, genErr );
        }
    }

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
    ptr += CMD_ID;
    cntBytes += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;
    cntBytes += LENGTH;

    val32 = HeartBeat;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += HRTBT_CNT;
    cntBytes += HRTBT_CNT;

    actualLength = cntBytes - CMD_ID - LENGTH;
    memcpy(msgLenPtr, &actualLength, sizeof(uint16_t));
    sendBytes = send(csocket, sendData, cntBytes, 0);

    // check message
    if ( (cntBytes != sendBytes) )
    {
        success = false;
        printf("ERROR! sendHEARBEAT: sent bytes don't equal message size\n");
        syslog(LOG_ERR, "%s:%d ERROR! heartbeat, insufficient message data %u != %u ", __FUNCTION__, __LINE__, sendBytes, cntBytes);
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
    uint32_t i;
    uint32_t  k;
    int32_t periodVar;
    int32_t numMPsMatching;
    int32_t numSamplesToChk, remainder;

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

    // for new topic/subscription
    publishMe = realloc(publishMe, sizeof(topicToPublish)*num_topics_total);
    if (NULL == publishMe)
    {
        printf("ERROR! MALLOC error when building publish data - number of topics\n");
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! BAD realloc()",__FUNCTION__, __LINE__);
    }
    else
    {
        publishMe[ currentTopic ].app_name     = src_app_name;
        publishMe[ currentTopic ].numMPs       = MPnum;
        publishMe[ currentTopic ].publishReady = false;

        // for MPs
        publishMe[ currentTopic ].topicSubscription = malloc(sizeof(MPinfo)*publishMe[ currentTopic ].numMPs);
        if (NULL == publishMe[ currentTopic ].topicSubscription)
        {
            printf("ERROR! MALLOC error when building publish data - number of MPs \n");
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! BAD malloc()",__FUNCTION__, __LINE__);
        }
    }

    if (true == success)
    {
        publishMe[ currentTopic ].topicSubscription = malloc(sizeof(MPinfo)*publishMe[ currentTopic ].numMPs);
        if (NULL == publishMe[ currentTopic ].topicSubscription)
        {
            printf("ERROR! MALLOC error when building publish data - number of MPs\n");
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! BAD malloc()",__FUNCTION__, __LINE__);
        }
    }

    if (true == success)
    {
        for( k = 0 ; (k < publishMe[ currentTopic ].numMPs) && (true == success) ; k++ )
        {
            publishMe[ currentTopic ].topicSubscription[ k ].mp             = sub_mp[ k ];
            publishMe[ currentTopic ].topicSubscription[ k ].mp_val_float   = NULL;
            publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long    = NULL;
            publishMe[ currentTopic ].topicSubscription[ k ].period         = sub_mpPer[ k ];
            publishMe[ currentTopic ].topicSubscription[ k ].numSamples     = sub_mpNumSamples[ k ];
            publishMe[ currentTopic ].topicSubscription[ k ].valid          = false;

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
            {
                publishMe[ currentTopic ].topicSubscription[ k ].logical = true;

                publishMe[ currentTopic ].topicSubscription[ k ].mp_val_float = (float*) calloc(sub_mpNumSamples[k], sizeof(float));
                if (NULL == publishMe[ currentTopic ].topicSubscription[ k ].mp_val_float)
                {
                    printf("ERROR! MALLOC error when building publish data - number of MPs\n");
                    success = false;
                    syslog(LOG_ERR, "%s:%d ERROR! BAD malloc()",__FUNCTION__, __LINE__);
                }
            }
            else /* timestamp */
            {
                publishMe[ currentTopic ].topicSubscription[ k ].logical = false;

                publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long = (uint32_t*) calloc(sub_mpNumSamples[k], sizeof(uint32_t));  // 18 = 9 seconds, 9 nanoseconds.  This is the max potential number of timestamps per second.
                if (NULL == publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long)
                {
                    printf("ERROR! MALLOC error when building publish data - number of samples \n");
                    success = false;
                    syslog(LOG_ERR, "%s:%d ERROR! BAD malloc()",__FUNCTION__, __LINE__);
                }
            } /* else timestamp */
        } /* for( k = 0 ; (k < publishMe[ currentTopic ].numMPs) && (true == success) ; k++ ) */
    } /* if (true == success) */

    if (true == success)
    {
        for( k = 0 ; (k < publishMe[ currentTopic ].numMPs) && (true == success) ; k++ )
        {
            numMPsMatching = 0;
            for( i = 0 ; i < MAX_fdl_TO_PUBLISH ; i++ )
            {
                if( fdlsubscriptionMP[ i ] == publishMe[ currentTopic ].topicSubscription[ k ].mp )
                {
                    numMPsMatching++;
                }
            }
#if 0
                    syslog(LOG_DEBUG, "%s:%d sub[%d][%d] (%d) match %d found at index %d",
                           __FUNCTION__, __LINE__, currentTopic, k,
                           fdlsubscriptionMP[i], numMPsMatching, i);
#endif            

            // checks numer of samples requested based on period requested and whatever the minimum period is configured.
            numSamplesToChk = publishMe[currentTopic].topicSubscription[k].numSamples * MINPER;
            remainder = publishMe[currentTopic].topicSubscription[k].period % MINPER;

#if 0
            syslog(LOG_DEBUG, "%s:%d sub[%d][%d] samples %d * %d = %d ?= period %d (remainder %d)",
                   __FUNCTION__, __LINE__, currentTopic, k,
                   publishMe[currentTopic].topicSubscription[k].numSamples, MINPER, numSamplesToChk,
                   publishMe[currentTopic].topicSubscription[k].period, remainder);
#endif

            if ( 0 == remainder )
            {
                if ( numSamplesToChk != publishMe[currentTopic].topicSubscription[k].period )
                {
                    printf("INVALID SUBSCRIPTION: MP number of samples doesn't correspond to the period requested \n");
                    syslog(LOG_ERR, "%s:%d INVALID SUBSCRIPTION: sub[%d][%d] MP %d: invalid period %d * %d != %d",
                           __FUNCTION__, __LINE__, currentTopic, k,
                           publishMe[currentTopic].topicSubscription[k].mp,
                           publishMe[currentTopic].topicSubscription[k].numSamples, MINPER,
                           publishMe[currentTopic].topicSubscription[k].period);
                    numSamplesToChk = 0;
                }
            }
            else
            {
                printf("INVALID SUBSCRIPTION: MP PERIOD not integer multiple of minimum period allowed \n");
                syslog(LOG_ERR, "%s:%d INVALID SUBSCRIPTION: sub[%d][%d] MP %d: invalid period %d %% %d != 0",
                       __FUNCTION__, __LINE__, currentTopic, k,
                       publishMe[currentTopic].topicSubscription[k].mp,
                       publishMe[currentTopic].topicSubscription[k].period, MINPER);
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
        } /* for( k = 0 ; (k < publishMe[ currentTopic ].numMPs) && (true == success) ; k++ ) */

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
                printf("ERROR! when building publish data - invalid subscription \n");
                publishMe[ currentTopic ].topicSubscription[ i ].valid = false;
                syslog(LOG_ERR, "%s:%d INVALID SUBSCRIPTION",__FUNCTION__, __LINE__);
                publishMe[ currentTopic ].period    = 0;
                publishMe[ currentTopic ].topic_id  = -1;
            }
        }

        prevPeriodChk  = publishMe[ currentTopic ].topicSubscription[ 0 ].period;
    } /* if (true == success) */

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
    uint32_t i, new_offset;
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
    uint32_t i;

    // only happens once.
    if ( 0 == nextPublishPeriod )
    {
        nextPublishPeriod = 1000;
    }


    for( i = 0 ; i < num_topics_total ; i++ )
    {
        if ( (0 != publishMe[i].period) && (( nextPublishPeriod % publishMe[i].period ) == 0) )
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

float getPower(float amplitude)
{
    float power;

    if (0 == amplitude)
    {
        power = 0;
    }
    else
    {
        power = pow(amplitude, 2) / 2;
    }

    return power;
}

