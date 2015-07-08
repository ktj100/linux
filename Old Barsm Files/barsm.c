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

sig_atomic_t child_exit_status;

void clean_up_child_process (int signal_number)
{
    /* Clean up the child process. */
    int status;
    wait(&status);
    /* Store its exit status in a global variable. */
    child_exit_status = status;
    printf("child cleaned up, status: %d\n", status);
}

// main ...
int main()
{
    /* Handle SIGCHLD by calling clean_up_child_process. */
    struct sigaction sigchld_action;
    memset (&sigchld_action, 0, sizeof (sigchld_action));
    sigchld_action.sa_handler = &clean_up_child_process;
    sigaction (SIGCHLD, &sigchld_action, NULL);

    int waitreturn;

    int launch_status = 0;
    launch_status = launch_item( dirs[0] );
    // check conditions ...

    launch_status = launch_item( dirs[1] );
    // check conditions ...

    launch_status = launch_item( dirs[2] );
    // check conditions ...

    /* Check whether main is parent or child */
    printf("Main PID: %d\n", getpid());

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
            //printf("error waiting for child process!: (%d:%s) \n", errno, strerror(errno));
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
        //n++;
        //sleep(0.25);
    }

    // keep the program from ending before the child processes
    //sleep(21);
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

    // Setup for sigtimedwait
    sigset_t mask;
    sigset_t orig_mask;
    struct timespec timeout;
    pid_t pid;
 
    sigemptyset (&mask);
    sigaddset (&mask, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
        printf("sigprocmask");
    }

    timeout.tv_sec = 5;
    timeout.tv_nsec = 0;

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

                //printf("Child pid: %d\n", getpid());

                errno = 0;
                if (0 != execl( concat , dp->d_name, (char *) NULL))       // not liking the 'const char' and 'char' definitions ... no errors, just warnings.
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
                /* Wait until the spawned process exits so we can determine if the 
                * command was executed successfully. */

                printf("Parent pid: %d\n", getpid());
                printf("Child pid: %d\n", pid);

                /*This section does not belong because it stops the parent process while the child is running 

                errno = 0;
                if (-1 == waitpid(pid, &rc, 0)) 
                {
                    //syslog(LOG_ERR, "error waiting for child process! (%d:%s)", errno, strerror(errno));
                    printf("error waiting for child process!: (%d:%s) \n", errno, strerror(errno));
                }
                if (0 != WEXITSTATUS(rc)) 
                {
                    //syslog(LOG_ERR, "command failed to execute correctly! (%d)", WEXITSTATUS(rc));     
                    printf("command failed to execute correctly!: (%d) \n" , WEXITSTATUS(rc));
                }*/

                /* kill will set errno according to whether the process specified by the pid is still running. 
                 * Even if the child program executed reached it's end, the process should show as running. 
                errno = 0;
                kill(pid, 0);
                printf("Kill pid: %d\n", pid);

                if (3 == errno)
                {
                    /* If errno = 3, the AACM process has stopped running 
                    printf("Child process is failed!\n");
                }
                else if (0 == errno)
                {
                    /* If errno = 0, the use of kill to check whether the AACM is still
                     * running was successful 
                    printf("Child process is good!\n");
                }
                else
                {
                    printf("Something is wrong, this line should not be executed! ERRNO: %d\n", errno);
                }*/

                /* This code will loop through, checking the life of the child process for 30 seconds. */
                //sleep(5); // To wait for child process to start up
                /*int n = 0;
                while (n <= 5)
                {
                    printf("checking process...\n");
                    if (sigtimedwait(&mask, NULL, &timeout) < 0) {
                        if (errno == EINTR) {
                            /* Interrupted by a signal other than SIGCHLD. 
                            n++;
                        }
                        else if (errno == EAGAIN) {
                            printf ("Timeout, killing child\n");
                            errno = 0;
                            kill (pid, 20);
                            printf("ERRNO for kill: %d\n", errno);
                            sleep(2); // Make sure that the kill function finishes before moving on
                            n = 6;
                        }
                        else {
                            printf("sigtimedwait");
                        }
                    }
                }*/

                /* kill will set errno according to whether the process specified by the pid is still running. 
                 * Even if the child program executed reached it's end, the process should show as running. */
                /*errno = 0;
                kill(pid, 0);
                printf("Kill pid: %d\n", pid);

                if (3 == errno)
                {
                    /* If errno = 3, the AACM process has stopped running 
                    printf("Child process is failed!\n");
                }
                else if (0 == errno)
                {
                    /* If errno = 0, the use of kill to check whether the AACM is still
                     * running was successful 
                    printf("Child process is good!\n");
                }
                else
                {
                    printf("Something is wrong, this line should not be executed! ERRNO: %d\n", errno);
                }*/

            } 
        }
        // free(concat);
    }
    closedir(dir);
    globalpid = pid;
}
