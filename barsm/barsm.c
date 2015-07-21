//=======================================
// GET
// ASSIMILATOR 
// BARSM
// 
// valgrind: --read-var-info=yes --leak-check=full --track-origins=yes --show-reachable=yes --malloc-fill=B5 --free-fill=4A
// ======================================

#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <stdint.h>

#define AACM_MAX_LAUNCH_ATTEMPTS    5

// #if 0 disables printf()
// #if 1 enables printf()
#if 1
  #define PRINT_F(a) printf a
#else
  #define PRINT_F(a) (void)0
#endif


// DIRECTORIES TO OPEN/READ 
const char *dirs[] = 
{
    "/opt/rc360/system/",       // AACM
    "/opt/rc360/modules/GE/",   // GE MODULES
    "/opt/rc360/modules/TPA/",  // TP MODULES
    "/opt/rc360/apps/GE/",      // GE APPS
    "/opt/rc360/apps/TPA/"      // TP APPS
};

// LINKED LIST  
struct child_pid_list_struct
{
    pid_t child_pid;
    const char *dir;
    char *item_name;
    int8_t alive;
    struct child_pid_list_struct *next;
};

typedef struct child_pid_list_struct child_pid_list; 
child_pid_list *first_node, *nth_node;
int32_t aacm_loop = 0;

// FUNCTION DECLARATIONS
int32_t launch_item( const char *directory );
void restart_process();
int32_t check_modules();

// MAIN
int32_t main( int argc , char *argv[] )
{
    // create linked list ...
    child_pid_list *first_node = (child_pid_list *)malloc(sizeof(child_pid_list));

    if ( NULL != first_node )
    {
        first_node->next = NULL;
        nth_node = first_node;
        PRINT_F(("FIRST NODE: %p \n" , first_node )); 
        PRINT_F(("NEXT NODE: %p \n" , first_node->next ));
    }
    else
    {
        syslog(LOG_ERR, "malloc() error when creating linked list.");
        PRINT_F(("BAD malloc \n"));
    }
    // end create linked list ... 

    int32_t launch_status = 0;
    int32_t dir_index = 0;

    // get number of elements in dirs ...
    int32_t dirs_array_size = 0;
    while( '\0' != dirs[++dirs_array_size] );

    // launch items in each dirs directory.  If directory empty, do nothing.  
    for (dir_index = 0; dir_index < dirs_array_size; dir_index++) 
    {
        PRINT_F(("OPEN DIRECTORY: %s \n", dirs[dir_index] ));
        if ( (0 == dir_index) ) 
        {
            for ( aacm_loop = 0; aacm_loop < AACM_MAX_LAUNCH_ATTEMPTS; aacm_loop++ ) 
            {
                launch_status = /* launch_item(dirs[dir_index]) */ 1;
                if (1 == launch_status)
                {
                    syslog(LOG_ERR, "Failed to launch AACM! Attempt: %d", ( aacm_loop + 1 ) );
                    PRINT_F(("DIRECTORY %s IS EMPTY! NOTHING LAUNCHED! ATTEMPT: %d \n", dirs[dir_index] , ( aacm_loop + 1) ));
                    // allow a wait between failed launch attempts
                    sleep(5);
                }
                else
                {
                    syslog(LOG_NOTICE, "ITEMS IN %s LAUNCHED!", dirs[dir_index]  );
                    PRINT_F(("ITEMS IN %s LAUNCHED! \n", dirs[dir_index] ));
                    PRINT_F(("CLOSE DIRECTORY: %s \n\n", dirs[dir_index] ));
                    break;
                }
            }
            if ( AACM_MAX_LAUNCH_ATTEMPTS == aacm_loop ) 
            {
                syslog(LOG_ERR, "Failed to launch! Max attempts reached: %d", ( aacm_loop ) );
                PRINT_F(("\n\nMAX LAUNCH ATTEMPTS REACHED!  STOP LOADING! \n\n"));
                /* break; */
                exit(1);
            }
            else
            {
                // TCP server/client connection needs to be made here ...
                // BARSM_TO_AACM_INIT_MSG ... to GE-app
                // BARSM_TO_AACM_INIT_MSG_ACK ... from GE-app
            }
        }
        else
        {
            launch_status = launch_item(dirs[dir_index]);
            if (1 == launch_status)
            {
                syslog(LOG_ERR, "DIRECTORY %s IS EMPTY! NOTHING LAUNCHED!", dirs[dir_index] );
                PRINT_F(("DIRECTORY %s IS EMPTY! NOTHING LAUNCHED! \n\n", dirs[dir_index]));
            }
            else
            {
                syslog(LOG_NOTICE, "ITEMS IN %s LAUNCHED!", dirs[dir_index] );
                PRINT_F(("ITEMS IN %s LAUNCHED! \n", dirs[dir_index] ));
                PRINT_F(("CLOSE DIRECTORY: %s \n\n", dirs[dir_index] ));
            }
        }
    }
    PRINT_F(("\n\nDONE START EVERYTHING!\n\n"));
    // all done, so last node is NULL
    nth_node->next = NULL;
    
    
    // print list ... similiar usage to traverse and check pids ...
    
//  while( NULL != nth_node->next )
//  {
//      PRINT_F(( "PID: %d \n"   , nth_node->child_pid ));
//      PRINT_F(( "DIR: %s \n"    , nth_node->dir ));
//      PRINT_F(( "NAME: %s \n\n" , nth_node->item_name ));
//      nth_node = nth_node->next;
//  }

    while(1)
    {
        // check if any of the processes have gone zombie or killed
        nth_node = first_node;  // list back to first node
        PRINT_F(("\n\n in while before checking \n\n"));
        check_modules( );
        sleep(60);
    }

    return(0);

    // SYS_INIT
    // 
    // while()
    // {
    //      SEND:   BARSM_TO_AACM_PROCESSES ... list of processes and names to AACM
    //      if (BAD == pid_health_status)
    //      {
    //          SEND:   BARSM_TO_AACM_MSG ... health check fail, send it to AACM
    //          RCV:    BARSM_TO_AACM_MSG_ACK ... AACM got it
    //      }     
    //      if (RCV:    AACM_TO_BARSM_MSG) ... AACM doesn't have comm with some module (internal comm loss)
    //      { 
    //          SEND:   AACM_TO_BARSM_MSG_ACK ... Ok, restart modules/apps
    //      }
    // }
    //
}



