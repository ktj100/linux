
/** @file simm.c
 * Main file to starting Sensor Master Module.  Init portion for
 * FPGA and registering the app and it's data.  Opens UDP and
 * waits for one sys init message and one subscribe message
 * before starting 3 threads - SUBSCRIBE, PUBLISH, and SENSORS
 *
 * Subscribe Thread: polls for a subscribe
 *
 * Publish Thread: every second, publish respective data
 *
 * Sensors Thread: interfaces to FPGA to collect data.  Once
 * collected, generates logical MPs and timestamps in order to
 * be published.
 *
 * Copyright (c) 2015, DornerWorks, Ltd.
 */

/****************
* INCLUDES
****************/
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dirent.h>
#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include "simm_functions.h"
#include "sensor.h"
#include "fpga_read.h"


/****************
* PRIVATE CONSTANTS
****************/
char UDPAddress[]   = "225.0.0.37";
int UDPPort_Bind         =  4097;
int UDPPort_Dest         =  4096;

// THREADS
static pthread_t thread_publish;        // read, write TCP
static pthread_t sensor_thread;     // get FPGA data
static pthread_t thread_subscribe;  // waits for a subscribe and returns subscribe_ack
pthread_mutex_t pubMutex                = PTHREAD_MUTEX_INITIALIZER;

/****************
* GLOBALS
****************/
static int32_t clientSocket_TCP = -1;
static int32_t clientSocket_UDP = -1;
struct sockaddr_in DestAddr_TCP;
struct sockaddr_in DestAddr_UDP;
//struct ip_mreq mreq;
struct in_addr localInterface;
struct sockaddr_in DestAddr_SUBSCRIBE;
int32_t Logicals[33];
int32_t TimeStamp_s[9];
int32_t TimeStamp_ns[9];
bool publish = false;
struct timespec goStart;
char simmAppName[5] = { 's', 'i', 'm', 'm', '\0' };
pid_t simmPid;

topicToPublish *publishMe = NULL;

/****************
* PRIVATE FUNCTION PROTOTYPES
****************/
static bool simm_init(void);
static void simm_shutdown(void);
static bool setupPublishStructure(void);
static void simm_run(void); // calls/setup the threads
static void* simm_runtime_publish(void *param);
static void* simm_runtime_subscribe(void *param);
static void* read_sensors(void *param);
bool UDPsetup(void);
bool TCPsetup(void);

/**
 * Calls SIMM init function.  If pass, start threads, else fail.
 *
 * @param[in] argc
 * @param[in] *argv[]
 * @param[out] true/false
 *
 * @return true/false sucess of simm startup.  if fails to init,
 *         returns false
 */
int32_t main(int32_t argc, char *argv[])
{
    /* If a "process name" is provided, copy it, otherwise leave the default 
     * name set */
    if (2 <= argc)
    {
        strncpy(simmAppName, argv[1], 4);
    }

    simmPid = getpid();

    openlog(DAEMON_NAME, LOG_CONS, LOG_LOCAL0);
    syslog(LOG_INFO, "%s started name:%s, pid:%u", DAEMON_NAME, simmAppName, simmPid);
    syslog(LOG_INFO, "version %s", DAEMON_VERSION);
    syslog(LOG_INFO, "date %s", DAEMON_BUILD_DATE);

    if ( false != simm_init() )
    {
        printf("simm_init() SUCCESS!\n");
        syslog(LOG_INFO, "%s:%d simm_init() SUCCESS!", __FUNCTION__, __LINE__);
        simm_run();
    }
    else
    {
        printf("simm_init() FAIL!\n");
        syslog(LOG_ERR, "%s:%d simm_init() ERROR!", __FUNCTION__, __LINE__);
    }

    // shutdown/cleanup
    simm_shutdown();

    return 0;
}

/**
 * Init FPGA to SIMM inteface.  Setup TCP and UDP socket.
 * Register the app and available data to publish.  Sends open
 * UDP and waits for sys_init message.  once received,
 * polls/waits for one subscribe message.  Once received (and
 * everything prior succeeds), return status is true.  Else if
 * anything fails along the way, returns false.
 *
 * @param[in] void
 * @param[out] true/false
 *
 * @return true/false sucess of simm init.
 */
