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

#define MAX_LAUNCH_ATTEMPTS    5
/* LAUNCH_CHECK_PERIOD must equal five to meet system requirements */
#define LAUNCH_CHECK_PERIOD	   5
/* START_ENSURE_DELAY must be greater than any possible amount of time that may 
 * pass between a child process being forked and the execl() command completion 
 * in the child process. */
#define START_ENSURE_DELAY     4

/* #if 0 disables printf()
 * #if 1 enables printf()  */
#if 1
  #define PRINT_F(a) printf a
#else
  #define PRINT_F(a) (void)0
#endif


/* DIRECTORIES TO OPEN/READ */
const char *dirs[] = 
{
    "/opt/rc360/system/",
    "/opt/rc360/modules/GE/",
    "/opt/rc360/modules/TPA/",
    "/opt/rc360/apps/GE/",
    "/opt/rc360/apps/TPA/"
};

/* LINKED LIST */
struct child_pid_list_struct
{
    pid_t child_pid;
    const char *dir;
    char *item_name;
    int32_t alive;
    struct child_pid_list_struct *next;
};

typedef struct child_pid_list_struct child_pid_list;
child_pid_list *first_node, *nth_node, *tmp_toFree;
int32_t aacm_loop = 0;

/* FUNCTION DECLARATIONS */
int32_t launch_item( const char *directory );
bool restart_process(void);
bool check_modules(void);

/* MAIN */
int32_t main(void)
{
    int32_t launch_status;
    int32_t dir_index;
    int32_t dirs_array_size;
    bool success = true;

    /* create linked list ... */
    errno = 0;
    first_node = (child_pid_list *)malloc(sizeof(child_pid_list));
    if ( 0 != errno )
    {
        syslog(LOG_ERR, "%s:%d ERROR: In 'malloc()' when creating 'first_node'! (%d: %s)", \
            __FUNCTION__, __LINE__, errno, strerror(errno));
        PRINT_F(("ERROR: In 'malloc()' when creating 'first_node'! (%d: %s) \n", \
            errno, strerror(errno)));
        success = false;
    }

    if ( NULL != first_node )
    {
        first_node->next = NULL;
        nth_node = first_node;
        PRINT_F(("FIRST NODE: %p \n" , (void *)first_node )); 
        PRINT_F(("NEXT NODE: %p \n" , (void *)first_node->next ));
    }
    else
    {
        syslog(LOG_ERR, "ERROR: In 'malloc()' when creating linked list!");
        PRINT_F(("ERROR: In 'malloc()' when creating linked list! \n"));
    }

    launch_status = 0;
    dir_index = 0;

    /* get number of elements in dirs ... */
    dirs_array_size = 0;
    while( '\0' != dirs[++dirs_array_size] );

    /* launch items in each dirs directory.  If directory empty, do nothing. */
    if ( success )
    {
        for (dir_index = 0; dir_index < dirs_array_size; dir_index++) 
        {
            PRINT_F(("\nOPEN DIRECTORY: %s \n", dirs[dir_index] ));

            launch_status = launch_item(dirs[dir_index]);
            if ( -1 == launch_status )
            {
                /* AACM was not able to start. */
                /* this is to give the test scripts time to confirm that there 
                 * were no lasting children. */
                sleep(1);   

                success = false;
                break;
            }
            else if (1 == launch_status)
            {
                syslog(LOG_NOTICE, "WARNING: Directory %s empty!", dirs[dir_index] );
                PRINT_F(("WARNING: Directory %s empty! \n\n", dirs[dir_index]));
            }
            else
            {
                syslog(LOG_NOTICE, "COMPLETED: All items in %s launched!", dirs[dir_index] );
                PRINT_F(("COMPLETED: All items in %s launched! \n", dirs[dir_index] ));
                PRINT_F(("CLOSE DIRECTORY: %s \n\n", dirs[dir_index] ));
            }
        }
        syslog(LOG_NOTICE, "COMPLETED: Launch sequence complete!");
        PRINT_F(("\nCOMPLETED: Launch sequence complete!\n\n"));
        /* all done, so last node is NULL */
        nth_node->next = NULL;
        
        while( success )
        {
            sleep(60);
            /* check if any of the processes have gone zombie or killed */
            /* list back to first node */
            nth_node = first_node;
            PRINT_F((" in while before checking \n"));
            success = check_modules( );
        }
    }
    /* Won't get here with the above while(1) loop.  But, if this main ever 
     * exits, this free's all the nodes in the linked list. */
    nth_node = first_node;
    while( NULL != nth_node->next )
    {
        tmp_toFree = nth_node;
        nth_node = nth_node->next;
        free(tmp_toFree);
    }
    free(first_node);
    return(0);
}



