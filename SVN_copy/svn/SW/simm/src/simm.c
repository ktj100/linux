//=========================================================================================================================================================
// GET
// ASSIMILATOR 
// SIMM - Sensor Interface Master Module
// David Norwood
// Travis Kuiper
// 
// -	Sensor Interface Master Module (SIMM)
//      o	SIMM shall interface with SI node, Convert RAW data to Logical value and get Logical  data to send to the AACM.
//      o	SIMM shall implement the GE API for Master Modules � that will enable standard communication between SIMM and AACM.
//      o	If there is problem communicating with the SI node then an error message is sent to the AACM.
//      o	SIMM shall interface with AACM as per ICD defined in document - API ICD for DornerWorks.docx.
// 
// 0.0.0
// July 14, 2015
//  Developed initial SMM functionality.  
//  
// 
// 0.0.1
// August 10, 2015
//  Added MPs for individual timestamps
// 
// 
// valgrind: --tool=memcheck --read-var-info=yes --leak-check=full --track-origins=yes --show-reachable=yes --show-possibly-lost=yes --malloc-fill=B5 --free-fill=4A
// ========================================================================================================================================================
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
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
#include "fpga_sim.h"

// THREADS
static pthread_t thread_TCP;        // read, write TCP
static pthread_t sensor_thread;     // get FPGA data
static pthread_t thread_subscribe;  // waits for a subscribe and returns subscribe_ack

pthread_mutex_t pubMutex                = PTHREAD_MUTEX_INITIALIZER;
//pthread_mutex_t waitPubData             = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pubCond                  = PTHREAD_COND_INITIALIZER;


//static pthread_t thread_SIN;      // process data, logical, etc.

//  char UDPAddress[]   = "225.0.0.37";
//  int UDPPort         =  4096;
    char UDPAddress[] = "127.0.0.1";
    int UDPPort =  4096;


// FUNCTION DECLARATIONS
static bool simm_init(void);
static void simm_run(void); // calls/setup the threads
static void* simm_runtime_publish(void *param);
static void* simm_runtime_subscribe(void *param);
static void* read_sensors(void *param);

bool UDPsetup(void);
bool TCPsetup(void);

// GLOBALS
static int32_t clientSocket_TCP = -1;
static int32_t clientSocket_UDP = -1;
struct sockaddr_in DestAddr_TCP;
struct sockaddr_in DestAddr_UDP;
struct sockaddr_in DestAddr_SUBSCRIBE;


//int32_t Logicals[5];
int32_t Logicals[33];
int32_t TimeStamp_s[9];
int32_t TimeStamp_ns[9];
bool publish = false;

struct timespec goStart;

int32_t main(int32_t UNUSED(argc), char UNUSED(*argv[]))
{
    openlog(DAEMON_NAME, LOG_CONS, LOG_LOCAL0);
    syslog(LOG_INFO, DAEMON_NAME " started");

    if ( true == simm_init() ) 
    {
        syslog(LOG_INFO, "%s:%d simm_init() SUCCESS!", __FUNCTION__, __LINE__);
        printf("\n SIMM INIT! \n");
        simm_run();
    } 
    else 
    {
        syslog(LOG_ERR, "%s:%d simm_init() ERROR!", __FUNCTION__, __LINE__);
        // anything else?
    }
    return 0;
}

