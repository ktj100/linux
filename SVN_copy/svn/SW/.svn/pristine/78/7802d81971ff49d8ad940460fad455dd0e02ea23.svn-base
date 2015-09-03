
/** @file fdl.c
 * Subscribes for data in order to calculate power, amplitude,
 * and phase.  Publishes calcuations as MPs.  Publishes 
 * heartbeat as well.   
 * 
 * Subscribe Thread: polls for a subscribe
 * 
 * Publish Thread: Publishes power, amplitude, and phase
 * 
 * Copyright (c) 2010, DornerWorks, Ltd.
 */

/****************
* INCLUDES
****************/
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
#include <math.h>
#include "fdl.h"



/****************
* PRIVATE CONSTANTS
****************/
//  char UDPAddress[]   = "225.0.0.37";
//  int UDPPort         =  4096;
char UDPAddress[] = "127.0.0.1";
int UDPPort =  4096;

// THREADS
static pthread_t thread_getPublish;         // read, write TCP
//static pthread_t thread_sendSubscribe;      // read, write TCP                                        
static pthread_t thread_sendPublish;        // read, write TCP
static pthread_t thread_getSubscribe;       // waits for a subscribe and returns subscribe_ack
pthread_mutex_t pubMutex                = PTHREAD_MUTEX_INITIALIZER;

/****************
* GLOBALS
****************/
static int32_t clientSocket_TCP = -1;
static int32_t clientSocket_UDP = -1;
struct sockaddr_in DestAddr_TCP;
struct sockaddr_in DestAddr_UDP;
struct in_addr localInterface;
struct sockaddr_in DestAddr_SUBSCRIBE;
int32_t Logicals[33];
int32_t TimeStamp_s[9];
int32_t TimeStamp_ns[9];
bool publish = false;
struct timespec goStart;

topicToPublish *publishMe;
FDLinfo *recvFDL;


/****************
* PRIVATE FUNCTION PROTOTYPES
****************/
static bool fdl_init(void);
static bool setupPublishStructure(void);
static void fdl_run(void); // calls/setup the threads
static void* fdl_runtime_sendPublish(void *param);
static void* fdl_runtime_getPublish(void *param);
static void* fdl_runtime_getSubscribe(void *param);
bool UDPsetup(void);
bool TCPsetup(void);

/**
 * Calls fdl init function.  If pass, start threads, else fail.
 *
 * @param[in] UNUSED(argc) 
 * @param[in] UNUSED(*argv[]) 
 * @param[out] true/false 
 *
 * @return true/false sucess of fdl startup.  if fails to init,
 *         returns false
 */
int32_t main(int32_t UNUSED(argc), char UNUSED(*argv[]))
{
    bool success_fdl = true;

    openlog(DAEMON_NAME, LOG_CONS, LOG_LOCAL0);
    syslog(LOG_INFO, DAEMON_NAME " started");

    if ( false != fdl_init() ) 
    {
        syslog(LOG_INFO, "%s:%d fdl_init() SUCCESS!", __FUNCTION__, __LINE__);
        fdl_run();
    } 
    else 
    {
        success_fdl = false;
        syslog(LOG_ERR, "%s:%d fdl_init() ERROR!", __FUNCTION__, __LINE__);
    }

    // shutdown/cleanup

    return success_fdl;
}

/**
 * Init FPGA to fdl inteface.  Setup TCP and UDP socket.
 * Register the app and available data to publish.  Sends open
 * UDP and waits for sys_init message.  once received,
 * polls/waits for one subscribe message.  Once received (and
 * everything prior succeeds), return status is true.  Else if
 * anything fails along the way, returns false.  
 *
 * @param[in] void
 * @param[out] true/false 
 *
 * @return true/false sucess of fdl init.
 */
