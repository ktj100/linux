#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <stdio.h>

#include "barsm_functions.h"

struct sockaddr_storage toRcvUDP;



/**
 * Establish TCP socket.  
 *
 * @param[in] csocket: socket for the TCP communications
 * @param[in] destAddr: settings holder for the TCP communications
 *
 * @return true/false status of socket setup.  
 */
bool TCPsetup( int32_t csocket, struct sockaddr_in destAddr ) 
{
    bool success            = true;
    char ServerIPAddress[]  = "127.0.0.1";
    int TCPServerPort       =  8000;

    errno = 0;
    csocket = socket(AF_INET, SOCK_STREAM, 0);
    if (0 > csocket)
    {
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! Failed to create socket %d (%d:%s) ", \
            __FUNCTION__, __LINE__, csocket, errno, strerror(errno));
    }

    errno = 0;
    if ( success )
    {
        memset(&destAddr,0,sizeof(destAddr));               // clear struct
        destAddr.sin_family = AF_INET;                          // must be this
        destAddr.sin_port = htons(TCPServerPort);               // set the port to write to
        destAddr.sin_addr.s_addr = inet_addr(ServerIPAddress);  // set destination IP address
        memset(&(destAddr.sin_zero), '\0', 8);                  // zero the rest of the struct

        if (0 != connect(csocket,(struct sockaddr *)&destAddr, sizeof(destAddr)))
        {
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! TCP failed to connect %u (%d:%s) ", \
                __FUNCTION__, __LINE__, destAddr.sin_port, errno, strerror(errno));
        }
    }

    return success;
}


/**
 * Establish UDP socket.  
 *
 * @param[in] csocket: socket for the UDP communications
 * @param[in] destAddr: settings holder for the UDP communications
 * @param[in] udpPort: port for the UDP communications
 * @param[in] udpAddr: IP address for the UDP communications
 *
 * @return true/false status of socket setup.  
 */
bool UDPsetup( int32_t csocket, struct sockaddr_in destAddr, int32_t udpPort , char *udpAddr ) 
{
    bool success        = true;
    char loopch;

    // multicast - still 100% on how this struct, setsockopt,  and IP_ADD_MEMBERSHIP tie together.  
    //struct ip_mreq mreq;
    if (true == success)
    {
        errno = 0;
        csocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (0 > csocket)
        {
            //printf("ERROR WITH UDP SOCKET\n");
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! Failed to create UDP socket %d (%d:%s) - configure socket failed", \
                __FUNCTION__, __LINE__, csocket, errno, strerror(errno));
        }
    }

    errno = 0;
    if (true == success)
    {
        memset(&destAddr,0,sizeof(destAddr));               // clear struct
        destAddr.sin_family = AF_INET;                          // must be this
        destAddr.sin_port = htons(udpPort);                     // set the port to write to
        destAddr.sin_addr.s_addr = inet_addr(udpAddr);       // set destination IP address
        memset(&(destAddr.sin_zero), '\0', 8);                  // zero the rest of the struct
    }

    // disable loopback, i.e. don't receive sent packets.  
    loopch  = 0;
    errno   = 0;
    if (setsockopt(csocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) 
    {
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! Failed to create UDP socket %d (%d:%s) - disbale loopback failed", \
            __FUNCTION__, __LINE__, csocket, errno, strerror(errno));
    }

    return success;
}



/**
 * Used to initialize communications between BARSM and AACM immediately after 
 * startup of the AACM.
 *
 * @param[in] csocket: TCP socket
 *
 * @return true/false whether a terminal error has occured
 */
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
    uint16_t actualLength = 0; 
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



/**
 * Used to receive the ackknoledgement message from the AACM.
 *
 * @param[in] csocket TCP socket
 *
 * @return true/false whether a terminal error has occured
 */
bool receive_barsmToAacmInitAck(int32_t csocket)
{
    bool success = true;

    enum barsmToAacmInitAck_params
    {
        CMD_ID          = 2,
        LENGTH          = 2,
        MSG_SIZE        =   CMD_ID +
                            LENGTH,
    };

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
                syslog(LOG_ERR, "%s:%d ERROR: Message received was not expected ACK!", \
                    __FUNCTION__, __LINE__);
                success = false;
            }
        }
    }

    return (success);
}



