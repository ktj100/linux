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

#if 0
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
void restart_process(int32_t s_pid, child_pid_list **first_node_ptr, child_pid_list **nth_node_ptr);
int32_t check_modules(child_pid_list **first_node_ptr, child_pid_list **nth_node_ptr);


// MAIN
int32_t main()
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
                launch_status = launch_item(dirs[dir_index]);
                if (1 == launch_status)
                {
                    syslog(LOG_ERR, "Failed to launch AACM! Attempt: %d", ( aacm_loop + 1 ) );
                    PRINT_F(("DIRECTORY %s IS EMPTY! NOTHING LAUNCHED! ATTEMPT: %d \n", dirs[dir_index] , ( aacm_loop + 1) ));
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
                break;
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
    // all done, so last node is NULL
    nth_node->next = NULL;
    
    
    // print list ... similiar usage to traverse and check pids ...
    nth_node = first_node;
    while( NULL != nth_node->next )
    {
        PRINT_F(( "PID: %d \n"   , nth_node->child_pid ));
        PRINT_F(( "DIR: %s \n"    , nth_node->dir ));
        PRINT_F(( "NAME: %s \n\n" , nth_node->item_name ));
        nth_node = nth_node->next;
    }

    while(1)
    {
        // check if any of the processes have gone zombie
        check_modules(&first_node, &nth_node);
        
        // restart any processes that have gone zombie or disappeared
        nth_node = first_node;
        while( nth_node->next != NULL)
        {
            if(-1 == nth_node->alive)
            {
                // restart stopped child
                restart_process(nth_node->child_pid, &first_node, &nth_node);
            }
            nth_node = nth_node->next;
        }
        sleep(10);
    }

    return(0);
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
    struct dirent *dp;      // I don't like the name dp ... change  TODO
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

            // concatenates 'directory' string and 'd_name' string to be used for execl()
            // moved here to stop valgrind message (in parent): Conditional jump or move depends on uninitialised value(s)
            // message started when opening aacm directory.  The message is gone, but now it's displayed when opening the next directory. 
            //char *concat = (char*) malloc(strlen(directory) + strlen(dp->d_name) + 1);      
            
            // I think the valgrind message is related to how I'm using strcat( dest , src )
            // " ... src does not need to be null-terminated if it contains n or more bytes."
            // So ... this was giving some potential problems seen in valgrind.  I'm going to try memcpy() next.   
            //strcat( concat , directory );        
            //strcat( concat , dp->d_name );

            // memcpy to help with valgrind messages.  
            // no messages/errors ... goign with memcpy() instead of strcat()
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
// FUNCTION: int32_t check_modules(child_pid_list **first_node_ptr, child_pid_list **nth_node_ptr)
//      - Used to check once if each of the child processes are running. 
//      - Only checks linked items with an 'alive' value of 1, which indicates that the process should be running.  
// 
// IN:  child_pid_list **first_node_ptr, child_pid_list **nth_node_ptr
//      - Used to allow reading and manipulation of the linked list used to store data for all the child processes.
// 
// OUT: int
//      - Currently not used.
// 
//  ... this will go in a .h
//===========================================================================================================================================

int32_t check_modules(child_pid_list **first_node_ptr, child_pid_list **nth_node_ptr)
{
    int32_t rc; 
    int32_t waitreturn;

    // set up linked list for this function
    child_pid_list *nth_node = *nth_node_ptr;
    child_pid_list *first_node = *first_node_ptr;

    // check status of the child processes that are supposed to be running
    nth_node = first_node;
    while (nth_node->next != NULL)
    {
        if (1 == nth_node->alive)
        {
            errno = 0;
            waitreturn = waitpid(nth_node->child_pid, &rc, WNOHANG);

            if (-1 == waitreturn) 
            {
                printf("\nERROR: Child process with PID %d is not existent. Restarting...\n", nth_node->child_pid);
                nth_node->alive = -1;   // -1 indicates that this process needs to be replaced
            }
            else if (0 < waitreturn)
            {
                printf("\nERROR: Child process with PID %d is a zombie. Restarting...\n", waitreturn);
                nth_node->alive = -1;   // -1 indicates that this process needs to be replaced
            }
        }
        nth_node = nth_node->next;
    }
    return(0);
}



//===========================================================================================================================================
// FUNCTION: void restart_process(int s_pid, child_pid_list **first_node_ptr, child_pid_list **nth_node_ptr)
//      - Used to start a file back up that stopped running when it shouldn't have, indicated by the PID
//      - Sets the 'alive' portion of the struct to 0 to indicate that the process is no linger used
//      - Starts up a new process to run the same file that shut down:
//          - Sets the 'alive' bit to 1
//          - Transfers over the 'dir' and 'item_name' values from the dead process
//          - Assigns a new PID
// 
// IN:  child_pid_list **first_node_ptr, child_pid_list **nth_node_ptr
//      - Used to allow reading and manipulation of the linked list used to store data for all the child processes.
//      int32_t s_pid
//      - Indicates by PID which dead process needs to be replaced.
// 
// OUT: Void
//      - Currently not used.
// 
//  ... this will go in a .h
//===========================================================================================================================================

void restart_process(int32_t s_pid, child_pid_list **first_node_ptr, child_pid_list **nth_node_ptr)
{
    // set up linked list for this function
    child_pid_list *nth_node = *nth_node_ptr;
    child_pid_list *first_node = *first_node_ptr;

    // navigate to the problematic link
    nth_node = first_node;
    while( (nth_node->next != NULL) && (nth_node->child_pid != s_pid) ) 
    {
        nth_node = nth_node->next;
    }

    // fork a new process to start the module back up
    pid_t pid;  
    pid = fork();
    if (0 == pid) 
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
    else if (-1 == pid)
    {
        syslog(LOG_ERR, "failed to fork child process! (%d:%s)", errno, strerror(errno));
    } 
    else
    {
        // copy in new pid for the app/module
        nth_node->child_pid = pid;
        nth_node->alive = 1;

        // cycle to the end of the linked list to create the new entry
        printf("\nAll stored child processes:\n");
        nth_node = first_node;
        while( nth_node->next != NULL )
        {
            if(1 == nth_node->alive)
            {
                printf("%d\n", nth_node->child_pid);
            }
            nth_node = nth_node->next;   // After the second module goes out, this function loops infinitely until segmentation error.
        }
    }
}