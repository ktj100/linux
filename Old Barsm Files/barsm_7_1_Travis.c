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
#include <signal.h>

#define MAX_DIR_LENGTH	3


// current directories to load apps ... more later?  
const char *dirs[] = 
{
    "/opt/rc360/aacm/",
    "/opt/rc360/modules/",
    "/opt/rc360/apps/"
};

int globalpid;

// main ...
int main()
{
    printf("Parent pid: %d\n", getpid());

    int waitreturn;

    int launch_status = 0;
    launch_status = launch_item( dirs[0] );
    // check conditions ...

    launch_status = launch_item( dirs[1] );
    // check conditions ...

    launch_status = launch_item( dirs[2] );
    // check conditions ...

    /* check for ended child processes */
    int n = 0;
    int rc;
    while(n < 7)
    {
        errno = 0;
        waitreturn = waitpid(-1, &rc, WNOHANG);
        //printf("\nrc (status): %d\n", rc);

        if (-1 == waitreturn) 
        {
            //syslog(LOG_ERR, "error waiting for child process! (%d:%s)", errno, strerror(errno));
            printf("error waiting for child process!: (%d:%s) \n", errno, strerror(errno));
        }
        else if (0 < waitreturn)
        {
            printf("pid of child whose state has changed: %d\n", waitreturn);
            n++;
        }
        else if (0 == waitreturn)
        {
            //printf("processes are still running\n");
        }
        
        if (0 != WEXITSTATUS(rc)) 
        {
            //syslog(LOG_ERR, "command failed to execute correctly! (%d)", WEXITSTATUS(rc));     
            printf("command failed to execute correctly!: (%d) \n" , WEXITSTATUS(rc));
        }
    }
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
    int rc = 0;
	struct dirent *dp;
    int dir_index = 0;

	DIR *dir;
    errno = 0;
    dir = opendir(directory);

    while ((dp = readdir(dir)) != NULL) 
    {
        // sleep allows a break between app prints for debugging purposes
        sleep(1);
        // concatenates 'directory' string and 'd_name' string to be used for execl()
        char *concat = (char*) malloc(strlen(directory) + strlen(dp->d_name) + 1);
        strcat( concat , directory );        
        strcat( concat , dp->d_name );
        if ( '.' != dp->d_name[0] ) 
        {
            printf("\nLaunching ... %s \n", concat);

            // start forking
            pid_t pid;	
            pid = fork();

            if (0 == pid)   // if == is <, then I get an "error waiting for child process" 
            {
                /* successfully childreated a child process, now start the required 
                * application */
                errno = 0;
                if (0 != execl( concat , dp->d_name, (char *) NULL))       // not liking the 'const char' and 'char' definitions ... no errors, just warnings.
                {
                    printf("failed to launch! (%d:%s) \n", errno, strerror(errno));
                    /* force the spawned process to exit */
                    exit(-errno);
                }
            } 
            else if (-1 == pid) 
            {
                /* failed to fork a child process */
                printf("failed to fork child process!: (%d:%s) \n", errno, strerror(errno));
            } 
            else 
            {
                printf("Parent pid: %d\n", getpid());
                printf("Child pid: %d\n", pid);
            } 
        }
    }
    closedir(dir);
}
