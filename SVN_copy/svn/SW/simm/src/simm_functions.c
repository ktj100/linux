
/** @file simm_functions.c
 * Functions used send, receive, and package SIMM/FPGA data.
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
#include "simm_functions.h"
#include "sensor.h"


/****************
* GLOBALS
****************/
#define MAXBUFSIZE  1000

extern int32_t clientSocket;
struct sockaddr_storage serverStorage_UDP;
struct sockaddr_storage toRcvUDP;
struct sockaddr_storage toSendUDP;

int32_t num_mps;
int32_t MPnum;
int32_t *sub_mp = NULL;
int32_t *sub_mpPer = NULL;
uint32_t *sub_mpNumSamples = NULL;
int32_t subAppName;

uint32_t num_topics_total = 0;
uint32_t num_topics_atCurrentRate;
uint32_t currentTopic = 0;
int32_t maxPublishPeriod;
int32_t prevPeriodChk;
int32_t nextPublishPeriod;

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
    uint8_t sendData[ MAXBUFSIZE ];
    struct timespec endTime;

    ptr = sendData;
    val16 = CMD_REGISTER_APP;
    memcpy(ptr, &val16, CMD_ID);
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    *ptr = PETALINUX;
    ptr += HOST_OS;

    memcpy(ptr, &simmPid, SRC_PROC_ID);
    ptr += SRC_PROC_ID;

    memcpy(ptr, simmAppName, SRC_APP_NAME);
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
    else
    {
        syslog(LOG_DEBUG, "%s:%d REGISTER APP SUCCESS",__FUNCTION__, __LINE__);
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
    uint8_t retData[ MAXBUFSIZE ];    // or whatever max is defined
    uint16_t appAckErr = 0;
    uint8_t *ptr;
    uint16_t actualLength = 0;

    int32_t calcLength = 0;

    errno = 0;
    retBytes = recv(csocket, retData , MSG_SIZE , 0 );
    if ( -1 == retBytes )
    {
        syslog(LOG_ERR, "%s:%d ERROR: In recv()! (%d: %s) ",
               __FUNCTION__, __LINE__, errno, strerror(errno));
    }
    else if (0 == retBytes)
    {
        syslog(LOG_ERR, "%s:%d ERROR: 0 bytes received in recv()! (%d: %s) ", \
               __FUNCTION__, __LINE__, errno, strerror(errno));
    }
    else if (retBytes == MSG_SIZE)
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
        syslog(LOG_DEBUG, "%s:%d REGISTER APP ACK SUCCESS",__FUNCTION__, __LINE__);
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
        PFP_VALUE               = 4,
        PTLT_TEMPERATURE        = 4,
        PTRT_TEMPERATURE        = 4,
        TCMP                    = 4,
        COP_PRESSURE            = 4,
        COP_FO_REAL             = 4,
        COP_FO_IMAG             = 4,
        //COP_FO_AMPLITUDE        = 4,
        //COP_FO_ENERGY           = 4,
        //COP_FO_PHASE            = 4,
        COP_HO_REAL             = 4,
        COP_HO_IMAG             = 4,
        //COP_HO_AMPLITUDE        = 4,
        //COP_HO_ENERGY           = 4,
        //COP_HO_PHASE            = 4,
        CRANK_FO_REAL           = 4,
        CRANK_FO_IMAG           = 4,
        //CRANK_FO_AMPLITUDE      = 4,
        //CRANK_FO_ENERGY         = 4,
        //CRANK_FO_PHASE          = 4,
        CRANK_HO_REAL           = 4,
        CRANK_HO_IMAG           = 4,
        //CRANK_HO_AMPLITUDE      = 4,
        //CRANK_HO_ENERGY         = 4,
        //CRANK_HO_PHASE          = 4,
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
        TURBO_REAL              = 4,
        TURBO_IMAG              = 4,
        //TURBO_FO_AMPLITUDE      = 4,
        //TURBO_FO_ENERGY         = 4,
        MSG_SIZE                =   CMD_ID                  +
                                    LENGTH                  +
                                    HOST_OS                 +
                                    SRC_PROC_ID             +
                                    SRC_APP_NAME            +
                                    NUM_MPS                 +
                                    PFP_VALUE               +
                                    PTLT_TEMPERATURE        +
                                    PTRT_TEMPERATURE        +
                                    TCMP                    +
                                    COP_PRESSURE            +
                                    COP_FO_REAL             +
                                    COP_FO_IMAG             +
                                    //COP_FO_AMPLITUDE        +
                                    //COP_FO_ENERGY           +
                                    //COP_FO_PHASE            +
                                    COP_HO_REAL             +
                                    COP_HO_IMAG             +
                                    //COP_HO_AMPLITUDE        +
                                    //COP_HO_ENERGY           +
                                    //COP_HO_PHASE            +
                                    CRANK_FO_REAL           +
                                    CRANK_FO_IMAG           +
                                    //CRANK_FO_AMPLITUDE      +
                                    //CRANK_FO_ENERGY         +
                                    //CRANK_FO_PHASE          +
                                    CRANK_HO_REAL           +
                                    CRANK_HO_IMAG           +
                                    //CRANK_HO_AMPLITUDE      +
                                    //CRANK_HO_ENERGY         +
                                    //CRANK_HO_PHASE          +
                                    CAM_SEC_1               +
                                    CAM_NSEC_1              +
                                    CAM_SEC_2               +
                                    CAM_NSEC_2              +
                                    CAM_SEC_3               +
                                    CAM_NSEC_3              +
                                    CAM_SEC_4               +
                                    CAM_NSEC_4              +
                                    CAM_SEC_5               +
                                    CAM_NSEC_5              +
                                    CAM_SEC_6               +
                                    CAM_NSEC_6              +
                                    CAM_SEC_7               +
                                    CAM_NSEC_7              +
                                    CAM_SEC_8               +
                                    CAM_NSEC_8              +
                                    CAM_SEC_9               +
                                    CAM_NSEC_9              +
                                    TURBO_REAL              +
                                    TURBO_IMAG              ,
                                    //TURBO_FO_AMPLITUDE      +
                                    //TURBO_FO_ENERGY,
    };

    bool success = true;
    int32_t sendBytes = 0;
    uint8_t *ptr;
    int16_t val16;
    int32_t val32;
    uint8_t *msgLenPtr;
    uint16_t actualLength = 0;
    uint8_t sendData[ MAXBUFSIZE ];

    ptr = sendData;
    val16 = CMD_REGISTER_DATA;
    memcpy(ptr, &val16, sizeof(val16));
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    *ptr = PETALINUX;
    ptr += HOST_OS;

    memcpy(ptr, &simmPid, SRC_PROC_ID);
    ptr += SRC_PROC_ID;

    memcpy(ptr, simmAppName, SRC_APP_NAME);
    ptr += SRC_APP_NAME;

    val32 = MAX_SIMM_SUBSCRIPTION;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += NUM_MPS;

    val32 = MP_PFP_VALUE;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += PFP_VALUE;

    val32 = MP_PTLT_TEMPERATURE;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += PTLT_TEMPERATURE;

    val32 = MP_PTRT_TEMPERATURE;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += PTRT_TEMPERATURE;

    val32 = MP_TCMP;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += TCMP;

    val32 = MP_COP_HO_REAL;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += COP_HO_REAL;

    val32 = MP_COP_HO_IMAG;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += COP_HO_IMAG;

    val32 = MP_COP_FO_REAL;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += COP_FO_REAL;

    val32 = MP_COP_FO_IMAG;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += COP_FO_IMAG;

    val32 = MP_CRANK_HO_REAL;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CRANK_HO_REAL;

    val32 = MP_CRANK_HO_IMAG;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CRANK_HO_IMAG;

    val32 = MP_CRANK_FO_REAL;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CRANK_FO_REAL;

    val32 = MP_CRANK_FO_IMAG;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CRANK_FO_IMAG;

    val32 = MP_TURBO_REAL;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += TURBO_REAL;

    val32 = MP_TURBO_IMAG;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += TURBO_IMAG;

    val32 = MP_CAM_SEC_1;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_SEC_1;

    val32 = MP_CAM_NSEC_1;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_NSEC_1;

    val32 = MP_CAM_SEC_2;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_SEC_2;

    val32 = MP_CAM_NSEC_2;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_NSEC_2;

    val32 = MP_CAM_SEC_3;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_SEC_3;

    val32 = MP_CAM_NSEC_3;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_NSEC_3;

    val32 = MP_CAM_SEC_4;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_SEC_4;

    val32 = MP_CAM_NSEC_4;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_NSEC_4;

    val32 = MP_CAM_SEC_5;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_SEC_5;

    val32 = MP_CAM_NSEC_5;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_NSEC_5;

    val32 = MP_CAM_SEC_6;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_SEC_6;

    val32 = MP_CAM_NSEC_6;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_NSEC_6;

    val32 = MP_CAM_SEC_7;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_SEC_7;

    val32 = MP_CAM_NSEC_7;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_NSEC_7;

    val32 = MP_CAM_SEC_8;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_SEC_8;

    val32 = MP_CAM_NSEC_8;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_NSEC_8;

    val32 = MP_CAM_SEC_9;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_SEC_9;

    val32 = MP_CAM_NSEC_9;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += CAM_NSEC_9;

    val32 = MP_COP_PRESSURE;
    memcpy(ptr, &val32, sizeof(val32));
    ptr += COP_PRESSURE;

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
        syslog(LOG_DEBUG, "%s:%d REGISTER DATA SUCCESS",__FUNCTION__, __LINE__);
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
        ERROR_PFP_VALUE             = 2,
        ERROR_PTLT_TEMPERATURE      = 2,
        ERROR_PTRT_TEMPERATURE      = 2,
        ERROR_TCMP                  = 2,
        ERROR_COP_PRESSURE          = 2,
        ERROR_COP_FO_REAL           = 2,
        ERROR_COP_FO_IMAG           = 2,
        //ERROR_COP_FO_AMPLITUDE      = 2,
        //ERROR_COP_FO_ENERGY         = 2,
        //ERROR_COP_FO_PHASE          = 2,
        ERROR_COP_HO_REAL           = 2,
        ERROR_COP_HO_IMAG           = 2,
        //ERROR_COP_HO_AMPLITUDE      = 2,
        //ERROR_COP_HO_ENERGY         = 2,
        //ERROR_COP_HO_PHASE          = 2,
        ERROR_CRANK_FO_REAL         = 2,
        ERROR_CRANK_FO_IMAG         = 2,
        //ERROR_CRANK_FO_AMPLITUDE    = 2,
        //ERROR_CRANK_FO_ENERGY       = 2,
        //ERROR_CRANK_FO_PHASE        = 2,
        ERROR_CRANK_HO_REAL         = 2,
        ERROR_CRANK_HO_IMAG         = 2,
        //ERROR_CRANK_HO_AMPLITUDE    = 2,
        //ERROR_CRANK_HO_ENERGY       = 2,
        //ERROR_CRANK_HO_PHASE        = 2,
        ERROR_CAM_SEC_1             = 2,
        ERROR_CAM_NSEC_1            = 2,
        ERROR_CAM_SEC_2             = 2,
        ERROR_CAM_NSEC_2            = 2,
        ERROR_CAM_SEC_3             = 2,
        ERROR_CAM_NSEC_3            = 2,
        ERROR_CAM_SEC_4             = 2,
        ERROR_CAM_NSEC_4            = 2,
        ERROR_CAM_SEC_5             = 2,
        ERROR_CAM_NSEC_5            = 2,
        ERROR_CAM_SEC_6             = 2,
        ERROR_CAM_NSEC_6            = 2,
        ERROR_CAM_SEC_7             = 2,
        ERROR_CAM_NSEC_7            = 2,
        ERROR_CAM_SEC_8             = 2,
        ERROR_CAM_NSEC_8            = 2,
        ERROR_CAM_SEC_9             = 2,
        ERROR_CAM_NSEC_9            = 2,
        ERROR_TURBO_REAL            = 2,
        ERROR_TURBO_IMAG            = 2,
        //ERROR_TURBO_AMPLITUDE       = 2,
        //ERROR_TURBO_ENERGY          = 2,
        MSG_SIZE                =   CMD_ID                      +
                                    LENGTH                      +
                                    ERROR                       +
                                    ERROR_PFP_VALUE             +
                                    ERROR_PTLT_TEMPERATURE      +
                                    ERROR_PTRT_TEMPERATURE      +
                                    ERROR_TCMP                  +
                                    ERROR_COP_PRESSURE          +
                                    ERROR_COP_FO_REAL           +
                                    ERROR_COP_FO_IMAG           +
                                    //ERROR_COP_FO_AMPLITUDE      +
                                    //ERROR_COP_FO_ENERGY         +
                                    //ERROR_COP_FO_PHASE          +
                                    ERROR_COP_HO_REAL           +
                                    ERROR_COP_HO_IMAG           +
                                    //ERROR_COP_HO_AMPLITUDE      +
                                    //ERROR_COP_HO_ENERGY         +
                                    //ERROR_COP_HO_PHASE          +
                                    ERROR_CRANK_FO_REAL         +
                                    ERROR_CRANK_FO_IMAG         +
                                    //ERROR_CRANK_FO_AMPLITUDE    +
                                    //ERROR_CRANK_FO_ENERGY       +
                                    //ERROR_CRANK_FO_PHASE        +
                                    ERROR_CRANK_HO_REAL         +
                                    ERROR_CRANK_HO_IMAG         +
                                    //ERROR_CRANK_HO_AMPLITUDE    +
                                    //ERROR_CRANK_HO_ENERGY       +
                                    //ERROR_CRANK_HO_PHASE        +
                                    ERROR_CAM_SEC_1             +
                                    ERROR_CAM_NSEC_1            +
                                    ERROR_CAM_SEC_2             +
                                    ERROR_CAM_NSEC_2            +
                                    ERROR_CAM_SEC_3             +
                                    ERROR_CAM_NSEC_3            +
                                    ERROR_CAM_SEC_4             +
                                    ERROR_CAM_NSEC_4            +
                                    ERROR_CAM_SEC_5             +
                                    ERROR_CAM_NSEC_5            +
                                    ERROR_CAM_SEC_6             +
                                    ERROR_CAM_NSEC_6            +
                                    ERROR_CAM_SEC_7             +
                                    ERROR_CAM_NSEC_7            +
                                    ERROR_CAM_SEC_8             +
                                    ERROR_CAM_NSEC_8            +
                                    ERROR_CAM_SEC_9             +
                                    ERROR_CAM_NSEC_9            +
                                    ERROR_TURBO_REAL            +
                                    ERROR_TURBO_IMAG            ,
                                    //ERROR_TURBO_AMPLITUDE       +
                                    //ERROR_TURBO_ENERGY,
    };

    bool success = true;

    ssize_t  retBytes               = 0;
    uint32_t errCnt                 = 0;
    uint16_t actualLength           = 0;
    uint32_t calcLength             = 0;
    uint32_t i                      = 0;
    uint8_t retData[ MAXBUFSIZE ];
    uint8_t *ptr;

    uint16_t command                = 0;
    uint16_t genErr                 = 0;
    uint16_t pfpErr                 = 0;
    uint16_t ptltErr                = 0;
    uint16_t ptrtErr                = 0;
    uint16_t tcmpErr                = 0;
    uint16_t copErr                 = 0;
    uint16_t copFoRealErr           = 0;
    uint16_t copFoImagErr           = 0;
    uint16_t copHoRealErr           = 0;
    uint16_t copHoImagErr           = 0;
    uint16_t crankFoRealErr         = 0;
    uint16_t crankFoImagErr         = 0;
    uint16_t crankHoRealErr         = 0;
    uint16_t crankHoImagErr         = 0;
    uint16_t camSecErr[ MAX_TIMESTAMPS ];
    uint16_t camNSecErr[ MAX_TIMESTAMPS ];
    uint16_t turboRealErr           = 0;
    uint16_t turboImagErr           = 0;

    errCnt = 0;

    retBytes = recv( csocket , retData , MSG_SIZE , 0 );
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

        memcpy(&pfpErr, ptr, sizeof(pfpErr));
        if (0 != pfpErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP pfpErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! pfpErr",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_PFP_VALUE;

        memcpy(&ptltErr, ptr, sizeof(ptltErr));
        if (0 != ptltErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP ptltErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! ptltErr",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_PTLT_TEMPERATURE;

        memcpy(&ptrtErr, ptr, sizeof(ptrtErr));
        if (0 != ptrtErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP ptrtErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! ptrtErr",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_PTRT_TEMPERATURE;

        memcpy(&tcmpErr, ptr, sizeof(tcmpErr));
        if (0 != tcmpErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP tcmpErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! tcmpErr",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_TCMP;

        memcpy(&copErr, ptr, sizeof(copErr));
        if (0 != copErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP copErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! copErr",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_COP_PRESSURE;


        // COP
        memcpy(&copFoRealErr, ptr, sizeof(copFoRealErr));
        if (0 != copFoRealErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP copFoRealErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copFoRealErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_COP_FO_REAL;

        memcpy(&copFoImagErr, ptr, sizeof(copFoImagErr));
        if (0 != copFoImagErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP copFoImagErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copFoImagErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_COP_FO_IMAG;

        memcpy(&copHoRealErr, ptr, sizeof(copHoRealErr));
        if (0 != copHoRealErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP copHoRealErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copHoRealErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_COP_HO_REAL;

        memcpy(&copHoImagErr, ptr, sizeof(copHoImagErr));
        if (0 != copHoImagErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP copHoImagErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, copHoImagErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_COP_HO_IMAG;

        // CRANK
        memcpy(&crankFoRealErr, ptr, sizeof(crankFoRealErr));
        if (0 != crankFoRealErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP crankFoRealErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankFoRealErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_CRANK_FO_REAL;

        memcpy(&crankFoImagErr, ptr, sizeof(crankFoImagErr));
        if (0 != crankFoImagErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP crankFoImagErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankFoImagErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_CRANK_FO_IMAG;

        memcpy(&crankHoRealErr, ptr, sizeof(crankHoRealErr));
        if (0 != crankHoRealErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP crankHoRealErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankHoRealErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_CRANK_HO_REAL;

        memcpy(&crankHoImagErr, ptr, sizeof(crankHoImagErr));
        if (0 != crankHoImagErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP crankHoImagErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR, crankHoImagErr!",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_CRANK_HO_IMAG;

        // TIMESTAMPS
        for( i = 0 ; i < MAX_TIMESTAMPS ; i++ )
        {
            memcpy(&camSecErr[i], ptr, sizeof(camSecErr[i]));
            if (0 != camSecErr[i])
            {
                printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP camSecErr \n");
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! camSecErr%d",__FUNCTION__, __LINE__, i);
                errCnt++;
            }
            ptr += ERROR_CAM_SEC_1;

            memcpy(&camNSecErr[i], ptr, sizeof(camNSecErr[i]));
            if (0 != camNSecErr[i])
            {
                printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP camNSecErr \n");
                syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! camNSecErr%d",__FUNCTION__, __LINE__, i);
                errCnt++;
            }
            ptr += ERROR_CAM_NSEC_1;
        }

        // TURBO
        memcpy(&turboRealErr, ptr, sizeof(turboRealErr));
        if (0 != turboRealErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP turboRealErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! turboRealErr",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_TURBO_REAL;

        memcpy(&turboImagErr, ptr, sizeof(turboImagErr));
        if (0 != turboImagErr)
        {
            printf("ERROR! REGISTER DATA ACNKOWLEDGE: MP turboImagErr \n");
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK MP ERROR! turboImagErr",__FUNCTION__, __LINE__);
            errCnt++;
        }
        ptr += ERROR_TURBO_IMAG;

        // check: receive message size, error, or not the command ...
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
        syslog(LOG_DEBUG, "%s:%d REGISTER DATA ACK SUCCESS",__FUNCTION__, __LINE__);
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
    uint8_t sendData[ MAXBUFSIZE ];

    socklen_t UDPaddr_size;

    ptr = sendData;
    val16 = CMD_OPEN;
    memcpy(ptr, &val16, sizeof(val16));
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    memcpy(ptr, &simmPid, APP_ID);
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
    else
    {
        syslog(LOG_DEBUG, "%s:%d OPEN UDP SUCCESS",__FUNCTION__, __LINE__);
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
    ssize_t  retBytes       = 0;
    uint16_t actualLength   = 0;
    uint16_t calcLength     = 0;
    uint8_t retData[ MAXBUFSIZE ];
    uint8_t *ptr;
    socklen_t toRcvUDP_size;

    toRcvUDP_size = sizeof(toRcvUDP);

    while (true != gotMsg)
    {
        retBytes = recvfrom(csocket , retData , MSG_SIZE , 0 , (struct sockaddr *)&toRcvUDP , &toRcvUDP_size);
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
            syslog(LOG_DEBUG, "%s:%d SYS INT COMPLETE SUCCESS",__FUNCTION__, __LINE__);
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
void process_publish( int32_t csocket , struct sockaddr_in addr_in , int32_t topic_to_pub )
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

    // ???
    val16 = 0;
    memcpy(ptr, &val16, sizeof(val16));
    ptr += SEQ_NUM;
    cntBytes += SEQ_NUM;

    for( i = 0 ; i < publishMe[ topic_to_pub ].numMPs ; i++ )
    {
        val32 = publishMe[ topic_to_pub ].topicSubscription[ i ].mp;
        memcpy(ptr, &val32, sizeof(val32));
        ptr += MP;
        cntBytes += MP;
        for ( j = 0 ; j < publishMe[ topic_to_pub ].topicSubscription[ i ].numSamples ; j++ )
        {
            // logicals
            if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_PFP_VALUE )
            {
                val32 = pfp_values[j];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_PTLT_TEMPERATURE )
            {
                val32 = pfp_values[j];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_PTRT_TEMPERATURE )
            {
                val32 = pfp_values[j];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_TCMP )
            {
                val32 = pfp_values[j];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_COP_PRESSURE )
            {
                val32 = pfp_values[j];
                memcpy(ptr, &val32, sizeof(val32));
            }

            // timestamps
            else if  (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_SEC_1)
            {
                val32 = cam_secs_chk[j*9+0];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_SEC_2 )
            {
                val32 = cam_secs_chk[j*9+1];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_SEC_3 )
            {
                val32 = cam_secs_chk[j*9+2];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_SEC_4 )
            {
                val32 = cam_secs_chk[j*9+3];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_SEC_5 )
            {
                val32 = cam_secs_chk[j*9+4];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_SEC_6 )
            {
                val32 = cam_secs_chk[j*9+5];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_SEC_7 )
            {
                val32 = cam_secs_chk[j*9+6];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_SEC_8 )
            {
                val32 = cam_secs_chk[j*9+7];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_SEC_9 )
            {
                val32 = cam_secs_chk[j*9+8];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_1)
            {
                val32 = cam_nsecs_chk[j*9+0];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_2 )
            {
                val32 = cam_nsecs_chk[j*9+1];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_3 )
            {
                val32 = cam_nsecs_chk[j*9+2];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_4 )
            {
                val32 = cam_nsecs_chk[j*9+3];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_5 )
            {
                val32 = cam_nsecs_chk[j*9+4];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_6 )
            {
                val32 = cam_nsecs_chk[j*9+5];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_7 )
            {
                val32 = cam_nsecs_chk[j*9+6];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_8 )
            {
                val32 = cam_nsecs_chk[j*9+7];
                memcpy(ptr, &val32, sizeof(val32));
            }
            else if (publishMe[ topic_to_pub ].topicSubscription[ i ].mp == MP_CAM_NSEC_9 )
            {
                val32 = cam_nsecs_chk[j*9+8];
                memcpy(ptr, &val32, sizeof(val32));
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

    if ( cntBytes != sendBytes )
    {
        printf("ERROR, publish, sent bytes don't equal message size\n");
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

    ssize_t  retBytes = 0;
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

            memcpy(&subAppName, ptr, sizeof(subAppName));
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
                MPnum = 23;
            }
            else
            {
                MPnum = num_mps;
            }

            if (true == mallocChk)
            {
                sub_mp = realloc(sub_mp, sizeof(sub_mp) * MPnum);
                if (NULL == sub_mp)
                {
                    mallocChk = false;
                    syslog(LOG_ERR, "%s:%d ERROR! BAD realloc() - sub_mp",__FUNCTION__, __LINE__);
                }
            }

            if (true == mallocChk)
            {
                sub_mpPer = realloc(sub_mpPer, sizeof(sub_mpPer)*MPnum);
                if (NULL == sub_mpPer)
                {
                    mallocChk = false;
                    syslog(LOG_ERR, "%s:%d ERROR! BAD realloc() - sub_mpPer",__FUNCTION__, __LINE__);
                }
            }

            if (true == mallocChk)
            {
                sub_mpNumSamples = realloc(sub_mpNumSamples, sizeof(sub_mpNumSamples)*MPnum);
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

    if ( (cnt_retBytes != (uint32_t)retBytes) || ( CMD_SUBSCRIBE != command ) || ( calcLength != actualLength ) || (calcMPs != num_mps) || (false == mallocChk) )
    {
        success = false;
        if ( (cnt_retBytes != (uint32_t)retBytes) )
        {
            printf("ERROR! SUBSCRIBE: bytes received don't equal message size \n");
            syslog(LOG_ERR, "%s:%d ERROR! insufficient message data retBytes %zd != cnt_retBytes %u", __FUNCTION__, __LINE__, retBytes, cnt_retBytes);
        }
        if ( ( CMD_SUBSCRIBE != command ) )
        {
            printf("ERROR! SUBSCRIBE: invalid command in payload \n");
            syslog(LOG_ERR, "%s:%d ERROR! invalid command %u", __FUNCTION__, __LINE__, command);
        }
        if ( ( calcLength != actualLength ) )
        {
            printf("ERROR! SUBSCRIBE: payload length incorrect \n");
            syslog(LOG_ERR, "%s:%d ERROR! payload not equal to expected number of bytes calcLength %u != actualLength %u ",__FUNCTION__, __LINE__, calcLength, actualLength);
        }

        if ( (calcMPs != num_mps) )
        {
            printf("ERROR! SUBSCRIBE: MPs to receive don't correspond to payload length \n");
            syslog(LOG_ERR, "%s:%d ERROR! length does not coincide with number of MPs: calcMPs %u != num_mps %u", __FUNCTION__, __LINE__, calcMPs, num_mps);
        }
        if ( (false == mallocChk) )
        {
            printf("ERROR! BAD realloc() for MPs\n");
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
 * Used to package data for sending subscribe acknowledgment
 * message
 *
 * @param[in] csocket UDP socket
 * @param[in] addr_in UDP address to send
 * @param[out] success true/false status
 *
 * @return success true/false status
 */
bool process_subscribe_ack( int32_t csocket )
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

    for( i = 0 ; i < publishMe[ currentTopic ].numMPs ; i++ )
    {
        if ( true == publishMe[ currentTopic ].topicSubscription[ i ].valid )
        {
            val16 = GE_SUCCESS;
            memcpy(ptr, &val16, sizeof(val16));
        }
        else
        {
            printf("ERROR! SUBSCRIBE ACKNOWLEDGE: MP subscription error detected \n");
            val16 = GE_INVALID_MP_NUMBER;
            memcpy(ptr, &val16, sizeof(val16));
            genErr = GE_INVALID_MP_NUMBER;
            success = false;
        }
        ptr += ERROR_MP;
        cntBytes += ERROR_MP;
    }

    actualLength = cntBytes - CMD_ID - LENGTH;


    memcpy(msgLenPtr,   &actualLength,                          sizeof(uint16_t));
    memcpy(topicIDptr,  &publishMe[ currentTopic ].topic_id,    sizeof(uint32_t));
    memcpy(msgErrPtr,   &genErr,                                sizeof(int16_t));

    sendBytes       = send(csocket, sendData, cntBytes, 0);

    // check message
    if ( (cntBytes != sendBytes) || ( GE_INVALID_MP_NUMBER == genErr ) )
    {
        success = false;
        if ( cntBytes != sendBytes )
        {
            printf("ERROR! SUBSCRIBE ACKNOWLEDGE: bytes sent don't equal message size \n");
            syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u ", __FUNCTION__, __LINE__, sendBytes, cntBytes);
        }

        if ( GE_INVALID_MP_NUMBER == genErr )
        {
            printf("ERROR! SUBSCRIBE ACKNOWLEDGE: MP subscription error \n");
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
    uint8_t sendData[ MAXBUFSIZE ];
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
        printf("ERROR! sending heartbeat - sent bytes don't equal message size \n");
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

    const uint32_t SIMMsubscriptionMP[ MAX_SIMM_SUBSCRIPTION ] =
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

    // for new topic/subscription
    publishMe = realloc(publishMe, sizeof(topicToPublish)*num_topics_total);
    if (NULL == publishMe)
    {
        printf("MALLOC error after subscription received - number of topics subscribed \n");
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! BAD realloc()",__FUNCTION__, __LINE__);
    }
    else
    {
        publishMe[ currentTopic ].app_name     = subAppName;
        publishMe[ currentTopic ].numMPs       = MPnum;
        publishMe[ currentTopic ].publishReady = false;
        
        // for MPs
        publishMe[ currentTopic ].topicSubscription = malloc(sizeof(MPinfo)*publishMe[ currentTopic ].numMPs);
        if (NULL == publishMe[ currentTopic ].topicSubscription)
        {
            printf("MALLOC error after subscription received - number of MPs \n");
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! BAD malloc()",__FUNCTION__, __LINE__);
        }
    }

    if (true == success)
    {
        publishMe[ currentTopic ].topicSubscription = (MPinfo*) malloc(sizeof(MPinfo)*publishMe[ currentTopic ].numMPs);
        if (NULL == publishMe[ currentTopic ].topicSubscription)
        {
            printf("MALLOC error after subscription received - number of MPs \n");
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! BAD malloc()",__FUNCTION__, __LINE__);
        }
    }

    if (true == success)
    {
#if 0
        syslog(LOG_DEBUG, "%s:%d PREPARING topic %d, num MPS %d",
               __FUNCTION__, __LINE__, currentTopic, publishMe[currentTopic].numMPs);
#endif
        for (k = 0; (k < publishMe[ currentTopic ].numMPs) && (true == success); k++)
        {
#if 0
            syslog(LOG_DEBUG, "%s:%d PREPARING publish[%d][%d] mp %d, period %d, samples %d",
                   __FUNCTION__, __LINE__, currentTopic, k, sub_mp[k], sub_mpPer[k], sub_mpNumSamples[k]);
#endif

            publishMe[ currentTopic ].topicSubscription[ k ].mp             = sub_mp[ k ];
            publishMe[ currentTopic ].topicSubscription[ k ].mp_val_float   = NULL;
            publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long    = NULL;
            publishMe[ currentTopic ].topicSubscription[ k ].period         = sub_mpPer[ k ];
            publishMe[ currentTopic ].topicSubscription[ k ].numSamples     = sub_mpNumSamples[ k ];
            publishMe[ currentTopic ].topicSubscription[ k ].valid          = false;

            // if MP is logical
            if ( (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_PFP_VALUE ) ||
                (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_PTLT_TEMPERATURE ) ||
                (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_PTRT_TEMPERATURE ) ||
                (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_TCMP ) ||
                (publishMe[ currentTopic ].topicSubscription[ k ].mp == MP_COP_PRESSURE ) )
            {
                publishMe[ currentTopic ].topicSubscription[ k ].logical = true;

                publishMe[ currentTopic ].topicSubscription[ k ].mp_val_float = (float*) calloc(sub_mpNumSamples[k], sizeof(float));
                if (NULL == publishMe[ currentTopic ].topicSubscription[ k ].mp_val_float)
                {
                    printf("MALLOC error after subscription received - number of MP samples \n");
                    success = false;
                    syslog(LOG_ERR, "%s:%d ERROR! failed to alloc for topic %u, sub %d, samples %d",
                           __FUNCTION__, __LINE__, currentTopic, k, sub_mpNumSamples[k]);
                }
            }
            else /* timestamp */
            {
                publishMe[ currentTopic ].topicSubscription[ k ].logical = false;

                // 18 = 9 seconds, 9 nanoseconds.  This is the max potential
                // number of timestamps per second.
                publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long = (uint32_t*) calloc(sub_mpNumSamples[k], sizeof(uint32_t));
                if (NULL == publishMe[ currentTopic ].topicSubscription[ k ].mp_val_long)
                {
                    printf("MALLOC error after subscription received - number of MP samples \n");
                    success = false;
                    syslog(LOG_ERR, "%s:%d ERROR! failed to alloc for topic %u, sub %d, samples %d",
                           __FUNCTION__, __LINE__, currentTopic, k, sub_mpNumSamples[k]);
                }
            } /* else timestamp */
        } /* for (k = 0; (k < publishMe[ currentTopic ].numMPs) && (true == success); k++) */
    } /* if (true == success) */

    if (true == success)
    {
        for (k = 0; (k < publishMe[ currentTopic ].numMPs) && (true == success); k++)
        {
            numMPsMatching = 0;
            for( i = 0 ; i < MAX_SIMM_SUBSCRIPTION ; i++ )
            {
                if( SIMMsubscriptionMP[ i ] == publishMe[ currentTopic ].topicSubscription[ k ].mp )
                {
                    numMPsMatching++;
                }
            } /* for( i = 0 ; i < MAX_SIMM_SUBSCRIPTION ; i++ ) */
#if 0
                    syslog(LOG_DEBUG, "%s:%d sub[%d][%d] (%d) match %d found at index %d",
                           __FUNCTION__, __LINE__, currentTopic, k,
                           SIMMsubscriptionMP[i], numMPsMatching, i);
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
        } /* for (k = 0; (k < publishMe[ currentTopic ].numMPs) && (true == success); k++) */

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
                printf("INVALID SUBSCRIPTION: periods are not all the same \n");
                publishMe[ currentTopic ].topicSubscription[ i ].valid = false;
                syslog(LOG_ERR, "%s:%d INVALID SUBSCRIPTION",__FUNCTION__, __LINE__);
                publishMe[ currentTopic ].period    = 0;
                publishMe[ currentTopic ].topic_id  = -1;
            }
        } /* for( i = 0 ; i < publishMe[currentTopic].numMPs ; i++ ) */

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

    avg     = 0;
    sum     = 0;
    var     = 0;
    diff    = 0;
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