static bool fdl_init(void) // 2.1
{
    bool success            = true; 

    float copAmplitudeHO;
    float copPhaseHO;
    float copPowerHO;
    float copAmplitudeFO;
    float copPhaseFO;
    float copPowerFO;

    float crankAmplitudeHO;
    float crankPhaseHO;
    float crankPowerHO;
    float crankAmplitudeFO;
    float crankPhaseFO;
    float crankPowerFO;

    //bool success_sub        = true; 
    //bool success_subAck     = true; 
    //bool success_bPD        = true;


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
        success = UDPsetup();
    }

    if (false != success)
    {
        success = TCPsetup();
    }

    if (false != success)
    {
        success = process_registerApp( clientSocket_TCP , goStart );
    }

    if (false != success)
    {
        success = process_registerApp_ack( clientSocket_TCP );
    }

    if (false != success)
    {
        success = process_registerData( clientSocket_TCP );
    }

    if (false != success)
    {
        success = process_registerData_ack( clientSocket_TCP );
    }

    if (false != success)
    {
        sleep(2);   // currently need for python script ... 
        success = process_openUDP( clientSocket_UDP , DestAddr_UDP );
    }
    
    if (false != success)
    {
        success = process_sysInit( clientSocket_UDP );
    }

    if (false != success)
    {
        success = setupPublishStructure();
    }

    // app related
    if (false != success)
    {
        // subscribes for 10 MPS
        success = process_sendSubscribe( clientSocket_TCP );
    }

    if (false != success)
    {
        // gets the acknowledge
        success = process_getSubscribe_ack( clientSocket_TCP );
    }


    // consider waiting for 1st subscribe from SIMM (or wherever) before receiving first publish.  


    if (false != success)
    {
        // gets the first publish subscribed for
        success                 = process_getPublish( clientSocket_UDP );

        copAmplitudeHO          = getAmplitude(recvFDL[0].cop[0], recvFDL[0].cop[1]);
        copPhaseHO              = getPhase(recvFDL[0].cop[0], recvFDL[0].cop[1]);
        copPowerHO              = pow(copAmplitudeHO, 2) / 2;
        copAmplitudeFO          = getAmplitude(recvFDL[0].cop[2], recvFDL[0].cop[3]);
        copPhaseFO              = getPhase(recvFDL[0].cop[2], recvFDL[0].cop[3]);
        copPowerFO              = pow(copAmplitudeFO, 2) / 2;

        crankAmplitudeHO        = getAmplitude(recvFDL[0].crank[0], recvFDL[0].crank[1]);
        crankPhaseHO            = getPhase(recvFDL[0].crank[0], recvFDL[0].crank[1]);
        crankPowerHO            = pow(crankAmplitudeHO, 2) / 2;
        crankAmplitudeFO        = getAmplitude(recvFDL[0].crank[2], recvFDL[0].crank[3]);
        crankPhaseFO            = getPhase(recvFDL[0].crank[2], recvFDL[0].crank[3]);
        crankPowerFO            = pow(crankAmplitudeFO, 2) / 2;

        printf("CALC AMPLITUDE, COP HO: %f\n",     copAmplitudeHO);
        printf("CALC PHASE, COP HO: %f\n",         copPhaseHO);
        printf("CALC POWER, COP HO: %f\n",         copPowerHO);
        printf("CALC AMPLITUDE, COP FO: %f\n",     copAmplitudeFO);
        printf("CALC PHASE, COP FO: %f\n",         copPhaseFO);
        printf("CALC POWER, COP FO: %f\n",         copPowerFO);

        printf("CALC AMPLITUDE, CRANK HO: %f\n",   crankAmplitudeHO);
        printf("CALC PHASE, CRANK HO: %f\n",       crankPhaseHO);
        printf("CALC POWER, CRANK HO: %f\n",       crankPowerHO);
        printf("CALC AMPLITUDE, CRANK FO: %f\n",   crankAmplitudeFO);
        printf("CALC PHASE, CRANK FO: %f\n",       crankPhaseFO);
        printf("CALC POWER, CRANK FO: %f\n",       crankPowerFO);

//      copPhaseHO          = getCopPhaseHO(recvFDL[0].cop);
//      copPowerHO          = getCopPowerHO(recvFDL[0].cop);
//
//      copAmplitudeFO      = getCopAmplitudeFO(recvFDL[0].cop);
//      copPhaseFO          = getCopPhaseFO(recvFDL[0].cop);
//      copPowerFO          = getCopPowerFO(recvFDL[0].cop);
//
//      crankAmplitudeHO    = getCrankAmplitudeHO(recvFDL[0].crank);
//      crankPhaseHO        = getCrankPhaseHO(recvFDL[0].crank);
//      crankPowerHO        = getCrankPowerHO(recvFDL[0].crank);
//
//      crankAmplitudeFO    = getCrankAmplitudeFO(recvFDL[0].crank);
//      crankPhaseFO        = getCrankPhaseFO(recvFDL[0].crank);
//      crankPowerFO        = getCrankPowerFO(recvFDL[0].crank);
//
//      turboAmplitudeFO    = getTurboAmplitudeFO(recvFDL[0].turbo);
//      turboPowerFO        = getTurboPowerFO(recvFDL[0].turbo);
       
    }
    // GETS through to here without errors



    // FDL needs to subscribe, not poll for a subscribe
