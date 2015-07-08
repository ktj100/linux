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


// main ...
int main()
{
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
    int rc = 0;             // I don't like the name rc ... change  TODO
	struct dirent *dp;      // I don't like the name dp ... change  TODO
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
                }
            }
            else if (-1 == pid)
            {
                /* failed to fork a child process */
                //syslog(LOG_ERR, "failed to fork child process! (%d:%s)", errno, strerror(errno));
                printf("failed to fork child process!: (%d:%s) \n", errno, strerror(errno));
            } 


//          else    // DEFINE THIS USE BETTER ... this was from another program, is this how we want it?
//          {
//              /* Wait until the spawned process exits so we can determine if the
//              * command was executed successfully. */
//              errno = 0;
//              if (-1 == waitpid(pid, &rc, 0))
//              {
//                  //syslog(LOG_ERR, "error waiting for child process! (%d:%s)", errno, strerror(errno));
//                  printf("error waiting for child process!: (%d:%s) \n", errno, strerror(errno));
//              }
//              if (0 != WEXITSTATUS(rc))
//              {
//                  //syslog(LOG_ERR, "command failed to execute correctly! (%d)", WEXITSTATUS(rc));
//                  printf("command failed to execute correctly!: (%d) \n" , WEXITSTATUS(rc));
//              }
//          }


        }

        sleep(1);
        // free(concat);
        // free(children);
    }
    closedir(dir);
    return empty_dir;
}