//===========================================================================================================================================
// FUNCTION: int32_t launch_item (const char *directory)
//      - Used to start the fork process of each item within a directory.  Currently we have three directories with 'n' items in each.  
//      - As for now, AACM will only have one ... not sure how many modules or apps will be used.   
// 
// IN:  const char* directory
//      - use one of of the current directories ... as of now, just aacm, modules, and apps.  
// 
// OUT: int32_t
//      - will be used to check status after running
//      ** I've got some printf usage to help debug for now ... will be sent to syslog() as well
// 
//  ... this will go in a .h
//===========================================================================================================================================
int32_t launch_item( const char *directory )
{
    int32_t rc = 0;             // I don't like the name rc ... change  TODO
    struct dirent *dp;          // I don't like the name dp ... change  TODO
    int32_t dir_index = 0;  
    int32_t return_val = 0; 
    int32_t empty_dir = 1;
    int32_t k = 0;   

    DIR *dir;
    errno = 0;
    dir = opendir(directory);
    
    while ( NULL != ( dp = readdir(dir) ) ) 
    {
        if ( '.' != dp->d_name[0] ) 
        {
            empty_dir = 0;  // directory is NOT empty

            size_t len1 = strlen(directory), len2 = strlen(dp->d_name);
            char *concat = (char*) malloc(len1 + len2 + 1);

            memcpy(concat, directory, len1);
            memcpy(concat+len1, dp->d_name, len2+1);

            PRINT_F(("Launching ... %s \n", concat));

            // start forking
            pid_t pid;  
            pid = fork();
            if (0 == pid) 
            {
                /* successfully childreated a child process, now start the required 
                * application */
                errno = 0;
                if ( (0 != execl(concat, dp->d_name, (char *)NULL)) )
                {
                    syslog(LOG_ERR, "Failed to launch! (%d:%s)", errno, strerror(errno));
                    PRINT_F(("failed to launch! (%d:%s) \n", errno, strerror(errno)));
                    /* force the spawned process to exit */
                    exit(-errno);
                }
            }
            else if (-1 == pid)
            {
                /* failed to fork a child process */
                syslog(LOG_ERR, "Failed to fork child process! (%d:%s)", errno, strerror(errno));
                PRINT_F(("failed to fork child process!: (%d:%s) \n", errno, strerror(errno)));
            } 
            else
            {
                nth_node->next = (child_pid_list *)malloc(sizeof(child_pid_list));
                nth_node->child_pid = pid;
                nth_node->dir = concat;
                nth_node->item_name = dp->d_name;
                nth_node->alive = 1;

                //syslog( LOG_NOTICE , "Child PID %d added to list" , nth_node->child_pid );

                if (NULL != nth_node->next )
                {
                    PRINT_F(( "CHILD linked list 'pid': %d \n" , nth_node->child_pid ));
                    PRINT_F(( "CHILD linked list 'dir': %s \n" , concat ));
                    PRINT_F(( "CHILD linked list 'name': %s \n" , nth_node->item_name ));
                    PRINT_F(( "this address %p \n" , nth_node ));
                    PRINT_F(( "next address %p \n" , nth_node->next )); 
                }
                else
                {
                    syslog(LOG_ERR, "realloc() error when adding node to linked list.");
                    PRINT_F(("BAD malloc \n"));
                }
                nth_node = nth_node->next;
            }
        }
        sleep(1);
    }
    return_val = empty_dir;
    return return_val;
}