static bool simm_init(void) // 2.1
{
    bool success            = true;
#if 0
    bool success_sub        = true;
    bool success_subAck     = true;
    bool success_bPD        = true;
#endif    
    bool success_WinCo      = true;


    //int32_t sizeofSubscribedMPs = 0;

    // not sure about this block
//  errno = 0;
//  if (0 > sigfillset(&set))
//  {
//      success = false;
//      syslog(LOG_ERR, "%s:%d unable to generate fillset (%d:%s)",__FUNCTION__, __LINE__, errno, strerror(errno));
//  }
//
//      errno = 0;
//      if (0 > sigdelset(&set, SIGCHLD))
//      {
//          success = false;
//          syslog(LOG_ERR, "%s:%d unable to remove SIGCHLD (%u) from signal set (%d:%s)",__FUNCTION__, __LINE__, SIGCHLD, errno, strerror(errno));
//      }
//
//  rc = pthread_sigmask(SIG_BLOCK, &set, NULL);
//  if (0 != rc)
//  {
//      success = false;
//      syslog(LOG_ERR, "%s:%d unable to set sigmask (%d:%s)",__FUNCTION__, __LINE__, rc, strerror(rc));
//  }
    // end block


    clock_gettime( CLOCK_REALTIME , &goStart );


    if (false != success)
    {
        success = fpga_init();
        if (false == success)
        {
            printf("fpga_init() FAIL!\n");
        }
        else
        {
            printf("fpga_init() SUCCESS!\n");
        }
    }

    if (false != success)
    {
        success = UDPsetup();
        if (false == success)
        {
            printf("UDPsetup() FAIL!\n");
        }
        else
        {
            printf("UDPsetup() SUCCESS!\n");
        }
    }

    if (false != success)
    {
        success = TCPsetup();
        if (false == success)
        {
            printf("TCPsetup() FAIL!\n");
        }
        else
        {
            printf("TCPsetup() SUCCESS!\n");
        }
    }

    //sleep(10);
    if (false != success)
    {
        success = process_registerApp( clientSocket_TCP , goStart );
        if (false == success)
        {
            printf("process_registerApp() FAIL!\n");
        }
        else
        {
            printf("process_registerApp() SUCCESS!\n");
        }
    }

    if (false != success)
    {
        success = process_registerApp_ack( clientSocket_TCP );
        if (false == success)
        {
            printf("process_registerApp_ack() FAIL!\n");
        }
        else
        {
            printf("process_registerApp_ack() SUCCESS!\n");
        }
    }

    if (false != success)
    {
        success = process_registerData( clientSocket_TCP );
        if (false == success)
        {
            printf("process_registerData() FAIL!\n");
        }
        else
        {
            printf("process_registerData() SUCCESS!\n");
        }
    }

    if (false != success)
    {
        success = process_registerData_ack( clientSocket_TCP );
        if (false == success)
        {
            printf("process_registerData_ack() FAIL!\n");
        }
        else
        {
            printf("process_registerData_ack() SUCCESS!\n");
        }
    }

    if (false != success)
    {
        //sleep(2);   // currently need for python script ...
        success = process_openUDP( clientSocket_UDP , DestAddr_UDP );
        if (false == success)
        {
            printf("process_openUDP() FAIL!\n");
        }
        else
        {
            printf("process_openUDP() SUCCESS!\n");
        }
    }

    if (false != success)
    {
        success = process_sysInit( clientSocket_UDP );
        if (false == success)
        {
            printf("process_sysInit() FAIL!\n");
        }
        else
        {
            printf("process_sysInit() SUCCESS!\n");
        }
    }
    if (false != success)
    {
        success = setupPublishStructure();
        if (false == success)
        {
            printf("setupPublishStructure() FAIL!\n");
        }
        else
        {
            printf("setupPublishStructure() SUCCESS!\n");
        }
    }

#if 0
    if (false != success)
    {
        success_sub     = process_subscribe( clientSocket_TCP );
        if (false == success_sub)
        {
            printf("initial process_subscribe() FAIL!\n");
            success = false;
        }
        else
        {
            printf("initial process_subscribe() SUCCESS!\n");
        }
    }

    if (false != success)
    {
        success_bPD     = buildPublishData();
        if (false == success_bPD)
        {
            printf("initial buildPublishData() FAIL!\n");
            success = false;
        }
        else
        {
            printf("initial buildPublishData() SUCCESS!\n");
        }
    }
    if (false != success)
    {
        success_subAck  = process_subscribe_ack( clientSocket_TCP );
        if (false == success_subAck)
        {
            printf("initial process_subscribe_ack() FAIL!\n");
            success = false;
        }
        else
        {
            printf("initial process_subscribe_ack() SUCCESS!\n");
        }
    }
#endif

    if (false != success)
    {
        success_WinCo = calcHannWindowCo(64);
        if (false == success_WinCo)
        {
            printf("calcHannWindowCo() FAIL!\n");
            success = false;
        }
        else
        {
            printf("calcHannWindowCo() SUCCESS!\n");
            printf("calcHannWindowCo() hardcoded to generated 64 coefficients until DFT is in place.\n");
        }
    }

    if (false == success)
    {
        syslog(LOG_ERR, "%s:%d ERROR! during boot.  Will not proceed to run_time.", __FUNCTION__, __LINE__);
        printf("ERROR! in boot-up configuration.  Will not proceed to run_time.\n");
        close(clientSocket_TCP);
    }

    return success;
}