/* ================================================================================================
 * FUNCTION: int32_t launch_item (const char *directory)
 *      - Used to start the fork process of each item within a directory.  Currently we have three 
 *        directories with 'n' items in each.  
 *      - As for now, AACM will only have one ... not sure how many modules or apps will be used.   
 * 
 * IN:  const char* directory
 *      - use one of of the current directories ... as of now, just aacm, modules, and apps.  
 *
 * OUT: int32_t
 *      - will be used to check status after running
 *      ** I've got some printf usage to help debug for now ... will be sent to syslog() as well
 * 
 *  ... this will go in a .h
 * ============================================================================================= */
int32_t launch_item( const char *directory )
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
                PRINT_F(("ERROR: In 'malloc()' when combining strings! (%d: %s) \n", \
                    errno, strerror(errno)));
                success = false;
            }

            memcpy(concat, directory, len1);
            memcpy(concat+len1, dp->d_name, len2+1);

            PRINT_F(("\nLaunching ... %s \n", concat));

            // pid = fork();
            // if (0 == pid) 
            // {
            //     errno = 0;
            //     if ( (0 != execl( concat, dp->d_name, (char *)NULL)) )
            //     {
            //         syslog(LOG_ERR, "ERROR: 'execl()' failed for %s! (%d:%s)",
            //             concat, errno, strerror(errno));
            //         PRINT_F(("ERROR: 'execl()' failed for %s! (%d:%s) \n",
            //             concat, errno, strerror(errno)));
            //         /* force the spawned process to exit */
            //         exit(-errno);
            //     }
            // }
            // else if (-1 == pid)
            // {
            //     /* failed to fork a child process */
            //     syslog(LOG_ERR, "ERROR: Failed to fork child process for %s in %s! (%d:%s)", 
            //         dp->d_name, directory, errno, strerror(errno));
            //     PRINT_F(("ERROR: Failed to fork child process for %s in %s! (%d:%s) \n", 
            //         dp->d_name, directory, errno, strerror(errno)));

            //     success = false;
            //     break;
            // } 
            // else
            // {
            //     nth_node->child_pid = pid;                
            // }

            nth_node->dir = concat;
            errno = 0;
            temp = (char *) malloc(len2 + 1);
            if ( 0 != errno )
            {
                syslog(LOG_ERR, "%s:%d ERROR: In 'malloc()' for temporary string storage! (%d: %s)", \
                    __FUNCTION__, __LINE__, errno, strerror(errno));
                PRINT_F(("ERROR: In 'malloc()' for temporary string storage! (%d: %s) \n", \
                    errno, strerror(errno)));
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
                PRINT_F(("ERROR: In 'malloc()' when creating a new node! (%d: %s) \n", \
                    errno, strerror(errno)));
                success = false;
            }

            if ( ! restart_process() )
            {
                success = false;
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
                        PRINT_F(("ERROR: File %s not launched properly! (%d:%s) \n", \
                            dp->d_name, errno, strerror(errno)));

                        restart_process();
                        launch_attempts++;
                        nth_node->alive = 0;
                        run_time = 0;
                    }
                    else
                    {
                        run_time += LAUNCH_CHECK_PERIOD;
                    }
                }
                sleep(START_ENSURE_DELAY);
                errno = 0;
                nth_node->alive = waitpid(nth_node->child_pid, &rc, WNOHANG);

                if ( 0 != nth_node->alive )
                {
                    /* This last launch error needs to be outside the previous for loop in order to
                     * log the error for a fifth launch error. */
                    syslog(LOG_ERR, "ERROR: File launch failed! (%s in %s) (try #%d) (%d:%s)", \
                        dp->d_name, directory, launch_attempts, errno, strerror(errno));
                    PRINT_F(("ERROR: File %s not launched properly! (%d:%s) \n", \
                        dp->d_name, errno, strerror(errno)));

                    syslog(LOG_ERR, "NOTICE: Process for %s in %s disabled permanently! (%d:%s)", \
                        dp->d_name, directory, errno, strerror(errno));
                    PRINT_F(("NOTICE: Process for %s in %s disabled permanently! (%d:%s) \n", \
                        dp->d_name, directory, errno, strerror(errno)));

                    nth_node->alive = -1;

                    /* return a termination value to signal BARSM shutdown if it is AACM that failed */
                    if ( dirs[0] == directory )
                    {
                        syslog(LOG_ERR, "ERROR: AACM Cannot be started! (%d:%s)", \
                            errno, strerror(errno));
                        PRINT_F(("ERROR: AACM Cannot be started! (%d:%s) \n", \
                            errno, strerror(errno)));

                        success = false;
                    }
                }

                if ( NULL != nth_node->next )
                {
                    PRINT_F(( "CHILD linked list 'pid': %d \n" , nth_node->child_pid ));
                    PRINT_F(( "CHILD linked list 'dir': %s \n" , nth_node->dir ));
                    PRINT_F(( "CHILD linked list 'name': %s \n" , nth_node->item_name ));
                    PRINT_F(( "this address %p \n" , (void *)nth_node ));
                    PRINT_F(( "next address %p \n" , (void *)nth_node->next ));
                }
                else /* ( NULL == nth_node->next ) */
                {
                    syslog(LOG_ERR, "ERROR: In 'realloc()' when adding node to linked list!");
                    PRINT_F(("ERROR: In 'realloc()' when adding node to linked list! \n"));
                }
                nth_node = nth_node->next;
            }
        }
        dp = readdir(dir);
    }
    closedir(dir);
    if ( ! success )
    {
        return(-1);
    }
    else if ( empty_dir )
    {
        return(1);
    }
    else
    {
        return(0);
    }
}



