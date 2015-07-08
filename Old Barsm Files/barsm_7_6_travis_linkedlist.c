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
#define MAX_NUM_DIRECTORIES         3

// current directories to load apps ... more later?  
const char *dirs[] = 
{
    "/opt/rc360/system/",       // AACM
    "/opt/rc360/modules/GE/",   // GE MODULES
    "/opt/rc360/modules/TPA/",  // TP MODULES
    "/opt/rc360/apps/GE/",      // GE APPS
    "/opt/rc360/apps/TPA/"      // TP APPS
};


// Going with a linked list.  Tried reallocating memory (iteratively) for each child pid.  But, this list
// will be traversed to determine the health (and existence) of each pid.  If one is dead, it'll need to be removed and recreated.  
// I think a linked list may be easier to deal with.  
struct child_pid_list_struct
{
    pid_t child_pid;
    const char *dir;
    char *item_name;
    int8_t alive;
    struct child_pid_list_struct *next;
};

// GLOBALS
typedef struct child_pid_list_struct child_pid_list; 
child_pid_list *first_node, *nth_node;

// FUNCTION DECLARATIONS
//int launch_item( const char *directory , struct child_pid_list_struct *first_node_ptr, struct child_pid_list_struct *nth_node_ptr );
int launch_item( const char *directory );
void create_pid_linked_list();
void restart_process(int s_pid, child_pid_list **first_node_ptr, child_pid_list **nth_node_ptr);
int check_modules();



// main ...
int main()
{
    printf("\n\n");
    // create linked list ...
    child_pid_list *first_node = (child_pid_list *)malloc(sizeof(child_pid_list));
    if (first_node != NULL)
    {
        printf("GOOD malloc \n");
        first_node->next = NULL;
        nth_node = first_node;
        //printf("\nFIRST NODE: %p \n" , first_node ); 
        //printf("NEXT NODE: %p \n" , first_node->next );
    }
    else
    {
        printf("BAD malloc \n");
    }
    // end create linked list ... 

    int launch_status = 0;
    int dir_index = 0;

    // get number of elements in dirs ...
    int dirs_array_size = 0;
    while(dirs[++dirs_array_size]!='\0');

    // launch items in each dirs directory.  If directory empty, do nothing.  
    for (dir_index = 0; dir_index < dirs_array_size; dir_index++) 
    {
        //printf("\nOPEN DIRECTORY: %s \n", dirs[dir_index] );
        //launch_status = launch_item(dirs[dir_index] , first_node, nth_node );
        launch_status = launch_item( dirs[dir_index] );
        if (1 == launch_status) 
        {
            printf("DIRECTORY %s IS EMPTY! NOTHING LAUNCHED! \n\n", dirs[dir_index]);
        }
        else
        {
            printf("\nITEMS IN %s LAUNCHED! \n", dirs[dir_index] );
            printf("CLOSE DIRECTORY: %s \n\n", dirs[dir_index] );
        }
    }

    // get size of child_pid_list
    nth_node = first_node;

    // print list of all initially opened PIDs
    printf("\nAll open PIDs:\n");
    while( nth_node->next != NULL)
    {
        printf( "%d \n" , nth_node->child_pid );
        nth_node = nth_node->next;
    }

    printf("main 1.1 \n");

    while(1)
    {
        // check if any of the processes have gone zombie
        check_modules(&first_node, &nth_node);
        printf("main 1.2 \n");
        
        // restart any processes that have gone zombie or disappeared
        nth_node = first_node;
        while( nth_node->next != NULL)
        {
            printf("main 1.3 \n");
            if(-1 == nth_node->alive)
            {
                // restart stopped child
                restart_process(nth_node->child_pid, &first_node, &nth_node);
            }
            nth_node = nth_node->next;
        }
        sleep(1);
    }

    return(0);

}