//  if (false != success)
//  {
//      success_sub     = process_getSubscribe( clientSocket_TCP );
//      success_bPD     = buildPublishData();
//      //success_subAck  = process_sendSubscribe_ack( clientSocket_TCP , DestAddr_TCP );
//      success_subAck  = process_sendSubscribe_ack( clientSocket_TCP );
//
//      if ( false == ( success_sub || success_bPD || success_subAck ) )
//      {
//          success = false;
//      }
//  }

    return success;
}

/**
 * Starts three threads per main description.  Only executes if
 * fdl init passes.
 *
 * @param[in] void
 * @param[out] void
 *
 * @return void
 */
static void fdl_run(void)
{
        bool success = true;
        int32_t rc_sendPublish;
        int32_t rc_getSubscribe; 
        int32_t rc_getPublish;
        sigset_t set;
        siginfo_t sig;

        errno = 0;
        rc_getPublish = pthread_create(&thread_getPublish, NULL, fdl_runtime_getPublish, NULL);
        if (0 != rc_getPublish)
        {
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! failed to create TCP thread (%d:%s)",__FUNCTION__, __LINE__, rc_getPublish, strerror(rc_getPublish));
        }

        // CREATE SUBSCRIBE THREAD (UDP)
        // looks to be ~272 possibly lost bytes per valgrind with each thread.
        errno = 0;
        rc_getSubscribe = pthread_create(&thread_getSubscribe, NULL, fdl_runtime_getSubscribe, NULL);
        if (0 != rc_getSubscribe)
        {
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! failed to create TCP thread (%d:%s)",__FUNCTION__, __LINE__, rc_getSubscribe, strerror(rc_getSubscribe));
        }

        // CREATE PUBLISH THREAD (UDP)
        // looks to be ~272 possibly lost bytes per valgrind with each thread.
        errno = 0;
        rc_sendPublish = pthread_create(&thread_sendPublish, NULL, fdl_runtime_sendPublish, NULL);
        if (0 != rc_sendPublish)
        {
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! failed to create TCP thread (%d:%s)",__FUNCTION__, __LINE__, rc_sendPublish, strerror(rc_sendPublish));
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
static void* fdl_runtime_getSubscribe(void * UNUSED(param) )
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
        //success_sub_run = process_subscribe( clientSocket_TCP );
        
        process_getSubscribe( clientSocket_TCP );
        numSub++;
        pthread_mutex_lock(&pubMutex);
        
        num_topics_total++;

        publishMe = realloc(publishMe, sizeof(topicToPublish)*num_topics_total);
        if (NULL == publishMe)
        {
            //success_sub_malloc_run = false;
            syslog(LOG_ERR, "%s:%d REALLOC ERROR!", __FUNCTION__, __LINE__);
            //printf("BAD REALLOC: publishMe in buildPublishData\n");
        }

        currentTopic++;
        //success_bPD_run = buildPublishData();
        buildPublishData();
        

        //success_subAck_run = process_sendSubscribe_ack( clientSocket_TCP , DestAddr_TCP );
        //process_sendSubscribe_ack( clientSocket_TCP , DestAddr_TCP );
        process_sendSubscribe_ack( clientSocket_TCP );
        pthread_mutex_unlock(&pubMutex);
//      if ( false == ( success_sub_run || success_subAck_run || success_bPD_run || success_sub_malloc_run ) )
//      {
//          success = false;
//      }
    }
    return 0;
}

