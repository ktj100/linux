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

int32_t tot_logicals[5] = {0,0,0,0,0};
int32_t tot_stamps[2] = {0,0};
int32_t ts_reads[2] = {0,0};

int32_t subscribe_config(void)
{
    // GET PERIOD REQUIRED FOR PUBLISH MESSAGES FOR LOGICAL VALUES
    // (done in fpga_sim.h)

    // GET PERIOD REQUIRED FOR PUBLISH MESSAGES FOR TIMESTAMP VALUES
    // (done in fpga_sim.h)

    // ALLOCATE SPACE FOR THE STORAGE OF THE LOGICAL VALUES 
    // = period of collection time in seconds
    pfp_values = (int32_t*)malloc(data_period[0] * sizeof(int32_t));
    ptlt_values = (int32_t*)malloc(data_period[1] * sizeof(int32_t));
    ptrt_values = (int32_t*)malloc(data_period[2] * sizeof(int32_t));
    tcmp_values = (int32_t*)malloc(data_period[3] * sizeof(int32_t));
    cop_values = (int32_t*)malloc(data_period[4] * sizeof(int32_t));

    // ALLOCATE SPACE FOR THE STORAGE OF THE TIMESTAMP VALUES
    // = period of collection time in seconds * 9 values per second (max)
    cam_secs = (int32_t*)malloc(timestamps_period[0] * sizeof(int32_t));
    cam_nsecs = (int32_t*)malloc(timestamps_period[1] * sizeof(int32_t));

    return(0);
}

int32_t get_logicals(int32_t *reset)
{
    int32_t i;
    int32_t voltages[5] = {0,0,0,0,0};

    // READ IN 1 HZ VOLTAGE VALUES
    fpga_sim_voltages(&voltages[0]);

    pfp_values[tot_logicals[0]] = convert_pfp(voltages[0]);
    ptlt_values[tot_logicals[1]] = convert_ptxt(voltages[1]);
    ptrt_values[tot_logicals[2]] = convert_ptxt(voltages[2]);
    tcmp_values[tot_logicals[3]] = convert_tcmp(voltages[3]);
    cop_values[tot_logicals[4]] = convert_cop(voltages[4]);

    // INCREMENT TOTAL TRACKER
    for (i = 0; i < 5; i++)
        tot_logicals[i]++;

    /* DEBUGGING */
    // print out the last three stored values
    for (i = 2; i >= 0; i--)
    {
        printf("\nPFP %d: %d\n", tot_logicals[0]-i, pfp_values[tot_logicals[0]-i-1]);
        printf("PTLT %d: %d\n", tot_logicals[1]-i, ptlt_values[tot_logicals[1]-i-1]);
        printf("PTRT %d: %d\n", tot_logicals[2]-i, ptrt_values[tot_logicals[2]-i-1]);
        printf("TCMP %d: %d\n", tot_logicals[3]-i, tcmp_values[tot_logicals[3]-i-1]);
        printf("COP %d: %d\n", tot_logicals[4]-i, cop_values[tot_logicals[4]-i-1]);
    }


    // if(3 < tot_logicals)
    //     i = tot_logicals - 3;
    // else
    //     i = 0;
    // for(; i < tot_logicals; i++)
    // {
    //     if(i < tot_logicals)
    //     {
    //         printf("\nPFP %d: %d\n", i + 1, pfp_values[i]);
    //         printf("PTLT %d: %d\n", i + 1, ptlt_values[i]);
    //         printf("PTRT %d: %d\n", i + 1, ptrt_values[i]);
    //         printf("TCMP %d: %d\n", i + 1, tcmp_values[i]);
    //         printf("COP %d: %d\n", i + 1, cop_values[i]);
    //     }
    // }

    // HERE'S THE SUBSCRIBE DATA
//  printf("FROM SENSOR num_mps: %d\n", num_mps);
//  for (i = 0 ; i < num_mps ; i++ )
//  {
//      printf("FROM SENSOR subscribeMP[i].mp: %d\n",       subscribeMP[i].mp);
//      printf("FROM SENSOR subscribeMP[i].period: %d\n",   subscribeMP[i].period);
//  }

    // INCLUDE A RETURN VALUE TO SHOW THAT STORAGE IS FULL AND READY TO BE SHIPPED
    for (i = 0; i < 5; i++)
    {
        if(tot_logicals[i] == data_period[i])
        {
            reset[i] = 1;
        }
    }
    return(0);
}