//===========================================================================================================================================
// FUNCTION: int launch_item (const char *directory)
//      - Used to start the fork process of each item within a directory.  Currently we have three directories with 'n' items in each.  
//      - As for now, AACM will only have one ... not sure how many modules or apps will be used.   
// 
// IN:  const char* directory
//      - use one of of the current directories ... as of now, just aacm, modules, and apps.  
// 
// OUT: int
//      - will be used to check status after running
//      ** I've got some printf usage to help debug for now ... will be sent to syslog() as well
// 
//  ... this will go in a .h
//===========================================================================================================================================
//int launch_item( const char *directory  , struct child_pid_list_struct *first_node_ptr, struct child_pid_list_struct *nth_node_ptr )
int launch_item( const char *directory )
{
    int rc = 0;             // I don't like the name rc ... change  TODO
    struct dirent *dp;      // I don't like the name dp ... change  TODO
    int dir_index = 0;  
    int return_val = 0; 
    int empty_dir = 1;
    int k = 0;   

    DIR *dir;
    errno = 0;
    dir = opendir(directory);
    
    while ((dp = readdir(dir)) != NULL) 
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

            printf("\nLaunching ... %s \n", concat);

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
                    //syslog(LOG_ERR, "failed to launch! (%d:%s)", errno, strerror(errno));
                    printf("failed to launch! (%d:%s) \n", errno, strerror(errno));
                    /* force the spawned process to exit */
                    return_val = 5;
                    exit(-errno);
                }
            }
            else if (-1 == pid)
            {
                /* failed to fork a child process */
                //syslog(LOG_ERR, "failed to fork child process! (%d:%s)", errno, strerror(errno));
                printf("failed to fork child process!: (%d:%s) \n", errno, strerror(errno));
                
            } 
            else
            {
                nth_node->next = (child_pid_list *)malloc(sizeof(child_pid_list));
                nth_node->child_pid = pid;
                nth_node->dir = concat;
                nth_node->item_name = dp->d_name;
                nth_node->alive = 1;    // 1 indicates that the process is supposed to be running

                if (nth_node->next != NULL)
                {
                    printf( "\nCHILD linked list 'pid': %d \n" , nth_node->child_pid );
                    printf( "CHILD linked list 'dir': %s \n" , nth_node->dir );
                    printf( "CHILD linked list 'name': %s \n" , nth_node->item_name );
                    //printf( "this address %p \n" , nth_node );
                    //printf( "next address %p \n" , nth_node->next ); 
                }
                else
                {
                    printf("BAD malloc \n");
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
// FUNCTION: int launch_item (const char *directory)
//      - Used to start the fork process of each item within a directory.  Currently we have three directories with 'n' items in each.  
//      - As for now, AACM will only have one ... not sure how many modules or apps will be used.   
// 
// IN:  const char* directory
//      - use one of of the current directories ... as of now, just aacm, modules, and apps.  
// 
// OUT: int
//      - will be used to check status after running
//      ** I've got some printf usage to help debug for now ... will be sent to syslog() as well
// 
//  ... this will go in a .h
//===========================================================================================================================================
//int launch_item( const char *directory  , struct child_pid_list_struct *first_node_ptr, struct child_pid_list_struct *nth_node_ptr )
int check_modules(child_pid_list **first_node_ptr, child_pid_list **nth_node_ptr)
{
    int rc, waitreturn;
    errno = 0;

    // find struct in link with pid matching that of the stopped process
    child_pid_list *nth_node = *nth_node_ptr;
    child_pid_list *first_node = *first_node_ptr;

    nth_node = first_node;

    while (nth_node->next != NULL)
    {
        /* Waitpid checks for child zombies. This occurs when the exec'ed program by the child 
         * process ends (which it shouldn't until shutdown in this case). If a zombie is found,
         * the zombie process is removed so that the kill function will find it. */
        if (1 == nth_node->alive)
        {
            waitreturn = waitpid(nth_node->child_pid, &rc, WNOHANG);
            //printf("\nrc (status): %d\n", rc);  // debugging

            if (-1 == waitreturn) 
            {
                printf("\nERROR: Child process with PID %d is not existent. Restarting...\n", nth_node->child_pid);
                nth_node->alive = -1;   // -1 indicates a need to be replaced
            }
            else if (0 < waitreturn)
            {
                printf("\nERROR: Child process with PID %d is a zombie. Restarting...\n", waitreturn);
                nth_node->alive = -1;   // -1 indicates a need to be replaced
            }
        }
        nth_node = nth_node->next;
    }
    return(0);
}




//===========================================================================================================================================
// FUNCTION: int launch_item (const char *directory)
//      - Used to start the fork process of each item within a directory.  Currently we have three directories with 'n' items in each.  
//      - As for now, AACM will only have one ... not sure how many modules or apps will be used.   
// 
// IN:  const char* directory
//      - use one of of the current directories ... as of now, just aacm, modules, and apps.  
// 
// OUT: int
//      - will be used to check status after running
//      ** I've got some printf usage to help debug for now ... will be sent to syslog() as well
// 
//  ... this will go in a .h
//===========================================================================================================================================
//int launch_item( const char *directory  , struct child_pid_list_struct *first_node_ptr, struct child_pid_list_struct *nth_node_ptr )
void restart_process(int s_pid, child_pid_list **first_node_ptr, child_pid_list **nth_node_ptr)
{
    // find struct in link with pid matching that of the stopped process
    child_pid_list *nth_node = *nth_node_ptr;
    child_pid_list *first_node = *first_node_ptr;

    nth_node = first_node;

    printf("\nRelaunching process...");

    // navigate to the problematic link
    while( (nth_node->next != NULL) && (nth_node->child_pid != s_pid) ) 
    {
        nth_node = nth_node->next;
    }

    //nth_node->alive = 0;    // 0 indicates that the module is being shut down and replaced (or already has been)

    /*size_t len1 = strlen(nth_node->dir);
    char *temp_dir = (char*) malloc(len1 + 1);
    size_t len2 = strlen(nth_node->item_name);
    char *temp_name = (char*) malloc(len2 + 1);

    memcpy(temp_dir, nth_node->dir, len1);
    memcpy(temp_name, nth_node->item_name, len2);*/

    pid_t pid;  
    pid = fork();
    if (0 == pid) 
    {
        /* successfully childreated a child process, now start the required 
         * application */

        printf( "\nPID to replace: %d \n"  , nth_node->child_pid );
        printf( "DIR to launch: %s \n"    , nth_node->dir );
        printf( "NAME to launch: %s \n\n" , nth_node->item_name );

        errno = 0;
        if ( (0 != execl(nth_node->dir, nth_node->item_name, (char *)NULL)) )
        {
            //syslog(LOG_ERR, "failed to launch! (%d:%s)", errno, strerror(errno));
            printf("\nERROR: Launch failed! (%d:%s) \n", errno, strerror(errno));
            /* force the spawned process to exit */
            exit(-errno);
        }
    }
    else if (-1 == pid)
    {
        /* failed to fork a child process */
        //syslog(LOG_ERR, "failed to fork child process! (%d:%s)", errno, strerror(errno));
        printf("\nERROR: Fork failed! (%d:%s) \n", errno, strerror(errno));
        
    } 
    else
    {
        //printf("Step 3\n");

        // copy over information for new linked list entry
        //temp_dir = nth_node->dir;
        //temp_name = nth_node->item_name;

        nth_node->child_pid = pid;
        nth_node->alive = 1;

        // cycle to the end of the list
        printf("\nAll stored child processes:\n");
        while( nth_node->next != NULL )
        {
            if(1 == nth_node->alive)
            printf("%d\n", nth_node->child_pid);
            //printf("Contents of next address: %p\n", nth_node->next);
            nth_node = nth_node->next;   // After the second module goes out, this function loops infinitely until segmentation error.
        }

        //printf("Step 4\n");

        // add the new slot for the new process
        /*nth_node->next = (child_pid_list *)malloc(sizeof(child_pid_list));
        nth_node->child_pid = pid;
        nth_node->dir = temp_dir;
        nth_node->item_name = temp_name;
        nth_node->alive = 1;*/
        // nth_node->next = NULL;

        /*if ( nth_node->next != NULL )
        {
            printf("\nMemory allocated and new process linked.\n");
            printf( "New PID: %d \n" , nth_node->child_pid );

            // debugging:
            //printf("Contents of next address: %p\n", nth_node->next);
            //printf( "this address %p \n" , nth_node );
            //printf( "Contents of next address: %p (NULL)\n" , nth_node->next ); 
        }
        else
        {
            printf( "BAD malloc \n" );
        }
        nth_node = nth_node->next;*/
    }
    //sleep(1);
    printf("\nrestart 10 \n");
}