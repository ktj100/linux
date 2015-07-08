#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char *argv[])
{
	printf("app 1 launched!\n");

	sleep(20);

	/* Sending the SIGCHLD message to the parent pid kills the parent 
	 * but not the child (should have killed the child?????) 

	printf("app 1 sending SIGCHLD to parent");

	int ppid = getppid();

	kill(ppid, 20);
	sleep(2); // make sure kill message got through
	printf("app 1 not ended");*/

	printf("app 1 ended\n");

	exit(0);
}