/**
 * Clean up SIMM resources.
 *
 * @param[in] void
 *
 * @return void
 */
static void simm_shutdown(void)
{
    subscribe_cleanup();

    /* TODO: Cleanup publish area */
}

/**
 * Starts three threads per main description.  Only executes if
 * SIMM init passes.
 *
 * @param[in] void
 * @param[out] void
 *
 * @return void
 */
static void simm_run(void)
{
        bool success = true;
        int32_t rc_sensor, rc_publish, rc_subscribe;
        sigset_t set;
        siginfo_t sig;

        printf("SIMM run_time(): threading started ... \n");
        syslog(LOG_ERR, "%s:%d STATUS, SIMM run_time(): threading started",__FUNCTION__, __LINE__);

        // CREATE SENSOR THREAD
        // looks to be ~272 possibly lost bytes per valgrind with each thread.
        errno = 0;
        rc_sensor = pthread_create(&sensor_thread, NULL, read_sensors, NULL);
        if(0 != rc_sensor)
        {
            printf("ERROR creating thread for reading sensors \n");
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! failed to create sensor thread (%d:%s)",__FUNCTION__, __LINE__, rc_sensor, strerror(rc_sensor));
        }

        // CREATE PUBLISH THREAD (UDP)
        // looks to be ~272 possibly lost bytes per valgrind with each thread.
        errno = 0;
        rc_publish = pthread_create(&thread_publish, NULL, simm_runtime_publish, NULL);
        if (0 != rc_publish)
        {
            printf("ERROR creating thread for publishing data \n");
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! failed to create TCP thread (%d:%s)",__FUNCTION__, __LINE__, rc_publish, strerror(rc_publish));
        }

        // CREATE SUBSCRIBE THREAD (UDP)
         // looks to be ~272 possibly lost bytes per valgrind with each thread.
        errno = 0;
        rc_subscribe = pthread_create(&thread_subscribe, NULL, simm_runtime_subscribe, NULL);
        if (0 != rc_subscribe)
        {
            printf("ERROR creating thread for subscriptions \n");
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! failed to create TCP thread (%d:%s)",__FUNCTION__, __LINE__, rc_subscribe, strerror(rc_subscribe));
        }

        errno = 0;
        if (0 > sigfillset(&set))
        {
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! unable to generate fillset (%d:%s)",__FUNCTION__, __LINE__, errno, strerror(errno));
        }

        // signal handling .. are these the ones I need?
        while (true == success)
        {
            errno = 0;
            if (0 >= sigwaitinfo(&set, &sig))
            {
                success = false;
                syslog(LOG_ERR, "%s:%d ERROR! waiting for signals (%d:%s)",__FUNCTION__, __LINE__, errno, strerror(errno));
                //printf("\nsigwaitinfo error\n");
            }
            else
            { /* ! (0 >= sigwaitinfo(&set, &sig)) */
                /* some signals are expected, so just print debugging information
                 * and continue */
                switch (sig.si_signo)
                {
                case SIGCHLD:
                    syslog(LOG_DEBUG, "DEBUG! ignoring signal %d (code: %d, pid: %d, uid: %d, status: %d)",sig.si_signo, sig.si_code, sig.si_pid, sig.si_uid, sig.si_status);
                    break;
                case SIGALRM:
                case SIGPIPE:
                    syslog(LOG_DEBUG, "DEBUG! ignoring signal %d (code: %d, value: %d)",sig.si_signo, sig.si_code, sig.si_value.sival_int);
                    break;
                default:
                    /* print as much debugging as possible for the unhandled sig. */
                    syslog(LOG_WARNING, "WARNING! received signal %d (code: %d, pid: %d, uid: %d, addr: %p)",sig.si_signo, sig.si_code, sig.si_pid, sig.si_uid, sig.si_addr);
                    /* exit the application */
                    success = false;
                    break;
                } /* switch (sig.si_signo) */
            } /* else ! (0 >= sigwaitinfo(&set, &sig)) */
        }
        /* set of signals to watch for */
        /* watch for signals */
  }


