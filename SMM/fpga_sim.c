#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "sensor.h"
#include "fpga_sim.h"

// File sigfp will only contain a single value, 1 or 0. If it is a 1, 
// then the fpga has just finished updating the values in the cam.in and 
// raw.in files.
int32_t wait_for_fpga(void)
{
    int32_t delay_finished;

    // CHECK IF BIT IS A '1'
    sigfp = fopen("sig.in", "r");
    if (sigfp == NULL)
    {
        printf("Can't open input file sig.in!\n");
    }
    fscanf(sigfp, "%d", &delay_finished);
    fclose(sigfp);

    // RESET BIT TO '0' WHEN IT BECOMES A '1'
    if(1 == delay_finished)
    {
        sigfp = fopen("sig.in", "w");
        if (sigfp == NULL) 
        {
            printf("Can't open input file sig.in!\n");
        }
        fprintf(sigfp, "0");
   	    fclose(sigfp);
    }

    return(delay_finished);
}

// 