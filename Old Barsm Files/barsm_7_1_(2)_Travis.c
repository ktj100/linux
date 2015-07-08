//=======================================
// GET
// ASSIMILATOR 
// BARSM
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

#define AACM_MAX_LAUNCH_ATTEMPTS    5
#define MAX_NUM_DIRECTORIES         3

// current directories to load apps ... more later?  
const char *dirs[] = 
{
    "/opt/rc360/aacm/",
    "/opt/rc360/modules/",
    "/opt/rc360/apps/"
};


// Used to count the number of PIDs started     TODO (change out with better method)
int p;
// Used to store all child pids                 TODO (change out with better method)
int child_pid[10];


// main ...
int main()
{
    // To change out later  TODO
    p = 0;

    int launch_status = 0;
    int dir_index = 0;
    // get number of elements in dirs ...
    int dirs_array_size = 0;
    while(dirs[++dirs_array_size]!='\0');

    // launch items in each dirs directory.  If directory empty, do nothing.  
    for (dir_index = 0; dir_index < dirs_array_size; dir_index++) 
    {
        launch_status = launch_item(dirs[dir_index]);
        if (1 == launch_status) 
        {
            printf("DIRECTORY %s IS EMPTY! NOTHING LAUNCHED! \n", dirs[dir_index]);
        }
        else
        {
            printf("ITEMS IN %s LAUNCHED! \n", dirs[dir_index] );
        }
    }

    sleep(20);
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
    int dir_index = 0;
    int child_pid_cnt = 0;  
	int return_val = 0; 
    int empty_dir = 1;
       
    DIR *dir;
    errno = 0;
    dir = opendir(directory);
   
    while ((dp = readdir(dir)) != NULL) 
    {
        // concatenates 'directory' string and 'd_name' string to be used for execl()
        char *concat = (char*) malloc(strlen(directory) + strlen(dp->d_name) + 1);
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
                else
                {
                    // CHILD PID LIST ... consider changing based on each launch_item() call ... 
                    pid_t* children = malloc( ( child_pid_cnt + 1 ) * sizeof(pid_t) );
                    children[ child_pid_cnt ] = getpid();
                    printf( "CHILD PID: %u \n" , children[ child_pid_cnt ] );
                    child_pid_cnt++;

                    // will be swapped with cleaner method later
                    child_pid[p] = pid;
                    p++;
                }
            }
            else if (-1 == pid)
            {
                /* failed to fork a child process */
                //syslog(LOG_ERR, "failed to fork child process! (%d:%s)", errno, strerror(errno));
                printf("failed to fork child process!: (%d:%s) \n", errno, strerror(errno));
            } 
        }

        sleep(1);
        // free(concat);
        // free(children);
    }
    closedir(dir);
    return empty_dir;
}



//===========================================================================================================================================
// FUNCTION: int check_modules()
//      -  
//      - 
// 
// IN:  
//      - 
// 
// OUT: int
//      - 
//      ** I've got some printf usage to help debug for now ... will be sent to syslog() as well
// 
//  ... this will go in a .h ????
//===========================================================================================================================================
int check_modules(*child_pids)
{
    int status;
    errno = 0;

    /* Waitpid checks for child zombies. This occurs when the exec'ed program by the child 
     * process ends (which it shouldn't until shutdown in this case). If a zombie is found,
     * the zombie process is removed so that the kill function will find it. */
    waitreturn = waitpid(-1, &status, WNOHANG);
    //printf("\nrc (status): %d\n", rc);  // debugging

    if (-1 == waitreturn) 
    {
        printf("ERROR: waitpid returned -1 error: (%d:%s) \n", errno, strerror(errno));
    }
    else if (0 < waitreturn)
    {
        printf("ERROR: Child process with PID %d has shut down.\n", waitreturn);
    }
    else if (0 == waitreturn)
    {
        /* No child zombies exist if watireturn == 0 */
        continue;
    }
    
    /* // Not sure if this WEXITSTATUS check is worth doing...
    if (0 != WEXITSTATUS(status)) 
    {    
        printf("command failed to execute correctly!: (%d) \n" , WEXITSTATUS(status));
    } */
    
    for (i = 0; i < p; i++)
    {
        check_process(child_pid[i]);
    }
}



//===========================================================================================================================================
// FUNCTION: int check_process(pid)
//      -  
//      - 
// 
// IN:  
//      - 
// 
// OUT: int
//      - 
//      ** I've got some printf usage to help debug for now ... will be sent to syslog() as well
// 
//  ... this will go in a .h ????
//===========================================================================================================================================
int check_process(pid)
{
    /* kill will set errno according to whether the process specified by the pid is still running. 
     * Even if the child program executed reached it's end, the process should show as running. */
    errno = 0;
    kill(pid, 0);

    if (3 == errno)
    {
        /* If errno = 3, the child process does not exist */
        printf("ERROR: Child process with PID: %d is failed!\n", pid);
        return(-1);
    }
    else if (0 == errno)
    {
        /* If errno = 0, the use of kill to check whether the AACM is still
         * running was successful */
        //printf("Child process is good!\n");
        return(0);
    }
    else if (10 == errno)
    {
        printf("All child processes are failed!");
        return(-2);
    }
}