/**
 * SUBSCRIBE thread.  Polls/waits for a subscribe.  Once
 * received, locks (mutex) data and alloactes space for a new
 * topic/subscription.  Once done, sends subscribe acknowledge
 * message.
 *
 * @param[in] void
 * @param[out] void
 *
 * @return void
 */
static void* simm_runtime_subscribe(void * UNUSED(param) )
{
    int32_t rc;
    //bool success,
    //bool success_sub_run, success_subAck_run, success_bPD_run, success_sub_malloc_run = true;
    int32_t numSub;

    rc = pthread_detach( pthread_self() );
    if (rc != 0)
    {
      //success = false;
      syslog(LOG_ERR, "%s:%d ERROR! Failed to detach thread (%d:%s)",__FUNCTION__, __LINE__, rc, strerror(rc));
    }

    numSub = 0;
    //printf("SUBSCRIBE THREAD STARTED!\n");
    while ( 1 )
    {
        if ( true == process_subscribe( clientSocket_TCP ) )
        {
            numSub++;
            pthread_mutex_lock(&pubMutex);


            num_topics_total++;

            publishMe = realloc(publishMe, sizeof(topicToPublish)*num_topics_total);
            if (NULL == publishMe)
            {
                //success_sub_malloc_run = false;
                syslog(LOG_ERR, "%s:%d REALLOC ERROR!", __FUNCTION__, __LINE__);
                printf("BAD REALLOC: when reallocating memory based on subscription (thread) \n");
            }

            buildPublishData();
            process_subscribe_ack( clientSocket_TCP );
            currentTopic++;
            printf("Incrementing currentTopic to %u\n", currentTopic);

            pthread_mutex_unlock(&pubMutex);
        }
    }
    return 0;
}


/**
 * PUBLISH thread.  Every second, publishes all available
 * topics/subscriptions ready to publish.  After publish is
 * made, determines next availble list of topics/subscriptions
 * to publish.
 *
 * @param[in] void
 * @param[out] void
 *
 * @return void
 */