//===========================================================================================================================================
// FUNCTION: int32_t check_modules()
//      - Used to check once if each of the child processes are running. 
//      - Restarts any process that disappears or becomes a zombie as soon as it is found.
//      - Logs an error for each dissapperance and zombie.  
// 
// IN:  VOID
//      - Currently not used.
// 
// OUT: int
//      - Currently not used.
// 
//  ... this will go in a .h
//===========================================================================================================================================
int32_t check_modules()
{
    int32_t rc; 
    int32_t waitreturn;

    // check status of the child processes that are supposed to be running
    PRINT_F(("\n\n in check_modules \n\n"));

    while( NULL != nth_node->next )
    {
        errno = 0;
        waitreturn = waitpid(nth_node->child_pid, &rc, WNOHANG);

        if (-1 == waitreturn)
        {
            // killed
            printf("\nERROR: Child process with PID %d is not existent. Restarting...\n", nth_node->child_pid);
            nth_node->alive = -1;   // -1 indicates that this process needs to be replaced
        }
        else if (0 < waitreturn)
        {
            // zombied
            printf("\nERROR: Child process with PID %d is a zombie. Restarting...\n", waitreturn);
            nth_node->alive = -1;   // -1 indicates that this process needs to be replaced
        }
        if (-1 == nth_node->alive)  // either killed or zombied, restart ...
        {
            restart_process();
        }

        PRINT_F(( "PID: %d \n"   , nth_node->child_pid ));
        PRINT_F(( "DIR: %s \n"    , nth_node->dir ));
        PRINT_F(( "NAME: %s \n" , nth_node->item_name ));
        PRINT_F(( "ALIVE: %d \n" , nth_node->alive ));
        nth_node = nth_node->next;
    }
    sleep(1);   // added to help see printf comments.  
    return(0);
}



//===========================================================================================================================================
// FUNCTION: void restart_process()
//      - Used to start a file back up that stopped running when it shouldn't have.
//      - Sets the 'alive' portion of the struct to 0 to indicate that the process has failed at one point.
//      - Starts up a new process to run the same file that shut down, using the same list link for the failed process.
//      - Restarts whatever process is described by the information in the active link in the linked list.
// 
// IN:  VOID
//      - Currently not used.
// 
// OUT: VOID
//      - Currently not used.
// 
//  ... this will go in a .h
//===========================================================================================================================================
void restart_process()
{
    
    // fork a new process to start the module back up
    pid_t new_pid;  
    new_pid = fork();
    if (0 == new_pid) 
    {
        // execute the file again in the new child process
        errno = 0;
        if ( (0 != execl(nth_node->dir, nth_node->item_name, (char *)NULL)) )
        {
            syslog(LOG_ERR, "failed to launch! (%d:%s)", errno, strerror(errno));
            // force the spawned process to exit
            exit(-errno);
        }
    }
    else if (-1 == new_pid)
    {
        syslog(LOG_ERR, "failed to fork child process! (%d:%s)", errno, strerror(errno));
    } 
    else
    {
        // add the new slot for the new process
        nth_node->child_pid = new_pid;
        nth_node->alive = 0;    // 0 indicates it has been restarted at least once.
    }
}