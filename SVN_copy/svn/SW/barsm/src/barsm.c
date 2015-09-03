/*
 * File: barsm.c
 * Copyright (c) 2015, DornerWorks, Ltd.
 *
 * Description:
 *   This application handles the startup, health monitoring, and upkeep
 *   of the all the software applications and modules in the GE RC360
 *   Assimilator.
 */

/* valgrind: --read-var-info=yes --leak-check=full --track-origins=yes --show-reachable=yes 
   --malloc-fill=B5 --free-fill=4A */

#include <sys/types.h>
#include <sys/wait.h>
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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>

#include "barsm_functions.h"


/****************
* PRIVATE CONSTANTS
****************/
#define MAX_LAUNCH_ATTEMPTS    5
/* LAUNCH_CHECK_PERIOD is the amount of time given between status checks on a 
 * module/appplication immediately after launch. Recommmended: 5 */
#define LAUNCH_CHECK_PERIOD    5
/* START_ENSURE_DELAY must be greater than any possible amount of time that may 
 * pass between a child process being forked and the execl() command completion 
 * in the child process. Recommmended: 4 */
#define START_ENSURE_DELAY     4

char UDPAddress[] = "127.0.0.1";
int UDPPort =  4096;

/* THREADS */
static pthread_t msgRcvThread;

/* RETURN VALUE ENUMS */
enum e_return
{
    terminalError = -1,
    dirIsEmpty    = 1,
    normal        = 0,
};

/****************
* GLOBALS
****************/
child_pid_list *first_node, *nth_node, *tmp_toFree;

static int32_t clientSocket_TCP = -1;
static int32_t clientSocket_UDP = -1;
struct sockaddr_in DestAddr_TCP;
struct sockaddr_in DestAddr_UDP;

const char *dirs[] = 
{
    "/opt/rc360/system/",
    "/opt/rc360/modules/GE/",
    "/opt/rc360/modules/TPA/",
    "/opt/rc360/apps/GE/",
    "/opt/rc360/apps/TPA/"
};

/****************
* PRIVATE FUNCTION PROTOTYPES
****************/
static bool aacmSetup(void);
static int32_t launch_itemsInDir( const char *directory );
static void* rcv_errMsgs(void *param);



/**
 * 
 *
 * @param[in] UNUSED(argc) 
 * @param[in] UNUSED(*argv[]) 
 * @param[out] true/false
 *
 * @return true/false status of system startup success
 */
