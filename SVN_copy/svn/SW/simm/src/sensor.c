#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#include "sensor.h"
#include "fpga_sim.h"

#define DEBUG_PRINT 9
#define CLOCK_OFFSET_TOLERANCE 2    // in seconds
#define MAX_DATA_PERIOD 60          // in seconds

;

int32_t tot_logicals[5] = {0,0,0,0,0};
int32_t tot_stamps[2] = {0,0};
int64_t last_ts = 0;
int64_t rollover = 0;
int64_t offset = 0;

/* This function allocates storage space for the data values that will be read in from the fpga.
 * Each array of values will store the last MAX_DATA_PERIOD worth of data, overwriting each value 
 * as new values are read in. Index values will keep track of the location of the newest value
 * read in from the FPGA. */
int32_t subscribe_config(void)
{
    // ALLOCATE SPACE FOR THE STORAGE OF THE LOGICAL VALUES
    // = maximum period of collection time in seconds
    pfp_values = (int32_t*)malloc(MAX_DATA_PERIOD * sizeof(int32_t));
    ptlt_values = (int32_t*)malloc(MAX_DATA_PERIOD * sizeof(int32_t));
    ptrt_values = (int32_t*)malloc(MAX_DATA_PERIOD * sizeof(int32_t));
    tcmp_values = (int32_t*)malloc(MAX_DATA_PERIOD * sizeof(int32_t));
    cop_values = (int32_t*)malloc(MAX_DATA_PERIOD * sizeof(int32_t));
    logical_index = MAX_DATA_PERIOD - 1;

    // ALLOCATE SPACE FOR THE STORAGE OF THE TIMESTAMP VALUES
    // = maximum period of collection time in seconds * 9 values per second
    cam_secs = (int32_t*)malloc(MAX_DATA_PERIOD * 9 * sizeof(int32_t));
    cam_nsecs = (int32_t*)malloc(MAX_DATA_PERIOD * 9 * sizeof(int32_t));
    timestamp_index = MAX_DATA_PERIOD * 9 - 1;

    return(0);
}

/* This function calculates the offset to be added to the cam timestamps, so that a true time is
 * stored, rather than a time realtive to startup. */
void clock_config(void)
{
    struct timespec real_time;
    struct timespec time_from_launch;

    int64_t full_real_time;
    int64_t full_time_from_launch;

    clock_gettime(CLOCK_REALTIME, &real_time);
    clock_gettime(CLOCK_MONOTONIC, &time_from_launch);

    // int32_t result;
    // result = (full_real_time = ((real_time.tv_sec << 32) | real_time.tv_nsec));
    // printf("Success = %d\n", result);
    // result = (full_time_from_launch = ((time_from_launch.tv_sec << 32) | time_from_launch.tv_nsec));
    // printf("Success = %d\n", result);

    // int32_t temp;
    // full_real_time = real_time.tv_sec << 32;
    // temp = real_time.tv_nsec;
    // printf("Original: %lu\nCopied: %d\n", real_time.tv_nsec, temp);
    // full_real_time = full_real_time & temp;
    // printf("Not shifted: %lu\nShifted: %lu\n", real_time.tv_sec, full_real_time);

    full_real_time = ((real_time.tv_sec * 1000000000L) + real_time.tv_nsec);
    full_time_from_launch = ((time_from_launch.tv_sec * 1000000000L) + time_from_launch.tv_nsec);

    // // DEBUGGING
    // printf("Clock Seconds: %lu\nClock NSeconds: %lu\n", real_time.tv_sec, real_time.tv_nsec);
    // printf("Launch Seconds: %lu\nLaunch NSeconds: %lu\n", time_from_launch.tv_sec, time_from_launch.tv_nsec);
    // printf("Clock Time: %lu\nStartup Time: %lu\n", full_real_time, full_time_from_launch);

    offset = full_real_time - full_time_from_launch;
}