// This is the thread for processing PUBLISH every second
static void* simm_runtime_publish(void * UNUSED(param) )
{
    int32_t rc;
    int32_t hrtBt;
    uint32_t numberToPublish, cntPublishes, i;
    bool success = true;
    bool timeHasElapsed = true;
    struct timespec sec_begin, sec_end;

    hrtBt = 0;

    rc = pthread_detach( pthread_self() );
    if (rc != 0)
    {
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! Failed to detach thread (%d:%s)",__FUNCTION__, __LINE__, rc, strerror(rc));
    }

    //printf("PUBLISH THREAD STARTED!\n");
    numberToPublish = 0;
    nextPublishPeriod = 1000;
    clock_gettime(CLOCK_REALTIME, &sec_begin);

    while ( true == success )
    {
        if (false == timeHasElapsed)
        {
            clock_gettime(CLOCK_REALTIME, &sec_end);                        // time to check
            timeHasElapsed = numSecondsHaveElapsed(sec_begin, sec_end, 1);     // check elapsed time of 1 second ... consider changing this based on nextPublishPeriod
        }
        else
        {
            hrtBt++;
            pthread_mutex_lock(&pubMutex);

            process_HeartBeat( clientSocket_TCP, hrtBt );

            cntPublishes = 0;
            for( i = 0 ; i < num_topics_total ; i++ )
            {
                if (true == publishMe[i].publishReady)
                {
                    //process_publish( clientSocket_UDP , DestAddr_UDP , Logicals , TimeStamp_s , TimeStamp_ns, i );
                    process_publish( clientSocket_UDP , DestAddr_UDP , i );
                    cntPublishes++;
                }
                if ( (numberToPublish == cntPublishes) && (numberToPublish == num_topics_total) )
                {
                    nextPublishPeriod = 0;
                }
            }
            pthread_mutex_unlock(&pubMutex);
            //printf("\n\n\nFROM PUBLISH THREAD, nextPublishPeriod: %d\n", nextPublishPeriod);
            timeHasElapsed = false;
            clock_gettime(CLOCK_REALTIME, &sec_begin);
            nextPublishPeriod += 1000;
            numberToPublish = publishManager();
        }
    }
    return 0;
}

/**
 * SENSORS thread.  Gets FGPA data and generates logical MP and
 * timestamp data.  See sensor.c
 *
 * @param[in] void
 * @param[out] void
 *
 * @return void
 */
static void* read_sensors(void * UNUSED(param) )
{
    bool success = true;
    int32_t rc;

//  voltages = {0,0,0,0,0};
//  timestamps = {0,0,0,0,0,0,0,0,0};
//  ts_HiLoCnt = {0,0,0};

    //printf("Thread Started!\n");

    // detach the thread so that main can resume
    errno = 0;
    rc = pthread_detach(pthread_self());
    if (rc != 0)
    {
        syslog(LOG_ERR, "%s:%d ERROR: Thread detaching unsuccessful (%d:%s)",__FUNCTION__, __LINE__, rc, strerror(rc));
        //printf("ERROR: Thread detaching unsuccessful: (%d)\n", errno);
    }

    // LOOP FOREVER, BULDING X SECONDS WORTH OF DATA AND EXPORTING
    while(success)
    {
        // CONTAINS A BLOCKING READ FOR THE FPGA INTERRUPT REGISTERS
        if ( false == wait_for_fpga() )
        {
            success = false;
        }
        // printf("\nFPGA Ready!\n");

        /* GET FPGA DATA IMMEDIATELY */
        //get_fpga_data(&voltages[0], &timestamps[0], &ts_HiLoCnt[0]);
        get_fpga_data();

        bufferFPGAdata();

        // save copy to use
        pthread_mutex_lock(&pubMutex);

        // GET LOGICAL VALUES FROM REGISTERS
        //make_logicals(&voltages[0]);
        make_logicals();
        // ADD ERROR CHECKING FOR STATUS

        // GET TIMESTAMPS FOM REGISTERS
        //calculate_timestamps(&timestamps[0], &ts_HiLoCnt[0]);
        calculate_timestamps();
        // ADD ERROR CHECKING FOR STATUS

        pthread_mutex_unlock(&pubMutex);
    }
    return 0;
}


/**
 * Establish UDP socket.
 *
 * @param[in] void
 * @param[out] true/false
 *
 * @return true/false status of socket setup.
 */
