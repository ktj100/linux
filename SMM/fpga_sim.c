#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <time.h>

#include "sensor.h"
#include "fpga_sim.h"

;

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

void fpga_sim_voltages(int *voltage)
{
    int32_t i, temp;

    rawfp = fopen("raw.in", "r");
    if (rawfp == NULL) 
    {
        printf("Can't open input file raw.in!\n");
    }
    for(i = 0; !feof(rawfp); i++)
    {
        temp = fscanf(rawfp, "%d", &voltage[i]);
        if(1 != temp)
        {
            printf("ERROR: Scan failed before reaching EOF\n");
            break;
        }
        else
        {
            //printf("Scanned Value: %d\n", voltage[i]);
        }
    }
    fclose(rawfp);
}

void fpga_sim_timestamps(int64_t *timestamps)
{
    int32_t i, temp;

    camfp = fopen("cam.in", "r");
    if (camfp == NULL) 
    {
        printf("Can't open input file cam.in!\n");
    }
    for(i = 0; !feof(camfp); i++)
    {
        temp = fscanf(camfp, "%ld", &timestamps[i]);
        if(temp != 1)
        {
            printf("ERROR: Scan failed before reaching EOF\n");
            break;
        }
        else
        {
            printf("Scanned Value: %ld\n", timestamps[i]);
        }
    }
    fclose(camfp);
}