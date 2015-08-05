//  SIMM FUNCTIONS.h
#ifndef __SIMMFUNCTIONS_H__
#define __SIMMFUNCTIONS_H__
#include <time.h>

#define DAEMON_NAME             "SIMM"
#define UNUSED(x) (x)__attribute__((unused))

// MILLSECONDS
#define MINPER_PFP_VALUE                        1000
#define MINPER_PTLT_TEMPERATURE                 1000
#define MINPER_PTRT_TEMPERATURE                 1000
#define MINPER_TCMP                             1000
#define MINPER_CAM_SEC_1                        1000
#define MINPER_CAM_NSEC_1                       1000
#define MINPER_COP_PRESSURE                     1000

//SUBSCRIBE MP INFO
typedef struct
{
    uint32_t mp;
    uint32_t period;
    uint32_t numSamples;
    bool logical;
    bool valid;
} MPinfo;

MPinfo *subscribeMP;
uint32_t num_mps;

void remote_set16(uint8_t *ptr, uint16_t val);
void remote_set32(uint8_t *ptr, uint32_t val);
void remote_set64(uint8_t *ptr, uint64_t val);
uint16_t remote_get16(uint8_t *ptr);
uint32_t remote_get32(uint8_t *ptr); 

// at boot API processing
void process_registerApp( int32_t csocket , struct timespec goTime );
bool process_registerApp_ack(int32_t csocket );
void process_registerData(int32_t csocket);
bool process_registerData_ack(int32_t csocket );
void process_openUDP( int32_t csocket , struct sockaddr_in addr_in );
bool process_sysInit( int32_t csocket );

// run-time API processing
void process_publish( int32_t csocket , struct sockaddr_in addr_in , int32_t LogMPs[] , int32_t time_secMP[] , int32_t time_nsecMP[] );
bool process_subscribe( int32_t csocket );
bool process_subscribe_ack( int32_t csocket , struct sockaddr_in addr_in );
//int32_t process_subscribe_send_DEBUG(int32_t csocket);
bool check_elapsedTime( struct timespec startTime , struct timespec stopTime , int32_t timeToChk );
bool check_validMP( int32_t fromLoop );


// API COMMANDS
enum WhichMsg 
{
    CMD_REGISTER_APP            = 0x0001,   // send
    CMD_REGISTER_APP_ACK        = 0x0002,   // rcv  
    CMD_REGISTER_DATA           = 0x0003,   // send
    CMD_REGISTER_DATA_ACK       = 0x0004,   // rcv
    CMD_SUBSCRIBE               = 0x0005,   // rcv
    CMD_SUBSCRIBE_ACK           = 0x0006,   // send
    CMD_HEARTBEAT               = 0x0007,                                           //
    CMD_OPEN                    = 0x0008,   // UDP send
    CMD_CLOSE                   = 0x0009,   // UDP send
    CMD_PUBLISH                 = 0x000A,   // send
    CMD_SYSINIT                 = 0x000B,   // send
};

// MPs for SIMM
#define MAX_SIMM_SUBSCRIPTION   7
//enum subscribeMPs
//{
//    MP_PFP_VALUE                        = 1003,
//    MP_PTLT_TEMPERATURE                 = 1004,
//    MP_PTRT_TEMPERATURE                 = 1005,
//    MP_TCMP                             = 1006,
//    MP_CAM_SEC_1                        = 1031,
//    MP_CAM_NSEC_1                       = 1032,
//    MP_COP_PRESSURE                     = 1033,
//};


// MPs
enum listMP
{
    MP_SOURCE_TIME_SEC                  = 1001, //    Time_t  This is the time calculated at the source. Seconds part of the clock_gettime API. This MP will be added to all publish messages by the source.
    MP_SOURCE_TIME_NSEC                 = 1002, //    long    This is the time calculated at the source. Nano seconds field of clock_gettime API. 1001 and 1002 should be the fields of the same clock_gettime API call. This MP will be added to all publish messages by the source.
    MP_PFP_VALUE                        = 1003, //    *float   Pre-filter  Fuel pressure sensor
    MP_PTLT_TEMPERATURE                 = 1004, //    *float   Exhaust manifold - pre-turbine temperature left
    MP_PTRT_TEMPERATURE                 = 1005, //    *float   Exhaust manifold - pre-turbine temperature right
    MP_TCMP                             = 1006, //    *Float   TCMP temperature
    MP_COP_HO_REAL                      = 1007, //    *float   COP half order real value
    MP_COP_HO_IMAG                      = 1008, //    *float   COP half order imaginary  value
    MP_COP_FO_REAL                      = 1009, //    *Float   COP full order real value
    MP_COP_FO_IMAG                      = 1010, //    *float   COP full order imaginary  value
    MP_CRANK_HO_REAL                    = 1011, //    *Float   CRANK half order real value
    MP_CRANK_HO_IMAG                    = 1012, //    *Float   CRANK half order imaginary  value
    MP_CRANK_FO_REAL                    = 1013, //    *Float   CRANK full order real value
    MP_CRANK_FO_IMAG                    = 1014, //    *float   CRANK full order imaginary  value
    MP_TURBO_REAL                       = 1015, //    *float   MPTurbo oil let sensor � real part
    MP_TURBO_IMAG                       = 1016, //    *Float   MPTurbo oil let sensor � imaginary  part
    MP_COP_HALFORDER_AMPLITUDE          = 1017, //    float   Following MP�s originate from FDL app
    MP_COP_HALFORDER_ENERGY             = 1018, //    float
    MP_COP_HALFORDER_PHASE              = 1019, //    Float
    MP_COP_FIRSTORDER_AMPLITUDE         = 1020, //    Float
    MP_COP_FIRSTORDER_ENERGY            = 1021, //    Float
    MP_COP_FIRSTORDER_PHASE             = 1022, //    Float
    MP_CRANK_HALFORDER_AMPLITUDE        = 1023, //    Float
    MP_CRANK_HALFORDER_ENERGY           = 1024, //    float
    MP_CRANK_HALFORDER_PHASE            = 1025, //    Float
    MP_CRANK_FIRSTORDER_AMPLITUDE       = 1026, //    Float
    MP_CRANK_FIRSTORDER_ENERGY          = 1027, //    Float
    MP_CRANK_FIRSTORDER_PHASE           = 1028, //    float
    MP_TURBO_OIL_FIRSTORDER_AMPLITUDE   = 1029, //    Float
    MP_TURBO_OIL_FIRSTORDER_ENERGY      = 1030, //    float
    MP_CAM_SEC_1                        = 1031, //    *long    This is the time calculated at the zero crossing for the CAM sensor. Seconds part of the clock_gettime API
    MP_CAM_NSEC_1                       = 1032, //    *long    This is the time calculated at the zero crossing for the CAM sensor. Nano- Seconds part of the clock_gettime API
    MP_COP_PRESSURE                     = 1033, //    *long    COP Pressure
};

// ERROR CODES
enum errorCodes
{
    GE_SUCCESS = 0,
    GE_REGN_TIMEOUT,
    GE_PACKET_ERR,
    GE_INVALID_COMMAND_ID,
    GE_INVALID_HOST_OS,
    GE_INVALID_PROCESS_ID,
    GE_INVALID_PROCESS_NAME,
    GE_INVALID_NO_OF_MPS,
    GE_INVALID_MP_NUMBER,
    GE_STALE_DATA,
    GE_DATA_NOT_AVAIL,
    GE_BARSM_TO_AACM_ERR,
    GE_AACM_TO_BARSM_ERR, 
};

// Host OSs
enum hostOS
{
    PETALINUX   = 0,
    // others ... ?
};



#endif