/* MAIN */
bool main(int32_t UNUSED(argc), char UNUSED(*argv[]))
{
    int32_t launch_status;
    int32_t dir_index;
    int32_t dirs_array_size;
    bool success = true;
    int32_t rc;
    char barsm_name[4];

    /* create linked list ... */
    errno = 0;
    first_node = (child_pid_list *)malloc(sizeof(child_pid_list));
    if ( 0 != errno && success )
    {
        syslog(LOG_ERR, "%s:%d ERROR: In 'malloc()' when creating 'first_node'! (%d: %s)", \
            __FUNCTION__, __LINE__, errno, strerror(errno));
        printf("ERROR: In 'malloc()' when creating 'first_node'! (%d: %s) \n", \
            errno, strerror(errno));
        success = false;
    }

    if ( success )
    {
        if ( NULL != first_node )
        {
            first_node->next = NULL;
            nth_node = first_node;
            printf("FIRST NODE: %p \n" , (void *)first_node ); 
            printf("NEXT NODE: %p \n" , (void *)first_node->next );
        }
        else
        {
            syslog(LOG_ERR, "ERROR: In 'malloc()' when creating linked list!");
            printf("ERROR: In 'malloc()' when creating linked list! \n");
        }

        launch_status = 0;
        dir_index = 0;

        /* get number of elements in dirs ... */
        dirs_array_size = 0;
        while( '\0' != dirs[++dirs_array_size] );
    } /* if (success) */

    /* launch items in each dirs directory.  If directory empty, do nothing. */
    if ( success )
    {
        for (dir_index = 0; dir_index < dirs_array_size; dir_index++) 
        {
            printf("\nOPEN DIRECTORY: %s \n", dirs[dir_index] );

            launch_status = launch_itemsInDir(dirs[dir_index]);
            if ( terminalError == launch_status )
            {
                /* AACM was not able to start. */
                /* this is to give the test scripts time to confirm that there 
                 * were no lasting children. */
                sleep(1);   

                success = false;
                break;
            }
            else if ( dirIsEmpty == launch_status )
            {
                syslog(LOG_NOTICE, "WARNING: Directory %s empty!", dirs[dir_index] );
                printf("WARNING: Directory %s empty! \n\n", dirs[dir_index]);
            }
            else /* ( normal == launch_status ) */
            {
                syslog(LOG_NOTICE, "COMPLETED: All items in %s launched!", dirs[dir_index] );
                printf("COMPLETED: All items in %s launched! \n", dirs[dir_index] );
                printf("CLOSE DIRECTORY: %s \n\n", dirs[dir_index] );

                if ( 0 == dir_index )
                {
                    // connect to the AACM TCP server
                    // send startup message
                    // receive ack
                    if ( success )
                    {
                        success = aacmSetup();
                    }

                    // start thread for recieving AACM messages
                    if ( success )
                    {
                        errno = 0;
                        rc = pthread_create(&msgRcvThread, NULL, rcv_errMsgs, NULL);
                        if ( 0 != rc )
                        {
                            success = false;
                            syslog(LOG_ERR, "%s:%d ERROR! failed to create TCP thread (%d:%s)", \
                                __FUNCTION__, __LINE__, errno, strerror(errno));
                        }
                    }
                    else
                    {
                        break;
                    }
                } /* if ( 0 == dir_index ) */
            } /* else if (normal == launch_status) */
        } /* for (dir_index = 0; dir_index < dirs_array_size; dir_index++) */
        syslog(LOG_NOTICE, "COMPLETED: Launch sequence complete!");
        printf("\nCOMPLETED: Launch sequence complete!\n\n");
        /* all done, so last node is NULL */
        nth_node->next = NULL;

        /* BARSM needs to be assigned a name as all the child processes were 
         * given when they were launched */
        if ( success ) 
        {
            assign_procName( &barsm_name[0] );
        }
        if ( success )
        {
            success = process_sysInit(clientSocket_UDP);
        }
        if ( success )
        {
            success = send_barsmToAacmProcesses(clientSocket_TCP, first_node, \
                &dirs[0], &barsm_name[0]);
        }
        
        while( success )
        {
            sleep(60);
            /* check if any of the processes have gone zombie or killed */
            /* list back to first node */
            nth_node = first_node;
            printf(" in while before checking \n");
            success = check_modules(nth_node);
        }
    } /* if (success) */

    syslog(LOG_NOTICE, "NOTICE: BARSM exiting due to terminal error!");

    /* Won't get here with the above while(1) loop.  But, if this main ever 
     * exits, this free's all the nodes in the linked list. */
    nth_node = first_node;
    while( NULL != nth_node->next )
    {    
        /* Need to kill all child processes that were started up */
        errno = 0;        
        if ( -1 == kill(nth_node->child_pid, SIGTERM) )
        {
            syslog(LOG_ERR, "%s:%d ERROR! Not able to kill process with PID %d (%d:%s)", \
                __FUNCTION__, __LINE__, nth_node->child_pid, errno, strerror(errno));
        }

        tmp_toFree = nth_node;
        nth_node = nth_node->next;
        free(tmp_toFree);
    }
    free(first_node);
    return(success);
} /* int32_t main(void) */



/**
 * Runs through the steps required to communicate with the AACM  after launching
 * it, including setting up the sockets and seding an initial message over them.
 *
 * @param[in] void
 *
 * @return true/false whether a terminal error has occured
 */
bool aacmSetup(void)
{
    bool success            = true;

    if ( success )
    {
        success = TCPsetup( &clientSocket_TCP, DestAddr_TCP );
        printf("TCP Setup Complete!\n");
    }
    if ( success )
    {
        success = UDPsetup( &clientSocket_UDP, DestAddr_UDP, UDPPort, &UDPAddress[0] );
        printf("UDP Setup Complete!\n");
    }
    if ( success )
    {
        success = send_barsmToAacmInit(clientSocket_TCP);
        printf("BARSM to AACM INIT sent!\n");
    }
    if ( success )
    {
        success = receive_barsmToAacmInitAck(clientSocket_TCP);
        printf("INIT ACK recieved!\n");
    }

    return (success);
}



