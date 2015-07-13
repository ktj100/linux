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

int32_t get_logicals(void)
{


    // INCLUDE A RETURN VALUE TO SHOW THAT STORAGE IS FULL AND READY TO BE SHIPPED
    if()
    {
        return();
    }
    return(0);
}

int32_t get_timestamps(void)
{


    // INCLUDE A RETURN VALUE TO SHOW THAT STORAGE IS FULL AND READY TO BE SHIPPED
    if()
    {
        return();
    }
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

void split_timestamps(int64_t *timestamp, int32_t *cam_secs, int32_t *cam_nsecs, int32_t total)
{
    int32_t i;
    for(i = 0; i < 9; i++)
    {
        // these formulas will be replaced by the actual formulas
        // they currently use the tens place for the seconds and the ones for the nsecs.
        cam_secs[total + i] = timestamp[i] / 1000000000L;
        cam_nsecs[total + i] = timestamp[i] - cam_secs[total + i] * 1000000000L;
    }
}

void clear_logicals(void)
{
    tot_logicals = 0;
    
    for(i = 0; i < data_period; i++)
    {
        pfp_values[i] = 0;
        ptlt_values[i] = 0;
        ptrt_values[i] = 0;
        tcmp_values[i] = 0;
        cop_values[i] = 0;
    }
}

void clear_timestamps(void)
{
    tot_stamps = 0;

    for(i = 0; i < timestamp_values; i++)
    {
        cam_secs[i] = 0;
        cam_nsecs[i] = 0;
    }
}