static bool simm_init(void) // 2.1
{
    bool success = true;
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

    // TCP/UDP SETUP
    if (false == UDPsetup() )
    {
        syslog(LOG_ERR, "%s:%d UDPsetup() ERROR!", __FUNCTION__, __LINE__);
        printf("UDPSETUP ERROR!\n");
    }

    if (false == TCPsetup() )
    {
        syslog(LOG_ERR, "%s:%d TCPsetup() ERROR!", __FUNCTION__, __LINE__);
        printf("UDPSETUP ERROR!\n");
    }
    clock_gettime( CLOCK_REALTIME , &goStart );

    // TCP STUFF
    // REGISTER APP (TCP)
    process_registerApp(clientSocket_TCP, goStart );
    if (true != process_registerApp_ack(clientSocket_TCP) )
    {
        // REGISTER APP ACK (TCP)
        syslog(LOG_ERR, "%s:%d REGISTER_APP_ACK ERROR!", __FUNCTION__, __LINE__);
        close(clientSocket_TCP);
        exit(0);
    }
    else
    {
        // REGISTER_DATA (TCP)
        process_registerData(clientSocket_TCP);
        // REGISTER_DATA ACK (TCP)
        if ( true != process_registerData_ack(clientSocket_TCP) )
        {
            syslog(LOG_ERR, "%s:%d REGISTER_DATA_ACK ERROR!", __FUNCTION__, __LINE__);
            //close(clientSocket_TCP);
            exit(0);
        }
        else
        {
            // The SMM shall send the OPEN message to the AACM UDP multicast address
            // of 225.0.0.37:4096 after receiving a successful REGISTER_DATA_ACK message.
            //close(clientSocket_TCP);
            printf("SENDING UDP OPEN ... \n");
            sleep(5);   // currently need for python script ... 
            process_openUDP( clientSocket_UDP , DestAddr_UDP );
        }
    }

    printf("POLLING FOR SYS_INIT ... \n");
    while( false == process_sysInit( clientSocket_UDP ) )
    {
        // wait until we get a sys_init response ...
        // time before we exit?
    }
    

    currentTopic = 0;
    num_topics_total = 1;
    prevPeriodChk = 0;

    publishMe = malloc(sizeof(topicToPublish));
    if (NULL == publishMe)
    {
        syslog(LOG_ERR, "%s:%d MALLOC ERROR!", __FUNCTION__, __LINE__);
        printf("BAD MALLOC: publishMe in simm_init\n");
    }

    publishMe[ currentTopic ].topicSubscription = malloc(sizeof(MPinfo));
    if (NULL == publishMe[ currentTopic ].topicSubscription)
    {
        syslog(LOG_ERR, "%s:%d MALLOC ERROR!", __FUNCTION__, __LINE__);
        printf("BAD MALLOC: publishMe[ currentTopic ].topicSubscription in simm_init\n");
    }
    // other malloc ...

    printf("POLLING FOR SUBSCRIBE ... \n");

    // get a subscribe before doing anything else ...
    //if (false != process_subscribe( clientSocket_UDP ) )
    if (false != process_subscribe( clientSocket_TCP ) )
    {
        //pthread_mutex_lock(&pubMutex);
        buildPublishData();
        //pthread_mutex_unlock(&pubMutex);
        sleep(1);
        printf("SENDING SUBSCRIBE_ACK ... \n\n");
        if ( true != process_subscribe_ack( clientSocket_TCP , DestAddr_UDP ) )
        //if ( true != process_subscribe_ack( clientSocket_UDP , DestAddr_UDP ) )
        {
            syslog(LOG_ERR, "%s:%d process_subscribe_ack ERROR!", __FUNCTION__, __LINE__);
        }
        printf("SENT SUBSCRIBE ACK\n");
    }

    nextPublishPeriod = 0;      // init value, will get set to most recet subscribe publish period when publishManager called.  

    return success;
}