int32_t get_logicals(int32_t *reset)
{
    int32_t i;
    int32_t voltages[5] = {0,0,0,0,0};

    // READ IN 1 HZ VOLTAGE VALUES
    fpga_sim_voltages(&voltages[0]);

    // MUTEX LOCKING NEEDED!! ----------------------------------------------------------------------------------------------------------------------------------------------------
    if ( 59 == logical_index )
        logical_index = 0;
    else
        logical_index++;

    pfp_values[logical_index] = convert_pfp(voltages[0]);
    ptlt_values[logical_index] = convert_ptxt(voltages[1]);
    ptrt_values[logical_index] = convert_ptxt(voltages[2]);
    tcmp_values[logical_index] = convert_tcmp(voltages[3]);
    cop_values[logical_index] = convert_cop(voltages[4]);

    // // DEBUGGING
    // pfp_values[tot_logicals[0]] = voltages[0];
    // ptlt_values[tot_logicals[1]] = voltages[1];
    // ptrt_values[tot_logicals[2]] = voltages[2];
    // tcmp_values[tot_logicals[3]] = voltages[3];
    // cop_values[tot_logicals[4]] = voltages[4];

    // NO LONGER NEEDED WHEN SWITCHING TO CONTINUOUS BUFFER
    // // INCREMENT TOTAL TRACKER
    // for (i = 0; i < 5; i++)
    //     tot_logicals[i]++;

    /* DEBUGGING */
    // print out the last DEBUG_PRINT stored values
    // this chunk of code results in many VALGRIND errors !!!!
    for (i = DEBUG_PRINT; i >= 0; i--)
    {
        if ( logical_index - i < 0 )
            j = logical_index - i + 60;
        else
            j = logical_index - i;

        printf("\nPFP %d: %d\n", j+1, pfp_values[j]);
        printf("PTLT %d: %d\n", j+1, ptlt_values[j]);
        printf("PTRT %d: %d\n", j+1, ptrt_values[j]);
        printf("TCMP %d: %d\n", j+1, tcmp_values[j]);
        printf("COP %d: %d\n", j+1, cop_values[j]);
    }

    // HERE'S THE SUBSCRIBE DATA
//  printf("FROM SENSOR num_mps: %d\n", num_mps);
//  for (i = 0 ; i < num_mps ; i++ )
//  {
//      printf("FROM SENSOR subscribeMP[i].mp: %d\n",       subscribeMP[i].mp);
//      printf("FROM SENSOR subscribeMP[i].period: %d\n",   subscribeMP[i].period);
//  }

    // NO LONGER NEED TO RESET DATA TO ZERO, BECAUSE DATA IS OVERWRITTEN
    // // INCLUDE A RETURN VALUE TO SHOW THAT STORAGE IS FULL AND READY TO BE SHIPPED
    // for (i = 0; i < 5; i++)
    // {
    //     if(tot_logicals[i] == data_period[i])
    //     {
    //         reset[i] = 1;
    //     }
    // }
    return(0);
}

int32_t get_timestamps(int32_t *reset)
{
    int32_t i;
    int64_t timestamps[9] = {0,0,0,0,0,0,0,0,0};

    // READ IN TIMESTAMPS
    fpga_sim_timestamps(&timestamps[0]);

    // ADD IN REAL TIME OFFSET VALUE
    for (i = 0; i < 9 && timestamps[i] != 0; i++)
    {
        timestamps[i] = timestamps[i] + offset / 10;
    }

    // STORE TIMESTAMPS SEPARATELY AS SECONDS AND NANOSECONDS
    split_timestamps(&timestamps[0]);

    // CHECK IF RESET VALUE NEEDS TO BE SET
    for (i = 0; i < 2; i++)
    {
        if(tot_stamps[i] == timestamps_period[i])
        {
            reset[i+5] = 1;
        }
    }

    return(0);
}

