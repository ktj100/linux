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

/*int32_t tot_logicals = 0;*/
int32_t tot_stamps = 0;

int32_t get_logicals(void)
{
    //int32_t i;
    int32_t voltages[5] = {0,0,0,0,0};

    // READ IN 1 HZ VOLTAGE VALUES
    fpga_sim_voltages(&voltages[0]);

    // SAVE VOLTAGE VALUES TO VARIABLES
    pfp_val = convert_pfp(voltages[0]);
    ptlt_val = convert_ptxt(voltages[1]);
    ptrt_val = convert_ptxt(voltages[2]);
    tcmp_val = convert_tcmp(voltages[3]);
    cop_val = convert_cop(voltages[4]);

    printf("\nPFP: %d\n", pfp_val);
    printf("PTLT: %d\n", ptlt_val);
    printf("PTRT: %d\n", ptrt_val);
    printf("TCMP: %d\n", tcmp_val);
    printf("COP: %d\n", cop_val);

    return(0);
}

int32_t get_timestamps(void)
{
    int64_t timestamps[9] = {0,0,0,0,0,0,0,0,0};

    // READ IN TIMESTAMPS
    fpga_sim_timestamps(&timestamps[0]);

    // STORE TIMESTAMPS SEPARATELY AS SECONDS AND NANOSECONDS
    split_timestamps(&timestamps[0]);

    return(0);
}

int32_t convert_pfp (int32_t voltage)
{
    int32_t pressure;

    /* This algorithm is only for testing. Actual algorithm will replace it. */
    pressure = voltage / 20;

    return (pressure);
}

int32_t convert_ptxt (int32_t voltage)
{
    int32_t temp;

    /* This algorithm is only for testing. Actual algorithm will replace it. */
    temp = voltage / 20;

    return (temp);
}

int32_t convert_tcmp (int32_t voltage)
{
    int32_t temp;

    /* This algorithm is only for testing. Actual algorithm will replace it. */
    temp = voltage / 200;

    return (temp);
}

int32_t convert_cop (int32_t voltage)
{
    int32_t pressure;

    /* This algorithm is only for testing. Actual algorithm will replace it. */
    pressure = voltage / 500 - 5;

    return (pressure);
}

void split_timestamps(int64_t *timestamps)
{
    int32_t i;

    for(i = 0; (i < 9) && (timestamps[i] != 0); i++)
    {
        // these formulas will be replaced by the actual formulas
        // they currently use the tens place for the seconds and the ones for the nsecs.
        cam_secs[i] = timestamps[i] / 1000000000L;
        cam_nsecs[i] = timestamps[i] - cam_secs[i] * 1000000000L;
    }
    // INCREMENT TOTAL TRACKER
    tot_stamps = i;

    /* DEBUGGING */
    // print out all stored values
    for(i = 0; i < tot_stamps; i++)
    {
        printf("\nSecond Stamp %d: %d\n", i + 1, cam_secs[i]);
        printf("Nano Stamp   %d: %9d\n", i + 1, cam_nsecs[i]);
    }
}