/* ================================================================================================
 * FUNCTION: int32_t check_modules()
 *      - Used to check once if each of the child processes are running. 
 *      - Restarts any process that disappears or becomes a zombie as soon as it is found.
 *      - Logs an error for each dissapperance and zombie.  
 * 
 * IN:  VOID
 *      - Currently not used.
 * 
 * OUT: int
 *      - Currently not used.
 * 
 * ... this will go in a .h
 * ============================================================================================= */
bool check_modules(void)
{
    bool success = true;
    int32_t rc; 
    int32_t waitreturn;

    /* check status of the child processes that are supposed to be running */
    PRINT_F(("\n\n in check_modules \n\n"));

    while( NULL != nth_node->next )
    {
        errno = 0;
        waitreturn = waitpid(nth_node->child_pid, &rc, WNOHANG);

        if ( -1 == nth_node->alive )
        {
            /* the process was not able to start up, so ignore */
        }
        else if (-1 == waitreturn)
        {
            syslog(LOG_ERR, "ERROR: Process for %s with PID %d no longer exists! (%d:%s)", \
                nth_node->dir, nth_node->child_pid, errno, strerror(errno));
            PRINT_F(("\nERROR: Process for %s with PID %d no longer exists! (%d:%s) \n", \
                nth_node->dir, nth_node->child_pid, errno, strerror(errno)));
            /* alive == 1 indicates that this process needs to be replaced */
            nth_node->alive = 1;   
        }
        else if (0 < waitreturn)
        {
            syslog(LOG_ERR, "ERROR: Process for %s with PID %d has changed state! (%d:%s)", \
                nth_node->dir, nth_node->child_pid, errno, strerror(errno));
            PRINT_F(("\nERROR: Process for %s with PID %d has changed state! (%d:%s) \n", \
                nth_node->dir, nth_node->child_pid, errno, strerror(errno)));
            /* alive == 1 indicates that this process needs to be replaced */
            nth_node->alive = 1;
        }
        if (1 == nth_node->alive)
        {
            syslog(LOG_NOTICE, "NOTICE: Restarting %s...", nth_node->item_name);
            PRINT_F(("NOTICE: Restarting %s...", nth_node->item_name));
            success = restart_process();
        }

        nth_node = nth_node->next;
    }
    return(success);
}



/* ================================================================================================
 * FUNCTION: void restart_process()
 *      - Used to start a process to replace one that ended prematurely or wouldn't start.
 *      - Sets the 'alive' portion of the struct to 0 to indicate that the process is functioning 
 *        properly.
 *      - Starts up a new process to run the same file that shut down, using the same list link for 
 *        the failed process.
 *      - Restarts whatever process is described by the information in the active link in the 
 *        linked list.
 * 
 * IN:  VOID
 *      - Currently not used.
 * 
 * OUT: VOID
 *      - Currently not used.
 * 
 *  ... this will go in a .h
 * ============================================================================================= */
bool restart_process(void)
{
    bool success = true;
    /* fork a new process to start the module back up */
    pid_t new_pid;  
    new_pid = fork();
    if (0 == new_pid)
    {
        /* execute the file again in the new child process */
        errno = 0;
        if ( (0 != execl(nth_node->dir, nth_node->item_name, (char *)NULL)) )
        {
            syslog(LOG_ERR, "ERROR: 'execl()' failed for %s! (%d:%s)", \
                nth_node->dir, errno, strerror(errno));
            PRINT_F(("ERROR: 'execl()' failed for %s! (%d:%s) \n", \
                nth_node->dir, errno, strerror(errno)));
            /* force the spawned process to exit */
            exit(-errno);
        }
    }
    else if (-1 == new_pid)
    {
        syslog(LOG_ERR, "ERROR: Failed to fork child process for %s! (%d:%s)", \
            nth_node->dir, errno, strerror(errno));

        success = false;
    } 
    else
    {
        nth_node->child_pid = new_pid;
        /* alive == 0 indicates that the process has been restarted and should be good */
        nth_node->alive = 0;
    }
    return (success);
}