/**
 * PUBLISH thread for receiving data.  Data should originate
 * only if this app made a subscription. At a minimum during
 * boot, this app should subscribe for real and imaginary values
 * for the half-order and first-order cam, crank and turbo
 * sensors.
 *
 * @param[in] void
 * @param[out] void
 *
 * @return void
 */
static void* fdl_runtime_getPublish(void * UNUSED(param) )
{
    bool success = true;
    int32_t rc;

    rc = pthread_detach( pthread_self() );
    if (rc != 0)
    {
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! Failed to detach thread (%d:%s)",__FUNCTION__, __LINE__, rc, strerror(rc));
    }

    while ( true == success )
    {
        success = process_getPublish( clientSocket_UDP );
    }
    return 0;
}

/**
 * PUBLISH thread for sending data.  Every second, checks which
 * available topics/subscriptions are ready to
 * publish.  After publish is made, determines next availble
 * list of topics/subscriptions to publish.
 *
 * @param[in] void
 * @param[out] void
 *
 * @return void
 */
static void* fdl_runtime_sendPublish(void * UNUSED(param) )
{
//  //bool success = true;
//  int32_t rc;
//
//  rc = pthread_detach( pthread_self() );
//  if (rc != 0)
//  {
//      //success = false;
//      syslog(LOG_ERR, "%s:%d ERROR! Failed to detach thread (%d:%s)",__FUNCTION__, __LINE__, rc, strerror(rc));
//  }
//
//  while ( 1 )
//  {
//      //sleep(1);
//      process_sendPublish( clientSocket_UDP , DestAddr_UDP , 1 );
//      //success = true;
//  }
//
//  return 0;





    int32_t rc, cntPublishes; //, i;
    //int32_t hrtBt;
    static int32_t numberToPublish;
    bool success = true;
    bool timeHasElapsed = true;
    struct timespec sec_begin, sec_end;

    //hrtBt = 0;

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
            //hrtBt++;
            pthread_mutex_lock(&pubMutex);

            //process_HeartBeat( clientSocket_TCP, hrtBt );

            cntPublishes = 0;
            printf("FROM PUBLISH, num_topics_total: %d\n", num_topics_total);
            //for( i = 0 ; i < num_topics_total ; i++ )
            //{
                if (true == publishMe[1].publishReady)
                {
                    //process_publish( clientSocket_UDP , DestAddr_UDP , Logicals , TimeStamp_s , TimeStamp_ns, i );
                    process_sendPublish( clientSocket_UDP , DestAddr_UDP , 1 );
                    cntPublishes++;
                }
                if ( (numberToPublish == cntPublishes) && (numberToPublish == num_topics_total) )
                {
                    nextPublishPeriod = 0;
                }
           // }
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
 * Establish UDP socket.  
 *
 * @param[in] void
 * @param[out] true/false
 *
 * @return true/false status of socket setup.  
 */
bool UDPsetup(void) 
{
    bool success        = true;
    char loopch;

    // multicast - still 100% on how this struct, setsockopt,  and IP_ADD_MEMBERSHIP tie together.  
    //struct ip_mreq mreq;
    if (true == success)
    {
        errno = 0;
        clientSocket_UDP = socket(AF_INET, SOCK_DGRAM, 0);
        if (0 > clientSocket_UDP)
        {
            //printf("ERROR WITH UDP SOCKET\n");
            success = false;
            syslog(LOG_ERR, "%s:%d ERROR! Failed to create UDP socket %d (%d:%s) - configure socket failed", __FUNCTION__, __LINE__, clientSocket_UDP, errno, strerror(errno));
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

    // disable loopback, i.e. don't receive sent packets.  
    loopch  = 0;
    errno   = 0;
    if (setsockopt(clientSocket_UDP, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) 
    {
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! Failed to create UDP socket %d (%d:%s) - disbale loopback failed", __FUNCTION__, __LINE__, clientSocket_UDP, errno, strerror(errno));
    }

    // set local interface for multicast
    // this seems redundant
//  errno = 0;
//  if(setsockopt(clientSocket_UDP, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface)) < 0)
//  {
//          success = false;
//          syslog(LOG_ERR, "%s:%d ERROR! Failed to create UDP socket %d (%d:%s) - set local interface failed", __FUNCTION__, __LINE__, clientSocket_UDP, errno, strerror(errno));
//  }

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

    errno = 0;
    if (true == success)
    {
        memset(&DestAddr_TCP,0,sizeof(DestAddr_TCP));				// clear struct
        DestAddr_TCP.sin_family = AF_INET;					        // must be this
        DestAddr_TCP.sin_port = htons(TCPServerPort);	            // set the port to write to
        DestAddr_TCP.sin_addr.s_addr = inet_addr(ServerIPAddress);  // set destination IP address
        memset(&(DestAddr_TCP.sin_zero), '\0', 8);                  // zero the rest of the struct

        if (0 != connect(clientSocket_TCP,(struct sockaddr *)&DestAddr_TCP, sizeof(DestAddr_TCP)))
        {
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
    bool success = true;

    currentTopic        = 0;
    num_topics_total    = 1;
    prevPeriodChk       = 0;

    publishMe = malloc(sizeof(topicToPublish));
    if (NULL == publishMe)
    {
        syslog(LOG_ERR, "%s:%d MALLOC ERROR!", __FUNCTION__, __LINE__);
        success = false;
    }

    publishMe[ currentTopic ].topicSubscription = malloc(sizeof(MPinfo));
    if (NULL == publishMe[ currentTopic ].topicSubscription)
    {
        syslog(LOG_ERR, "%s:%d MALLOC ERROR!", __FUNCTION__, __LINE__);
        success = false;
    }

    recvFDL = malloc(sizeof(FDLinfo));
    if (NULL == recvFDL)
    {
        syslog(LOG_ERR, "%s:%d MALLOC ERROR!", __FUNCTION__, __LINE__);
        success = false;
    }

    recvFDL[0].mp = malloc(sizeof(int32_t)*10);
    if (NULL == recvFDL[0].mp)
    {
        syslog(LOG_ERR, "%s:%d MALLOC ERROR!", __FUNCTION__, __LINE__);
        success = false;
    }

    recvFDL[0].cop = malloc(sizeof(int32_t)*4);
    if (NULL == recvFDL[0].cop)
    {
        syslog(LOG_ERR, "%s:%d MALLOC ERROR!", __FUNCTION__, __LINE__);
        success = false;
    }

    recvFDL[0].crank = malloc(sizeof(int32_t)*4);
    if (NULL == recvFDL[0].crank)
    {
        syslog(LOG_ERR, "%s:%d MALLOC ERROR!", __FUNCTION__, __LINE__);
        success = false;
    }

    recvFDL[0].turbo = malloc(sizeof(int32_t)*2);
    if (NULL == recvFDL[0].turbo)
    {
        syslog(LOG_ERR, "%s:%d MALLOC ERROR!", __FUNCTION__, __LINE__);
        success = false;
    }

    return success;
}
