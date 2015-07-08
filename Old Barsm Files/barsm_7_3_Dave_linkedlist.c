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
    const char *dir;
    char *item_name;
    struct child_pid_list_struct *next;
};

// GLOBALS
typedef struct child_pid_list_struct child_pid_list; 
child_pid_list *first_node, *nth_node;

// FUNCTION DECLARATIONS
int launch_item( const char *directory );
void create_pid_linked_list();



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
        printf("FIRST NODE: %p \n" , first_node ); 
        printf("NEXT NODE: %p \n" , first_node->next );
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
    // all done, so last node is NULL
    nth_node->next = NULL;
    
    
    // print list ... similiar usage to traverse and check pids ...
    nth_node = first_node;
    while( nth_node->next != NULL)
    {
        printf( "PID: %d \n"    , nth_node->child_pid );
        printf( "DIR: %s \n"    , nth_node->dir );
        printf( "NAME: %s \n\n" , nth_node->item_name );
        nth_node = nth_node->next;
    }
    return 0;
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
                nth_node->dir = directory;
                nth_node->item_name = dp->d_name;

                if (nth_node->next != NULL)
                {
                    printf( "CHILD linked list 'pid': %d \n" , nth_node->child_pid );
                    printf( "CHILD linked list 'dir': %s \n" , nth_node->dir );
                    printf( "CHILD linked list 'name': %s \n" , nth_node->item_name );
                    printf( "this address %p \n" , nth_node );
                    printf( "next address %p \n" , nth_node->next ); 
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