static void simm_run(void)
{
        bool success = true;
        int32_t rc_sense, rc_tcp, rc_subscribe;
        sigset_t set;
        siginfo_t sig;

        // CREATE SENSOR THREAD
        // looks to be ~272 possibly lost bytes per valgrind with each thread.
        errno = 0;
        rc_sense = pthread_create(&sensor_thread, NULL, read_sensors, NULL);
        if(0 != rc_sense)
        {
            syslog(LOG_ERR, "%s:%d ERROR! failed to create sensor thread (%d:%s)",__FUNCTION__, __LINE__, errno, strerror(errno));
            printf("ERROR: Thread creation failed! (%d) \n", errno);
        }

        // CREATE PUBLISH THREAD (UDP)
        // looks to be ~272 possibly lost bytes per valgrind with each thread.
        errno = 0;
        rc_tcp = pthread_create(&thread_TCP, NULL, simm_runtime_publish, NULL);
        if (0 != rc_tcp)
        {
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! failed to create TCP thread (%d:%s)",__FUNCTION__, __LINE__, rc_tcp, strerror(rc_tcp));
            printf("ERROR: Thread creation failed! (%d) \n", errno);
        }

        // CREATE SUBSCRIBE THREAD (UDP)
         // looks to be ~272 possibly lost bytes per valgrind with each thread.
        errno = 0;
        rc_subscribe = pthread_create(&thread_subscribe, NULL, simm_runtime_subscribe, NULL);
        if (0 != rc_subscribe)
        {
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! failed to create TCP thread (%d:%s)",__FUNCTION__, __LINE__, rc_subscribe, strerror(rc_subscribe));
            printf("ERROR: Thread creation failed! (%d) \n", errno);
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
                printf("\nsigwaitinfo error\n");
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
        exit(0);
  }


static void* simm_runtime_subscribe(void * UNUSED(param) )
{
    int32_t rc;
    bool success = true;

    rc = pthread_detach( pthread_self() );
    if (rc != 0)
    {
      success = false;
      syslog(LOG_ERR, "%s:%d ERROR! Failed to detach thread (%d:%s)",__FUNCTION__, __LINE__, rc, strerror(rc));
    }

    printf("SUBSCRIBE THREAD STARTED!\n");
    while ( true == success )
    {
        printf("POLLING FOR SUBSCRIBE ... \n");
        if (false != process_subscribe( clientSocket_UDP ) )
        {
            pthread_mutex_lock(&pubMutex);
            // make room for subscription
            num_topics_total++;
            publishMe = realloc(publishMe, sizeof(topicToPublish)*num_topics_total);
            if (NULL == publishMe)
            {
                syslog(LOG_ERR, "%s:%d REALLOC ERROR!", __FUNCTION__, __LINE__);
                printf("BAD REALLOC: publishMe in buildPublishData\n");
            }

            currentTopic++;
            buildPublishData();
            pthread_mutex_unlock(&pubMutex);

            printf("SENDING SUBSCRIBE_ACK ... \n");
            if ( true != process_subscribe_ack( clientSocket_UDP , DestAddr_UDP ) )
            {
                syslog(LOG_ERR, "%s:%d process_subscribe_ack ERROR!", __FUNCTION__, __LINE__);
            }
            
            printf("SENT SUBSCRIBE ACK\n");
        }
    }
    close(clientSocket_UDP);
    pthread_exit(0);
    exit(0);
}


// This is the thread for processing PUBLISH every second
static void* simm_runtime_publish(void * UNUSED(param) )
{
    int32_t rc, numberToPublish, cntPublishes, i;
    bool success = true;
    bool timeElapsed = true;
    struct timespec sec_begin, sec_end;

    struct timespec timeToWait, currentTime;
    int rt;

    rc = pthread_detach( pthread_self() );
    if (rc != 0)
    {
      success = false;
      syslog(LOG_ERR, "%s:%d ERROR! Failed to detach thread (%d:%s)",__FUNCTION__, __LINE__, rc, strerror(rc));
    }

    nextPublishPeriod = 1000;

    printf("PUBLISH THREAD STARTED!\n");
    numberToPublish = 1;
    nextPublishPeriod = 1000;
    // 1st version
    // using the clock_gettime() function will change in order to support a publish based on SUBSCRIBE
    clock_gettime(CLOCK_REALTIME, &sec_begin);  // get first start time ...
    while ( ( true == success ) )
    {
        if (false == timeElapsed)
        {
            clock_gettime(CLOCK_REALTIME, &sec_end);    // time to check
            timeElapsed = check_elapsedTime(sec_begin, sec_end, 1);   // check elapsed time of 1 second
        }
        else
        {
            pthread_mutex_lock(&pubMutex);
            cntPublishes = 0;
            for (i = 0 ; i < num_topics_total ; i++ )
            {
                 if (true == publishMe[i].publishReady)
                 {
                     process_publish( clientSocket_UDP , DestAddr_UDP , Logicals , TimeStamp_s , TimeStamp_ns );
                     cntPublishes++; 
                 }
            }
            if (numberToPublish != cntPublishes)
            {
                syslog(LOG_ERR, "%s:%d ERROR! number of topics to publish (%d) != topics published (%d)", __FUNCTION__, __LINE__, numberToPublish, cntPublishes );
                printf("NUMBER OF TOPICS TO PUBLISH != TOPICS PUBLISHED\n");
            }
            timeElapsed = false;
            clock_gettime(CLOCK_REALTIME, &sec_begin);
            nextPublishPeriod = nextPublishPeriod + 1000;
            numberToPublish = publishManager();
            pthread_mutex_unlock(&pubMutex);
        }
    }

//  // 2nd version
//  clock_gettime(CLOCK_REALTIME, &currentTime);
//  timeToWait.tv_sec = currentTime.tv_sec + ( publishPeriod / 1000 );
//
//  while ( ( true == success ) )
//  {
//      if ( true == goPublish )
//      {
//          pthread_mutex_lock(&pubMutex);
//          rt = pthread_cond_timedwait(&pubCond, &pubMutex, &timeToWait);
//          pthread_mutex_unlock(&pubMutex);
//          process_publish( clientSocket_UDP , DestAddr_UDP , Logicals , TimeStamp_s , TimeStamp_ns );
//
//      }
//  }
    close(clientSocket_UDP);
    pthread_exit(0);
    exit(0);
}


static void* read_sensors(void * UNUSED(param) )
{
    int32_t rc, fpga_ready;
    int32_t get_lv_status, get_ts_status;

    // detach the thread so that main can resume
    errno = 0;
    rc = pthread_detach(pthread_self());
    if (rc != 0)
    {
        syslog(LOG_ERR, "%s:%d ERROR! Failed to detach thread (%d:%s)",__FUNCTION__, __LINE__, rc, strerror(rc));
        printf("ERROR: Thread detaching unsuccessful: (%d)\n", errno);
    }

    printf("SENSOR THREAD STARTED!\n");
    // LOOP FOREVER, BULDING X SECONDS WORTH OF DATA AND EXPORTING
    while(1)
    {
        // WAIT FOR SIGNAL FROM FPGA
        // ADD ERROR FOR >1 SEC TIMEOUT
        for(fpga_ready = 0; 1 != fpga_ready;)
        {
            // "fpga_ready" WILL BE TRIGGERED BY AN INTERRUPT LATER
            fpga_ready = wait_for_fpga();
            // ADD ERROR SCENARIOS
            if(0 == fpga_ready)
            {

            }
        }

        // GET LOGICAL VALUES FROM REGISTERS
        get_lv_status = get_logicals(Logicals, 5);
        // ADD ERROR CHECKING FOR STATUS
        if(1 == get_lv_status)
        {

        }
        else if (0 == get_lv_status)
        {

        }

        // GET TIMESTAMPS FOM REGISTERS
        get_ts_status = get_timestamps(TimeStamp_s, TimeStamp_ns, 9);
        // ADD ERROR CHECKING FOR STATUS
        if(get_ts_status)
        {
            
        }
    }
    pthread_exit(0);
    exit(0);
}


bool UDPsetup(void) 
{
    // desitnation (AACM UDP multicast address)
    bool success        = true;

    // multicast - still 100% on how this struct, setsockopt,  and IP_ADD_MEMBERSHIP tie together.  
    struct ip_mreq mreq;

    if (true == success)
    {
        errno = 0;
        clientSocket_UDP = socket(AF_INET, SOCK_DGRAM, 0);
        if (0 > clientSocket_UDP)
        {
            printf("ERROR WITH UDP SOCKET\n");
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! Failed to create UDP socket %d (%d:%s) ", __FUNCTION__, __LINE__, clientSocket_UDP, errno, strerror(errno));
        }
    }

    errno = 0;
    if (true == success)
    {
        //memset((char *) &DestAddr_UDP, 0, sizeof(DestAddr_UDP));
        memset(&DestAddr_UDP,0,sizeof(DestAddr_UDP));				// clear struct
        //bzero( &DestAddr_UDP , sizeof(DestAddr_UDP ) );
        DestAddr_UDP.sin_family = AF_INET;					        // must be this
        DestAddr_UDP.sin_port = htons(UDPPort);	                    // set the port to write to
        DestAddr_UDP.sin_addr.s_addr = inet_addr(UDPAddress);       // set destination IP address
        memset(&(DestAddr_UDP.sin_zero), '\0', 8);                  // zero the rest of the struct
        //sizeof(DestAddr_UDP.sin_zero)
    }

    // bind?
//  mreq.imr_multiaddr.s_addr = inet_addr(UDPAddress);
//  mreq.imr_interface.s_addr = htonl(UDPPort);
//  if (setsockopt(csocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
//  {
//      printf("setsockopt ERROR\n");
//  }


    return success;
}

bool TCPsetup(void) 
{
    bool success            = true;
    char ServerIPAddress[]  = "127.0.0.1";
    int TCPServerPort       =  8000;

    if (true == success)
    {
        errno = 0;
        clientSocket_TCP = socket(AF_INET, SOCK_STREAM, 0);
        if (0 > clientSocket_TCP)
        {
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! Failed to create socket %d (%d:%s) ", __FUNCTION__, __LINE__, clientSocket_TCP, errno, strerror(errno));
        }
    }

    // CONNECT TO TCP SERVER
    errno = 0;
    if (true == success)
    {
        memset(&DestAddr_TCP,0,sizeof(DestAddr_TCP));				// clear struct
        DestAddr_TCP.sin_family = AF_INET;					        // must be this
        DestAddr_TCP.sin_port = htons(TCPServerPort);	            // set the port to write to
        DestAddr_TCP.sin_addr.s_addr = inet_addr(ServerIPAddress);  // set destination IP address
        memset(&(DestAddr_TCP.sin_zero), '\0', 8);                  // zero the rest of the struct

        // when SSM is ready to register with AACM ... ???
        if (0 != connect(clientSocket_TCP,(struct sockaddr *)&DestAddr_TCP, sizeof(DestAddr_TCP)))
        {
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! TCP failed to connect %u (%d:%s) ", __FUNCTION__, __LINE__, DestAddr_TCP.sin_port, errno, strerror(errno));
            exit(0);
        }
    }
    return success;
}



