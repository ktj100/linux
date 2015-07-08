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
    "/opt/rc360/aacm/",
    "/opt/rc360/modules/",
    "/opt/rc360/apps/"
};


// Going with a linked list.  Tried reallocating memory (iteratively) for each child pid.  But, this list
// will be traversed to determine the health (and existence) of each pid.  If one is dead, it'll need to be removed and recreated.  
// I think a linked list may be easier to deal with.  
struct child_pid_list_struct
{
    pid_t child_pid;
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
    // create linked list ...
    child_pid_list *first_node = (child_pid_list *)malloc(sizeof(child_pid_list));
    if (first_node != NULL)
    {
        printf("GOOD malloc \n");
        first_node->next = NULL;
        nth_node = first_node;
        printf("FIRST NODE: %u \n" , first_node ); 
        printf("NEXT NODE: %u \n" , first_node->next );
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
        printf("OPEN DIRECTORY: %s \n", dirs[dir_index] );
        //launch_status = launch_item(dirs[dir_index] , first_node, nth_node );
        launch_status = launch_item( dirs[dir_index] );
        if (1 == launch_status) 
        {
            printf("DIRECTORY %s IS EMPTY! NOTHING LAUNCHED! \n\n", dirs[dir_index]);
        }
        else
        {
            printf("ITEMS IN %s LAUNCHED! \n", dirs[dir_index] );
            printf("CLOSE DIRECTORY: %s \n\n", dirs[dir_index] );

        }
    }


    // get size of child_pid_list
    nth_node = first_node;
    while( nth_node->next != NULL)
    {
        printf( "PID #: %lu \n" , nth_node->child_pid );
        nth_node = nth_node->next;
    }

    // while check conditions child PIDS


    // monitor the processes
    int stopped_pid = 0;

    while(-1 != stopped_pid)
    {
        stopped_pid = check_modules();
        
        if(stopped_pid > 0)
        {
            // restart stopped child

            printf("Step 1\n");

            restart_process(stopped_pid, &first_node, &nth_node);
        }
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
        // concatenates 'directory' string and 'd_name' string to be used for execl()
        char *concat = (char*) malloc(strlen(directory) + strlen(dp->d_name) + 1);      //
        strcat( concat , directory );        
        strcat( concat , dp->d_name );
        if ( '.' != dp->d_name[0] ) 
        {
            empty_dir = 0;
            printf("Launching ... %s \n", concat);

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

                if (nth_node->next != NULL)
                {
                    printf( "CHILD linked list 'pid': %d \n" , nth_node->child_pid );
                    printf("this address %lu \n" , nth_node );
                    printf("next address %lu \n" , nth_node->next ); 
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
int check_modules(void)
{
    int rc, waitreturn;
    errno = 0;

    /* Waitpid checks for child zombies. This occurs when the exec'ed program by the child 
     * process ends (which it shouldn't until shutdown in this case). If a zombie is found,
     * the zombie process is removed so that the kill function will find it. */
    waitreturn = waitpid(-1, &rc, WNOHANG);
    //printf("\nrc (status): %d\n", rc);  // debugging

    if (-1 == waitreturn) 
    {
        printf("ERROR: waitpid returned -1 error: (%d:%s) \n", errno, strerror(errno));
        sleep(1);
    }
    else if (0 < waitreturn)
    {
        printf("ERROR: Child process with PID %d was a zombie: process eliminated.\n", waitreturn);
        return(waitreturn);
    }
    else if (0 == waitreturn)
    {
        /* No child zombies exist if watireturn == 0 */
    }
    
    /* // Not sure if this WEXITSTATUS check is worth doing...
    if (0 != WEXITSTATUS(status)) 
    {    
        printf("command failed to execute correctly!: (%d) \n" , WEXITSTATUS(status));
    } */
    
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

    printf("%lu\n", nth_node->next); // This line causes a segmentation error

    while( (nth_node->next != NULL) && (nth_node->child_pid != s_pid) ) 
    {
        nth_node = nth_node->next;
    }

    pid_t pid;  
    pid = fork();
    if (0 == pid) 
    {
        /* successfully childreated a child process, now start the required 
        * application */

        errno = 0;
        if ( (0 != execl(/*nth_node->INSERT_PATH*/ "/opt/rc360/aacm/aacm", /*nth_node->INSERT_NAME*/ "aacm", (char *)NULL)) )
        {
            //syslog(LOG_ERR, "failed to launch! (%d:%s)", errno, strerror(errno));
            printf("failed to launch! (%d:%s) \n", errno, strerror(errno));
            /* force the spawned process to exit */
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
        printf("Step 3\n");

        // cycle to the end of the list
        while( nth_node->next != NULL )
        {
            printf("PID: %d\n", nth_node->child_pid);
            printf("Contents of next address: %d\n", *nth_node->next);
            nth_node = nth_node->next;   // After the second module goes out, this function loops infinitely until segmentation error.
        }

        printf("Step 4\n");

        // add the new slot for the new process
        nth_node->next = (child_pid_list *)malloc(sizeof(child_pid_list));
        nth_node->child_pid = pid;
        // nth_node->next = NULL;

        printf("Memory allocated and new PID written.\n");
        printf("Contents of next address: %d\n", *nth_node->next);

        if ( nth_node->next != NULL )
        {
            printf( "CHILD linked list 'pid': %d \n" , nth_node->child_pid );
            printf("this address %lu \n" , *nth_node );
            printf("Contents of next address: %d (NULL)\n" , *nth_node->next ); 
        }
        else
        {
            printf("BAD malloc \n");
        }
        nth_node = nth_node->next;
    }
    sleep(1);
}
