#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>

int
main(int argc, char *argv[])
{
   // Copied over from fork program
    int32_t rc;
    pid_t pid;

    char aacm_dir[50] = "/home/travis/sandbox/aacm/aacm_dummy";

    // launch AACM

    pid = fork();

    int success = execl("/home/travis/sandbox/aacm/aacm_dummy", "aacm_dummy", 0, 0, NULL);

    if (0 != success) {
        printf("Error: failed to exec AACM\nERRNO: %d\n", errno);

        /* force the spawned process to exit */
        exit(-errno);
    }
    return(0);
}