int32_t get_timestamps(int32_t *reset)
{
    int32_t i;
    int64_t timestamps[9] = {0,0,0,0,0,0,0,0,0};

    // READ IN TIMESTAMPS
    fpga_sim_timestamps(&timestamps[0]);

    // STORE TIMESTAMPS SEPARATELY AS SECONDS AND NANOSECONDS
    split_timestamps(&timestamps[0]);

    // CHECK IF RESET VALUE NEEDS TO BE SET
    for (i = 0; i < 2; i++)
    {
        if(ts_reads[i] == timestamps_period[i])
        {
            reset[i+5] = 1;
        }
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

void split_timestamps(int64_t *timestamps)
{
    int32_t i;
    for(i = 0; (i < 9) && (timestamps[i] != 0); i++)
    {
        // these formulas will be replaced by the actual formulas
        // they currently use the tens place for the seconds and the ones for the nsecs.
        cam_secs[tot_stamps[0] + i] = timestamps[i] / 1000000000L;
        cam_nsecs[tot_stamps[1] + i] = (timestamps[i] - cam_secs[tot_stamps[0] + i] * 1000000000L) * 10;
        printf("Nano Stamps: %d\n", cam_nsecs[tot_stamps[1] + i]);
    }
    // INCREMENT TOTAL TRACKER
    tot_stamps[0] += i;
    tot_stamps[1] += i;
    ts_reads[0] += 9;
    ts_reads[1] += 9;

    /* DEBUGGING */
    // print out all stored values
    if(20 < tot_stamps[0])
        i = tot_stamps[0] - 20;
    else
        i = 0;
    for(; i < tot_stamps[0]; i++)
    {
        printf("Second Stamp %d: %d\n", i + 1, cam_secs[i]);
    }
    if(20 < tot_stamps[1])
        i = tot_stamps[1] - 20;
    else
        i = 0;
    for(; i < tot_stamps[1]; i++)
    {
        printf("Nano Stamp   %d: %9d\n", i + 1, cam_nsecs[i]);
    }
    printf("\nTotal Logicals: %d, %d, %d, %d, %d\n", tot_logicals[0], tot_logicals[1], tot_logicals[2], tot_logicals[3], tot_logicals[4]);
    printf("Total Timestamps: %d, %d\n", tot_stamps[0], tot_stamps[1]);
}

void clear_logicals(int32_t mp)
{
    int32_t i;

    tot_logicals[mp] = 0;
    
    for(i = 0; i < data_period[mp]; i++)
    {
        if ( 0 == mp )
            pfp_values[i] = 0;
        if ( 1 == mp )
            ptlt_values[i] = 0;
        if ( 2 == mp )
            ptrt_values[i] = 0;
        if ( 3 == mp )
            tcmp_values[i] = 0;
        if ( 4 == mp )
            cop_values[i] = 0;
    }
}

void clear_timestamps(int32_t mp)
{
    int32_t i;

    tot_stamps[mp-5] = 0;
    ts_reads[mp-5] = 0;
    
    for(i = 0; i < timestamps_period[mp-5]; i++)
    {
        if ( 5 == mp )
        {
            cam_secs[i] = 0;
            printf("Cam_secs reset!");
        }
        if ( 6 == mp )
        {
            cam_nsecs[i] = 0;       
            printf("Cam_nsecs reset!");
        }
    }
}