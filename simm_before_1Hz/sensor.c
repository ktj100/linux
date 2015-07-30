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

int32_t tot_logicals = 0;
int32_t tot_stamps = 0;

int32_t subscribe_config(void)
{
    // GET PERIOD REQUIRED FOR PUBLISH MESSAGES FOR LOGICAL VALUES
    // (done in fpga_sim.h)

    // GET PERIOD REQUIRED FOR PUBLISH MESSAGES FOR TIMESTAMP VALUES
    // (done in fpga_sim.h)

    // ALLOCATE SPACE FOR THE STORAGE OF THE LOGICAL VALUES 
    // = period of collection time in seconds
    pfp_values = (int32_t*)malloc(data_period);
    ptlt_values = (int32_t*)malloc(data_period);
    ptrt_values = (int32_t*)malloc(data_period);
    tcmp_values = (int32_t*)malloc(data_period);
    cop_values = (int32_t*)malloc(data_period);

    // ALLOCATE SPACE FOR THE STORAGE OF THE TIMESTAMP VALUES 
    // = period of collection time in seconds * 9 values per second (max)
    cam_secs = (int32_t*)malloc(timestamps_period);
    cam_nsecs = (int32_t*)malloc(timestamps_period);

    return(0);
}

int32_t get_logicals(void)
{
    int32_t i;
    int32_t voltages[5] = {0,0,0,0,0};

    // READ IN 1 HZ VOLTAGE VALUES
    fpga_sim_voltages(&voltages[0]);

    pfp_values[tot_logicals] = convert_pfp(voltages[0]);
    ptlt_values[tot_logicals] = convert_ptxt(voltages[1]);
    ptrt_values[tot_logicals] = convert_ptxt(voltages[2]);
    tcmp_values[tot_logicals] = convert_tcmp(voltages[3]);
    cop_values[tot_logicals] = convert_cop(voltages[4]);

    // INCREMENT TOTAL TRACKER
    tot_logicals++;

    /* DEBUGGING */
    // print out all stored values
    if(3 < tot_logicals)
        i = tot_logicals - 3;
    else
        i = 0;
    for(; i < tot_logicals; i++)
    {
        if(i < tot_logicals)
        {
            printf("\nPFP %d: %d\n", i + 1, pfp_values[i]);
            printf("PTLT %d: %d\n", i + 1, ptlt_values[i]);
            printf("PTRT %d: %d\n", i + 1, ptrt_values[i]);
            printf("TCMP %d: %d\n", i + 1, tcmp_values[i]);
            printf("COP %d: %d\n", i + 1, cop_values[i]);
        }
    }

    // INCLUDE A RETURN VALUE TO SHOW THAT STORAGE IS FULL AND READY TO BE SHIPPED
    if(tot_logicals == data_period)
    {
        return(1);
    }
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
        cam_secs[tot_stamps + i] = timestamps[i] / 1000000000L;
        cam_nsecs[tot_stamps + i] = timestamps[i] - cam_secs[tot_stamps + i] * 1000000000L;
    }
    // INCREMENT TOTAL TRACKER
    tot_stamps += i;

    /* DEBUGGING */
    // print out all stored values
    if(9 < tot_stamps)
        i = tot_stamps - 9;
    else
        i = 0;
    for(; i < tot_stamps; i++)
    {
        printf("\nSecond Stamp %d: %d\n", i + 1, cam_secs[i]);
        printf("Nano Stamp   %d: %9d\n", i + 1, cam_nsecs[i]);
    }
    printf("\nTotal Logicals: %d\nTotal Timestamps: %d\n", tot_logicals, tot_stamps);
}

void clear_logicals(void)
{
    int32_t i;

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
    int32_t i;

    tot_stamps = 0;

    for(i = 0; i < timestamps_period; i++)
    {
        cam_secs[i] = 0;
        cam_nsecs[i] = 0;
    }
}