// void rollover_timestamps(int64_t *timestamps)
// {   
//     bool roll_found = false;
//     int32_t i;
//     for (i = 0; i < 9 && timestamps[i] != 0; i++)
//     {
//         if (timestamps[i] < last_ts && !roll_found)
//         {
//             rollover += 4294967295;
//             printf("Rollover occurred at i = %d! \n", i);
//         }
//         last_ts = timestamps[i];
//         timestamps[i] += rollover;
//         // printf("Rollover added at i = %d! \n", i);
//     }
// }

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
    int32_t i, dif;
    struct timespec real_time;
    int64_t full_stamp[10];

    // for(i = 0; (i < 9) && (timestamps[i] != 0); i++)
    // {
    //     // these formulas will be replaced by the actual formulas
    //     // they currently use the tens place for the seconds and the ones for the nsecs.
    //     cam_secs[tot_stamps[0] + i] = timestamps[i] / 100000000L;
    //     cam_nsecs[tot_stamps[1] + i] = (timestamps[i] - cam_secs[tot_stamps[0] + i] * 100000000L) * 10;
    //     // printf("Nano Stamps: %d\n", cam_nsecs[tot_stamps[1] + i]);
    // }
    // new_stamps = i;

    for(i = 0; i < 9; i++)
    {
        // these formulas will be replaced by the actual formulas
        // they currently use the tens place for the seconds and the ones for the nsecs.
        timestamp_index++;
        cam_secs[ timestamp_index ] = timestamps[i] / 100000000L;
        cam_nsecs[ timestamp_index ] = (timestamps[i] - cam_secs[ timestamp_index ] * 100000000L) * 10;
        // printf("Nano Stamps: %d\n", cam_nsecs[tot_stamps[1] + i]);
    }

    // INCREMENT TOTAL TRACKER
    tot_stamps[0] += 9;
    tot_stamps[1] += 9;

    // /* DEBUGGING */
    // // print out all stored values
    // if(DEBUG_PRINT < tot_stamps[0])
    //     i = tot_stamps[0] - DEBUG_PRINT;
    // else
    //     i = 0;
    // for(; i < tot_stamps[0]; i++)
    // {
    //     printf("Second Stamp %d: %d\n", i + 1, cam_secs[i]);
    // }
    // if(DEBUG_PRINT < tot_stamps[1])
    //     i = tot_stamps[1] - DEBUG_PRINT;
    // else
    //     i = 0;
    // for(; i < tot_stamps[1]; i++)
    // {
    //     printf("Nano Stamp   %d: %9d\n", i + 1, cam_nsecs[i]);
    // }

    // /* DEBUGGING */
    // // print out all stored values
    for (i = DEBUG_PRINT-1; i >= 0; i--)
    {
        if ( timestamp_index - i < 0 )
            j = timestamp_index - i + (60*9);
        else
            j = timestamp_index - i;

        printf("Second Stamp %d: %d\n", j+1, cam_secs[j]);
    }

    for (i = DEBUG_PRINT-1; i >= 0; i--)
    {
        if ( timestamp_index - i < 0 )
            j = timestamp_index - i + (60*9);
        else
            j = timestamp_index - i;

        printf("Nano Stamp %d:   %d\n", j+1, cam_nsecs[j]);
    }

    // printf("\nTotal Logicals: %d, %d, %d, %d, %d\n", tot_logicals[0], \
    //     tot_logicals[1], tot_logicals[2], tot_logicals[3], tot_logicals[4]);
    // printf("Total Timestamps: %d, %d\n", tot_stamps[0], tot_stamps[1]);

    /* DEBUGGING */
    printf("Logical Index: %d\nTimestamp Index: %d\n", logical_index, timestamp_index);

    // CHECK IF TIMESTAMP OFFSET ADJUSTMENT IS WORKING PROPERLY
    clock_gettime(CLOCK_REALTIME, &real_time);
    dif = real_time.tv_sec - cam_secs[timestamp_index-8] - 1;

    // // DEBUGGING
    // printf("Current time: %lu\n", real_time.tv_sec);
    // printf("Last stamp:   %d\n", cam_secs[tot_stamps[0]-1]);
    // printf("Difference:   %d\n", dif);

    if( abs(dif) > CLOCK_OFFSET_TOLERANCE )
    {
        printf("\nERROR: Clock offset correction unsuccessful!\n");
        printf("ERROR may have been caused by the FPGA starting its timestamp values more ");
        printf("than %d second(s) after startup.\nOffset: %d\n\n", CLOCK_OFFSET_TOLERANCE, dif);
    }

    // // CHECK FOR ERRORS IN THE TIMESTAMPS
    // for (i = 0; i < tot_stamps[0]; i++)
    // {
    //     if (timestamps_period[0] == timestamps_period[1])
    //     {
    //         // reconstruct full timestamp
    //         full_stamp[i] = cam_secs[i]*1000000000L + cam_nsecs[i];
    //     }
    //     else
    //     {
    //         full_stamp[i] = cam_secs[i];
    //     }

    //     // check that it is greater than the previous one
    //     if (0 != i)
    //     {
    //         if (full_stamp[i] < full_stamp[i-1])
    //         {
    //             printf("\nERROR: Timestamp %d is samller than timestamp %d!\n\n", i, i-1);
    //         }
    //     }
    // }


    // CHECK FOR ERRORS IN THE TIMESTAMPS
    if ( 0 > timestamp_index - 9 )
    {
        min = timestamp_index + MAX_DATA_PERIOD*9 - 9;
    }
    else
    {
        min = timestamp_indexl - 9;
    }
    non_zero_ts = 0;
    temp_index = timestamp_index;
    while ( min < temp_index )
    {
        // create full timestamp
        full_stamp[non_zero_ts] = cam_secs[temp_index]*1000000000L + cam_nsecs[temp_index];

        // if zero
            // do nothing
        // if not zero
            // compare to previous actual timestamp
            // increment non_zero_ts
        /* We don't care about timestamps that are padded zeros, so we don't add them to 
         * the array that needs to be checked for correctness. */
        if ( 0 != full_stamp[non_zero_ts] )
        {
            if ( 0 != non_zero_ts )
            {
                if ( full_stamp[non_zero_ts] > full_stamp[non_zero_ts-1] )
                {
                    printf("\nERROR: Timestamps are out of order!\n\n");
                }
                non_zero_ts++;
            }
        }

        // if ( timestamp_index - i < 0 )
        //     j = timestamp_index - 1 + MAX_DATA_PERIOD*9;
        // else
        //     j = timestamp_index - 1;
        
        // // reconstruct full timestamp
        // full_stamp[j] = cam_secs[j]*1000000000L + cam_nsecs[j];

        // // check that it is greater than the previous one
        // if (0 != i)
        // {
        //     if (full_stamp[j] < full_stamp[j-1] && full_stamp[j] != 0)
        //     {
        //         printf("\nERROR: Timestamp %lu (taken later) is smaller than timestamp %lu (taken earlier)!\n\n", full_stamp[j], full_stamp[j-1]);
        //     }
        // }

        if ( 0 > temp_index-- )
        {
            temp_index = temp_index + MAX_DATA_PERIOD*9 - 1;
        }
        else
        {
            temp_index--;
        }
    }
}

// NO LONGER NEEDED
// void clear_logicals(int32_t mp)
// {
//     int32_t i;

//     tot_logicals[mp] = 0;
    
//     for(i = 0; i < data_period[mp]; i++)
//     {
//         if ( 0 == mp )
//             pfp_values[i] = 0;
//         if ( 1 == mp )
//             ptlt_values[i] = 0;
//         if ( 2 == mp )
//             ptrt_values[i] = 0;
//         if ( 3 == mp )
//             tcmp_values[i] = 0;
//         if ( 4 == mp )
//             cop_values[i] = 0;
//     }
// }

// NO LONGER NEEDED
// void clear_timestamps(int32_t mp)
// {
//     int32_t i;

    // tot_stamps[mp-5] = 0;
    
//     for(i = 0; i < timestamps_period[mp-5]; i++)
//     {
//         if ( 5 == mp )
//         {
//             cam_secs[i] = 0;
//             // printf("Cam_secs reset!");
//         }
//         if ( 6 == mp )
//         {
//             cam_nsecs[i] = 0;       
//             // printf("Cam_nsecs reset!");
//         }
//     }
// }