/**
 * Used to assign a name to each process in the system.
 *
 * @param[in] pName: The 4 character string to store the name in
 *
 * @return void
 */
void assign_procName( char * pName )
{
    int32_t i;
    for ( i = 0; i < 4; i++ )
    {
        pName[i] = rand_letter();
    }
}



/**
 * Returns a random character from the alphabet.
 *
 * @param[in] void
 *
 * @return char: randomly selected letter of the alphabet
 */
char rand_letter(void)
{
    uint32_t randNum;
    char returnVal = NULL;

    randNum = rand() % 26;

    switch (randNum)
    {
        case 0:
        returnVal = 'a';
        break;
        case 1:
        returnVal = 'b';
        break;
        case 2:
        returnVal = 'c';
        break;
        case 3:
        returnVal = 'd';
        break;
        case 4:
        returnVal = 'e';
        break;
        case 5:
        returnVal = 'f';
        break;
        case 6:
        returnVal = 'g';
        break;
        case 7:
        returnVal = 'h';
        break;
        case 8:
        returnVal = 'i';
        break;
        case 9:
        returnVal = 'j';
        break;
        case 10:
        returnVal = 'k';
        break;
        case 11:
        returnVal = 'l';
        break;
        case 12:
        returnVal = 'm';
        break;
        case 13:
        returnVal = 'n';
        break;
        case 14:
        returnVal = 'o';
        break;
        case 15:
        returnVal = 'p';
        break;
        case 16:
        returnVal = 'q';
        break;
        case 17:
        returnVal = 'r';
        break;
        case 18:
        returnVal = 's';
        break;
        case 19:
        returnVal = 't';
        break;
        case 20:
        returnVal = 'u';
        break;
        case 21:
        returnVal = 'v';
        break;
        case 22:
        returnVal = 'w';
        break;
        case 23:
        returnVal = 'x';
        break;
        case 24:
        returnVal = 'y';
        break;
        case 25:
        returnVal = 'z';
        break;
    }

    return ( returnVal );
}



/**
 * Starts a new process in the linked list position that was send in the input
 * parameters. The new PID is copied in place and 'alive' status updated.
 *
 * @param[in] tmp_node: pointer position in the linked list wehre the new 
 *      process information needs to be stored
 *
 * @return true/false whether a terminal error has occured
 */
bool start_process(child_pid_list *tmp_node)
{
    bool success = true;
    /* fork a new process to start the module back up */
    pid_t new_pid;  
    new_pid = fork();
    if (0 == new_pid)
    {
        /* execute the file again in the new child process */
        errno = 0;
        if ( (0 != execl(tmp_node->dir, tmp_node->item_name, (char *)NULL)) )
        {
            syslog(LOG_ERR, "ERROR: 'execl()' failed for %s! (%d:%s)", \
                tmp_node->dir, errno, strerror(errno));
            printf("ERROR: 'execl()' failed for %s! (%d:%s) \n", \
                tmp_node->dir, errno, strerror(errno));
            /* force the spawned process to exit */
            exit(-errno);
        }
    }
    else if (-1 == new_pid)
    {
        syslog(LOG_ERR, "ERROR: Failed to fork child process for %s! (%d:%s)", \
            tmp_node->dir, errno, strerror(errno));

        success = false;
    } 
    else
    {
        tmp_node->child_pid = new_pid;
        /* alive == 0 indicates that the process has been restarted and should be good */
        tmp_node->alive = 0;
    }
    return (success);
}