/**
 * Used to launch all the items in a passed in directory location. This function 
 * indicates if a terminal error has occurred, or if the directory is empty with
 * the return value.
 *
 * @param[in] const char *directory: The directory location from which things 
 *      need to be luanched.
 *
 * @return int: To indicate normal operation, empty directory, or terminal error
 */
int32_t launch_itemsInDir( const char *directory )
{
    int32_t rc = 0;
    int32_t launch_attempts = 0, run_time = 0;

    struct dirent *dp;

    bool success = true;
    bool empty_dir = true;

    size_t len1, len2; 

    char *concat, *temp;

    DIR *dir;
    errno = 0;
    dir = opendir(directory);

    if ( (ENOENT == errno) )
    {
        syslog(LOG_ERR, "%s:%d ERROR: Directory does not exist, or name is an empty string. (%d:%s)",__FUNCTION__, __LINE__, errno, strerror(errno));
    }
    else if (( ENOTDIR == errno ) )
    {
        syslog(LOG_ERR, "%s:%d ERROR: Name is not a directory. (%d:%s)",__FUNCTION__, __LINE__, errno, strerror(errno));
    }
    else
    {
        dp = readdir(dir);
        while ( NULL != dp )
        {
            if ( '.' != dp->d_name[0] ) 
            {
                empty_dir = false;
     
                len1 = strlen(directory);
                len2 = strlen(dp->d_name);
                errno = 0;  
                concat = (char*) malloc(len1 + len2 + 1);
                if ( 0 != errno )
                {
                    syslog(LOG_ERR, "%s:%d ERROR: In 'malloc()' when combining strings! (%d: %s)", \
                        __FUNCTION__, __LINE__, errno, strerror(errno));
                    printf("ERROR: In 'malloc()' when combining strings! (%d: %s) \n", \
                        errno, strerror(errno));
                    success = false;
                }
                memcpy(concat, directory, len1);
                memcpy(concat+len1, dp->d_name, len2+1);
                nth_node->dir = concat;

                errno = 0;
                temp = (char *) malloc(len2 + 1);
                if ( 0 != errno )
                {
                    syslog(LOG_ERR, "%s:%d ERROR: In 'malloc()' for temporary string storage! (%d: %s)", \
                        __FUNCTION__, __LINE__, errno, strerror(errno));
                    printf("ERROR: In 'malloc()' for temporary string storage! (%d: %s) \n", \
                        errno, strerror(errno));
                    success = false;
                }
                temp = strdup(dp->d_name);
                nth_node->item_name = temp;

                errno = 0;
                nth_node->next = (child_pid_list *)malloc(sizeof(child_pid_list));
                if ( 0 != errno )
                {
                    syslog(LOG_ERR, "%s:%d ERROR: In 'malloc()' when creating a new node! (%d: %s)", \
                        __FUNCTION__, __LINE__, errno, strerror(errno));
                    printf("ERROR: In 'malloc()' when creating a new node! (%d: %s) \n", \
                        errno, strerror(errno));
                    success = false;
                }

                assign_procName( &nth_node->proc_name[0] );

                printf("\nLaunching ... %s \n", concat);

                if ( success )
                {
                    success = start_process(nth_node);
                }
                
                if ( ! success )
                {
                    break;
                }
                else
                {
                    /* Check if the process just launched was successful, if not, try up to four times
                     * to restart it. The waitpid() function will return a false positive if the child 
                     * process has not yet tried to execl(). The run_time value is used to make sure 
                     * that enough time has been given to make sure that execl() was called. */
                    launch_attempts = 1;
                    run_time = 0;
                    while ( launch_attempts < MAX_LAUNCH_ATTEMPTS && run_time <= START_ENSURE_DELAY )
                    {
                        sleep(LAUNCH_CHECK_PERIOD);
                        errno = 0;
                        nth_node->alive = waitpid(nth_node->child_pid, &rc, WNOHANG);

                        if ( 0 != nth_node->alive )
                        {
                            syslog(LOG_ERR, "ERROR: File launch failed! (%s in %s) (try #%d) (%d:%s)", \
                                dp->d_name, directory, launch_attempts, errno, strerror(errno));
                            printf("ERROR: File %s not launched properly! (%d:%s) \n", \
                                dp->d_name, errno, strerror(errno));

                            start_process(nth_node);
                            launch_attempts++;
                            nth_node->alive = 0;
                            run_time = 0;
                        }
                        else
                        {
                            run_time += LAUNCH_CHECK_PERIOD;
                        }
                    } /* while ( launch_attempts < MAX_LAUNCH_ATTEMPTS && run_time <= START_ENSURE_DELAY ) */
                    sleep(START_ENSURE_DELAY);
                    errno = 0;
                    nth_node->alive = waitpid(nth_node->child_pid, &rc, WNOHANG);

                    if ( 0 != nth_node->alive )
                    {
                        /* This last launch error needs to be outside the previous for loop in order to
                         * log the error for a fifth launch error. */
                        syslog(LOG_ERR, "ERROR: File launch failed! (%s in %s) (try #%d) (%d:%s)", \
                            dp->d_name, directory, launch_attempts, errno, strerror(errno));
                        printf("ERROR: File %s not launched properly! (%d:%s) \n", \
                            dp->d_name, errno, strerror(errno));

                        syslog(LOG_ERR, "NOTICE: Process for %s in %s disabled permanently! (%d:%s)", \
                            dp->d_name, directory, errno, strerror(errno));
                        printf("NOTICE: Process for %s in %s disabled permanently! (%d:%s) \n", \
                            dp->d_name, directory, errno, strerror(errno));

                        nth_node->alive = -1;

                        /* return a termination value if it is AACM that failed */
                        if ( dirs[0] == directory )
                        {
                            syslog(LOG_ERR, "ERROR: AACM Cannot be started! (%d:%s)", \
                                errno, strerror(errno));
                            printf("ERROR: AACM Cannot be started! (%d:%s) \n", \
                                errno, strerror(errno));

                            success = false;
                        }
                    } /* if ( 0 != nth_node->alive ) */

                    if ( NULL == nth_node->next )
                    {
                        syslog(LOG_ERR, "ERROR: In 'realloc()' when adding node to linked list!");
                        printf("ERROR: In 'realloc()' when adding node to linked list! \n");
                        success = false;
                    }
                    
                    printf( "CHILD linked list 'pid': %d \n" , nth_node->child_pid );
                    printf( "CHILD linked list 'dir': %s \n" , nth_node->dir );
                    printf( "CHILD linked list 'name': %s \n" , nth_node->item_name );
                    printf( "this address %p \n" , (void *)nth_node );
                    printf( "next address %p \n" , (void *)nth_node->next );

                    nth_node = nth_node->next;
                } /* else if ( success ) */
            } /* if ( '.' != dp->d_name[0] ) */
            dp = readdir(dir);
        } /* while ( NULL != dp ) */
        closedir(dir);
    } /* else if (errno != ENOENT && errno != ENOTDIR) */

    if ( ! success )
    {
        return(terminalError);
    }
    else if ( empty_dir )
    {
        return(dirIsEmpty);
    }
    else
    {
        return(normal);
    }
} /* int32_t launch_itemsInDir( const char *directory ) */



/**
 * Used to run the thread which watches for messages from the AACM and responds
 * to the messages by restarting or possibly terminating (not yet implemented) 
 * the modules/applications.
 *
 * @param[in] void * UNUSED(param)
 *
 * @return void
 */
void* rcv_errMsgs( void * UNUSED(param) )
{
    int32_t rc;
    bool success = true;

    rc = pthread_detach( pthread_self() );
    if (rc != 0)
    {
        success = false;
        syslog(LOG_ERR, "%s:%d ERROR! Failed to detach thread (%d:%s)", \
            __FUNCTION__, __LINE__, errno, strerror(errno));
    }
    
    while ( success )
    {
        printf("Waiting for aacmToBarsm MSG... \n");
        receive_aacmToBarsm( clientSocket_TCP, first_node );
        send_aacmToBarsmAck( clientSocket_TCP );
    }

    return (0);
}