bool UDPsetup(void)
{

    bool success = true;
    struct ip_mreq mreq;

    if (true == success)
    {
        errno = 0;
        clientSocket_UDP = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (0 > clientSocket_UDP)
        {
            printf("ERROR WITH UDP socket() config \n");
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! Failed to create UDP socket %d (%d:%s) - configure socket failed", __FUNCTION__, __LINE__, clientSocket_UDP, errno, strerror(errno));
        }
    }

    if (true == success)
    {
        int reuseAddr = 1;
        errno = 0;
        if (setsockopt(clientSocket_UDP, SOL_SOCKET, SO_REUSEADDR, (char*) &reuseAddr, sizeof(reuseAddr)) < 0)
        {
            printf("Error setsockopt() SO_REUSEADDR\n");
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! error with setsockopt SO_REUSEADDR %d (%d:%s)", __FUNCTION__, __LINE__, clientSocket_UDP, errno, strerror(errno));
        }
    }

    if (true == success)
    {
        struct sockaddr_in bind_addr;

        memset(&bind_addr, 0, sizeof(bind_addr));
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_port = htons(UDPPort_Bind);
        bind_addr.sin_addr.s_addr = INADDR_ANY;
        
        errno = 0;
        if ( bind(clientSocket_UDP, (struct sockaddr *) &bind_addr, sizeof(bind_addr)) < 0)
        {
            printf("Error bind() clientSocket_UDP\n");
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! bind() clientSocket_UDP %d (%d:%s)", __FUNCTION__, __LINE__, clientSocket_UDP, errno, strerror(errno));            
        }
    }

    if (true == success)
    {
        errno = 0;
        mreq.imr_multiaddr.s_addr = inet_addr(UDPAddress);
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        if (setsockopt(clientSocket_UDP, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0)
        {
            printf("ERROR setsockopt() IP_ADD_MEMBERSHIP \n");
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! error with setsockopt multicast group IP_ADD_MEMBERSHIP %d (%d:%s)", __FUNCTION__, __LINE__, clientSocket_UDP, errno, strerror(errno));
        }
    }

    memset(&DestAddr_UDP,0,sizeof(DestAddr_UDP));               // clear struct
    DestAddr_UDP.sin_family = AF_INET;                          // must be this
    DestAddr_UDP.sin_port = htons(UDPPort_Dest);                     // set the port to write to
    DestAddr_UDP.sin_addr.s_addr = inet_addr(UDPAddress);

    return success;
}

/**
 * Establish TCP socket.
 *
 * @param[in] void
 * @param[out] true/false
 *
 * @return true/false status of socket setup.
 */
bool TCPsetup(void)
{
    bool success            = true;
    char ServerIPAddress[]  = "127.0.0.1";
    int TCPServerPort       =  8000;
    int32_t reuse           = 1;
    int32_t chkSetSockOpt   = 0;

    if (true == success)
    {
        errno = 0;
        clientSocket_TCP = socket(AF_INET, SOCK_STREAM, 0);
        if (0 > clientSocket_TCP)
        {
            printf("ERROR WITH TCP socket() config \n");
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! Failed to create socket %d (%d:%s) ", __FUNCTION__, __LINE__, clientSocket_TCP, errno, strerror(errno));
        }

        errno = 0;
        chkSetSockOpt = setsockopt(clientSocket_TCP, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        if ( 0 > chkSetSockOpt )
        {
            printf("ERROR setsockopt() SO_REUSEADDR \n");
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! setsockopt for reusing address failed %d (%d:%s) ", __FUNCTION__, __LINE__, clientSocket_TCP, errno, strerror(errno));
        }
    }

    errno = 0;
    if (true == success)
    {
        memset(&DestAddr_TCP,0,sizeof(DestAddr_TCP));               // clear struct
        DestAddr_TCP.sin_family = AF_INET;                          // must be this
        DestAddr_TCP.sin_port = htons(TCPServerPort);               // set the port to write to
        DestAddr_TCP.sin_addr.s_addr = inet_addr(ServerIPAddress);  // set destination IP address
        memset(&(DestAddr_TCP.sin_zero), '\0', 8);                  // zero the rest of the struct

        if (0 != connect(clientSocket_TCP,(struct sockaddr *)&DestAddr_TCP, sizeof(DestAddr_TCP)))
        {
            printf("ERROR TCP connect() \n");
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! TCP failed to connect %u (%d:%s) ", __FUNCTION__, __LINE__, DestAddr_TCP.sin_port, errno, strerror(errno));
        }
    }
    return success;
}

/**
 * Initial allocation of publish structs and associated
 * elements.
 *
 * @param[in] void
 * @param[out] true/false
 *
 * @return true/false status of mallocs.
 */
bool setupPublishStructure(void)
{
    currentTopic        = 0;
    num_topics_total    = 0;
    prevPeriodChk       = 0;

    return true;
}