/**
 * Receives the system initialization message from AACM
 *
 * @param[in] csocket UDP socket
 *
 * @return true/false whether the message was received
 */
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

    uint16_t command = 0;
    uint32_t retBytes = 0; 
    uint32_t actualLength = 0; 
    uint32_t calcLength;
    uint8_t retData[ MSG_SIZE ];
    uint8_t *ptr;
    socklen_t toRcvUDP_size;
    struct pollfd myPoll[1];
    int32_t retPoll;

    myPoll[0].fd = csocket;
    myPoll[0].events = POLLIN; 

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
                retBytes = recvfrom(csocket , retData , MSG_SIZE , 0 , \
                    (struct sockaddr *)&toRcvUDP , &toRcvUDP_size);

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
        if ( (retPoll <= 0) || (MSG_SIZE != retBytes) || ( CMD_SYSINIT != command ) \
            || ( calcLength != actualLength ) )
        {
            gotMsg = false;
            if ( (MSG_SIZE != retBytes) )
            {
                syslog(LOG_ERR, "%s:%d ERROR! insufficient message data %u != %u", \
                    __FUNCTION__, __LINE__, retBytes, MSG_SIZE);
            }
            if ( ( CMD_SYSINIT != command ) )
            {
                syslog(LOG_ERR, "%s:%d ERROR! invalid command %u", \
                    __FUNCTION__, __LINE__, command);
            }
            if ( ( calcLength != actualLength ) )
            {
                syslog(LOG_ERR, "%s:%d ERROR! payload not equal to expected number of bytes %u != %u ", \
                    __FUNCTION__, __LINE__, calcLength, actualLength);
            }
            if ( (retPoll <= 0) )
            {
                syslog(LOG_ERR, "%s:%d ERROR! error getting message %u ", \
                    __FUNCTION__, __LINE__, retPoll);
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
 * Sends the list of all processes in the system with the process IDs, names, 
 * and type values immediately after the SYS_INIT message is received.
 *
 * @param[in] csocket: TCP socket
 * @param[in] tmp_node: the pointer to the first item in the linked list
 * @param[in] dirs[]: list of directories for making the process type parameters
 * @param[in] barsm_name[4]: the random 4 char string assigned to barsm (the 
 *      name not stored in the linked list)
 *
 * @return true/false whether a terminal error has occured
 */
bool send_barsmToAacmProcesses( int32_t csocket, child_pid_list *tmp_node, \
    const char *dirs[], char barsm_name[4] )
{
    bool success = true;

    enum registerData_params
    {
        CMD_ID                  = 2,
        LENGTH                  = 2,
        NUM_PROCESSES           = 2, 
        PID                     = 4, 
        PROC_NAME               = 4, 
        PROC_TYPE               = 4,
    };

    uint8_t sendData[ MAXBUFSIZE ];
    uint8_t *ptr;
    uint16_t val16;
    uint8_t *msgLenPtr;
    uint8_t *numProcsPtr;
    uint32_t tmpPid;
    uint8_t tmpChar;
    int32_t i;
    uint32_t type;
    /* Start at 1 for BARSM */
    int32_t num_procs = 1;
    uint32_t actualLength = 0;
    uint32_t msg_size = 0;
    uint32_t sentBytes = 0;

    ptr = sendData;
    val16 = CMD_BARSM_TO_AACM_PROCESSES;
    memcpy(ptr, &val16, sizeof(val16));
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    numProcsPtr = ptr;
    ptr += NUM_PROCESSES;

    /* BARSM's info is not stored in the linked list so it needs to be handled 
     * outside of the loop */
    tmpPid = getpid();
    memcpy(ptr, &tmpPid, sizeof(tmpPid));
    ptr += PID;

    for ( i = 0; i < 4; i++ )
    {
        tmpChar = barsm_name[i];
        memcpy(ptr, &tmpChar, sizeof(tmpChar));
        ptr += 1;
    }

    type = 0x0000000C;
    memcpy(ptr, &type, sizeof(type));
    ptr += PROC_TYPE;

    while ( NULL != tmp_node->next )
    {
        if ( -1 != tmp_node->alive )
        {
            num_procs += 1;

            tmpPid = tmp_node->child_pid;
            memcpy(ptr, &tmpPid, sizeof(tmpPid));
            ptr += PID;

            for ( i = 0; i < 4; i++ )
            {
                tmpChar = tmp_node->proc_name[i];
                memcpy(ptr, &tmpChar, sizeof(tmpChar));
                ptr += 1;
            }

            if ( NULL != strstr(tmp_node->dir, dirs[0]) )
            {
                type = 0x00000002;
            }
            else if ( NULL != strstr(tmp_node->dir, dirs[1]) )
            {
                type = 0x00000001;
            }
            else if ( NULL != strstr(tmp_node->dir, dirs[2]) )
            {
                type = 0x00000009;
            }
            else if ( NULL != strstr(tmp_node->dir, dirs[3]) )
            {
                type = 0x00000000;
            }
            else if ( NULL != strstr(tmp_node->dir, dirs[4]) )
            {
                type = 0x00000008;
            }
            else
            {
                syslog(LOG_ERR, "%s:%d ERROR: When reading 'dir' for app/mod! ", \
                    __FUNCTION__, __LINE__);
                success = false;
            }
            memcpy(ptr, &type, sizeof(type));
            ptr += PROC_TYPE;
        }
    }

    memcpy(numProcsPtr, &num_procs, sizeof(num_procs));
    actualLength = NUM_PROCESSES + num_procs * (PID + PROC_TYPE + PROC_NAME);
    memcpy(msgLenPtr, &actualLength, sizeof(actualLength));
    msg_size = actualLength + CMD_ID + LENGTH;

    sentBytes = send( csocket, sendData, msg_size, 0 );
    if ( sentBytes != msg_size )
    {
        syslog(LOG_ERR, "%s:%d ERROR: Insufficient message data, %u != %u", \
            __FUNCTION__, __LINE__, sentBytes, msg_size);
        success = false;
    }

    return (success);
}



/**
 * Goes through the entire list of modules once, checking if each successfully 
 * started module is still running, and restarts those that have stopped.
 *
 * @param[in] tmp_node: a pointer to the first item in the linked list
 *
 * @return true/false whether a terminal error has occurred
 */ 
bool check_modules(child_pid_list *tmp_node)
{
    bool success = true;
    int32_t rc; 
    int32_t waitreturn;

    /* check status of the child processes that are supposed to be running */
    printf("\n\n in check_modules \n\n");

    while( NULL != tmp_node->next )
    {
        errno = 0;
        waitreturn = waitpid(tmp_node->child_pid, &rc, WNOHANG);

        if ( -1 == tmp_node->alive )
        {
            /* the process was not able to start up, so ignore */
        }
        else if (-1 == waitreturn)
        {
            syslog(LOG_ERR, "ERROR: Process for %s with PID %d no longer exists! (%d:%s)", \
                tmp_node->dir, tmp_node->child_pid, errno, strerror(errno));
            printf("\nERROR: Process for %s with PID %d no longer exists! (%d:%s) \n", \
                tmp_node->dir, tmp_node->child_pid, errno, strerror(errno));
            /* alive == 1 indicates that this process needs to be replaced */
            tmp_node->alive = 1;   
        }
        else if (0 < waitreturn)
        {
            syslog(LOG_ERR, "ERROR: Process for %s with PID %d has changed state! (%d:%s)", \
                tmp_node->dir, tmp_node->child_pid, errno, strerror(errno));
            printf("\nERROR: Process for %s with PID %d has changed state! (%d:%s) \n", \
                tmp_node->dir, tmp_node->child_pid, errno, strerror(errno));
            /* alive == 1 indicates that this process needs to be replaced */
            tmp_node->alive = 1;
        }
        if (1 == tmp_node->alive)
        {
            syslog(LOG_NOTICE, "NOTICE: Restarting %s...", tmp_node->item_name);
            printf("NOTICE: Restarting %s...", tmp_node->item_name);
            if ( ! start_process(tmp_node) )
            {
                success = false;
            }
        }

        tmp_node = tmp_node->next;
    }
    return(success);
}



/**
 * Waits for error messages from the AACM, and restarts or terminates modules 
 * and applications as directed in the messages.
 *
 * @param[in] csocket: TCP socket
 * @param[in] tmp_node: a pointer to the first item in the linked list
 *
 * @return void
 */
void receive_aacmToBarsm( int32_t csocket, child_pid_list *tmp_node )
{
    enum aacmToBarsm_params
    {
        CMD_ID                  = 2,
        LENGTH                  = 2,
        PID                     = 4,
        NUM_PROCESSES           = 2,
    };

    uint16_t command = 0;
    uint16_t actualLength = 0;
    uint16_t numErrors = 0;
    uint8_t retData[ MAXBUFSIZE ];
    uint8_t *ptr;
    pid_t srcPid = 0;
    pid_t tmpPid = 0;
    struct pollfd myPoll[1];
    int32_t retPoll = 0;
    int32_t retBytes = 0;

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
            errno = 0;
            retBytes = recv( csocket , retData , MAXBUFSIZE , 0 );
            if ( -1 == retBytes )
            {
                syslog(LOG_ERR, "%s:%d ERROR: In recv()! (%d: %s) ", \
                    __FUNCTION__, __LINE__, errno, strerror(errno));
            }

            ptr = retData;
            memcpy(&command, ptr, sizeof(command));
            ptr += CMD_ID;

            memcpy(&actualLength, ptr, sizeof(actualLength));
            ptr += LENGTH;

            memcpy(&srcPid, ptr, sizeof(srcPid));
            ptr += PID;
            start_select(srcPid, tmp_node);

            memcpy(&numErrors, ptr, sizeof(numErrors));
            ptr += NUM_PROCESSES;

            while ( numErrors > 0 )
            {
                memcpy(&tmpPid, ptr, sizeof(tmpPid));
                ptr += PID;
                start_select(tmpPid, tmp_node);
                numErrors--;
            }
        }
    }
}



