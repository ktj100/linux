#define BARSM_TO_AACM_INIT_ACK_MSG 0x00100000

enum WhichMsg 
{
    CMD_BARSM_TO_AACM               = 0x000C,
    CMD_BARSM_TO_AACM_ACK           = 0x000D,
    CMD_AACM_TO_BARSM               = 0x000E,
    CMD_AACM_TO_BARSM_ACK           = 0x000F,
    CMD_BARSM_TO_AACM_INIT          = 0x0001,
    CMD_BARSM_TO_AACM_INIT_ACK      = 0x0010,
    CMD_BARSM_TO_AACM_PROCESSES     = 0x0011,
};

enum errorCodes
{
    GE_SUCCESS                          = 0,
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

bool send_barsmToAacmInit(int32_t csocket);
bool recieve_barsmToAacmInitAck(int32_t csocket);