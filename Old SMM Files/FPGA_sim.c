#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <time.h>

void main(void)
{
    // time tracking structs
    struct timespec real_clock, start_time;

    // temporary variables
    int delay_finished = 0, i;
    int delta_time = 800000000;
    int num_timestamps;
    long int timestamps[9] = {0,0,0,0,0,0,0,0,0};

    // long term tracking variables
    time_t run_time_s = 0, run_time_ns = 0;

    // file declarations
    FILE *rawfp, *camfp, *sigfp;

    // using a clock to simulate the interrupt
    clock_gettime(CLOCK_REALTIME, &start_time);


    // set initial delta time value

    while(1)
    {

        // using a clock to simulate the interrupt
        for(delay_finished = 0; 1 != delay_finished;)
        {
            // read in the current time to see if it is a second multiple of the start time
            clock_gettime(CLOCK_REALTIME, &real_clock);
            if(start_time.tv_sec + run_time_s <= real_clock.tv_sec)
            {
                // exit the while loop and read in the values
                delay_finished = 1;
            }
        }

        //printf("1 \n");

        // calculate all timestamps for the next second, fill places over 1 second with 0's
        for(i = 0 ;run_time_ns < 1000000000; i++)
        {
            delta_time = delta_time + 0.005 * delta_time * (1 - delta_time / 58000000); 

            printf("Delta-Time: %d\n", delta_time);
 
            run_time_ns += delta_time;                       // (1 / 8.75 = 114285714)
            timestamps[i] = 1000000000L * run_time_s + run_time_ns;
        }
        num_timestamps = i;

        //printf("2 \n");

        // remove one second's worth of ns and add to s counter
        run_time_ns -= 1000000000;
        run_time_s++;

        //printf("3 \n");

        // write timestamps into file cam.in
        camfp = fopen("cam.in", "w");
        if (camfp == NULL) {
            printf("Can't open input file cam.in!\n");
        }

        //printf("4 \n");

        fprintf(camfp, "%ld", timestamps[0]);
        for(i = 1; i < num_timestamps; i++)
        {
            fprintf(camfp, "\n%ld", timestamps[i]);
        }
        fclose(camfp);

        //printf("5 \n");

        /*rawfp = fopen("raw.in", "w+");
        if (rawfp == NULL) 
        {
            printf("Can't open input file raw.in!\n");
        }*/

        sigfp = fopen("sig.in", "w+");
        if (sigfp == NULL)
        {
            printf("Can't open input file sig.in!\n");
        }
        fprintf(sigfp, "1");
        fclose(sigfp);

        printf("'1' written to file.\n");
    }
}