/**
 * Sends a response to the AACM_TO_BARSM message conatining a signal showing
 * whether the app/modules were restarted or terminated.
 *
 * @param[in] csocket: TCP socket
 *
 * @return void
 */
// MODEL AFTER "process_registerApp" in simm_functions.c
void send_aacmToBarsmAck( int32_t csocket )
{
    enum registerApp_params
    {
        CMD_ID                  = 2,
        LENGTH                  = 2,
        ERROR                   = 2,
        MSG_SIZE                =   CMD_ID + 
                                    LENGTH + 
                                    ERROR,
    };

    uint8_t sendData[ MSG_SIZE ];
    uint8_t *ptr;
    uint16_t val16;
    uint8_t *msgLenPtr = 0;
    uint32_t actualLength = 0;
    int32_t sentBytes = 0;

    ptr = sendData;
    val16 = CMD_AACM_TO_BARSM_ACK;
    memcpy(ptr, &val16, sizeof(val16));
    ptr += CMD_ID;

    msgLenPtr = ptr;
    ptr += LENGTH;

    val16 = GE_AACM_TO_BARSM_ERR;           /* THIS WILL NEED TO CHANGE ONCE SPECIFIED BY GE */
    memcpy(ptr, &val16, sizeof(val16));
    ptr += ERROR;

    *ptr = MSG_SIZE;
    ptr += MSG_SIZE;

    actualLength = MSG_SIZE - CMD_ID - LENGTH;
    memcpy(msgLenPtr, &actualLength, sizeof(actualLength));

    errno = 0;
    sentBytes = send(csocket, sendData, MSG_SIZE, 0);
    if ( -1 == sentBytes )
    {
        syslog(LOG_ERR, "%s:%d ERROR: Sending of AACM_TO_BARSM_ACK failed! (%d: %s)", \
            __FUNCTION__, __LINE__, errno, strerror(errno));
    }
    else if ( 6 != sentBytes )
    {
        syslog(LOG_ERR, "%s:%d ERROR: During ending of AACM_TO_BARSM_ACK!", \
            __FUNCTION__, __LINE__);
    }
}



/**
 * Selects the item in the linked list that matches the given PID, and calls
 * the start_process() with that item of the linked list.
 *
 * @param[in] pid: the PID of the process that needs to be replaced/restarted
 * @param[in] tmp_node: a pointer to the first item in the linked list
 *
 * @return true/false whether a terminal error occured
 */
bool start_select(pid_t pid, child_pid_list *tmp_node)
{
    bool success = true;

    while ( NULL != tmp_node->next )
    {
        if ( pid == tmp_node->child_pid )
        {
            if ( ! start_process(tmp_node) )
            {
                success = false;
            }
        }
    }

    return (success);
}