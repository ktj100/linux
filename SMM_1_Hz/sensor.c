#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "sensor.h"
#include "fpga_sim.h"

;

int32_t get_logicals(void)
{
    //int32_t i;
    int32_t raw[5] = {0,0,0,0,0};

    // READ IN 1 HZ VOLTAGE VALUES
    fpga_sim_voltages(&raw[0]);

    // SAVE VOLTAGE VALUES TO VARIABLES
    if(0 != convert_pfp(raw[0]))
    {

    }
    if(0 != convert_ptlt(raw[1]))
    {

    }
    if(0 != convert_ptrt(raw[2]))
    {

    }
    if(0 != convert_tcmp(raw[3]))
    {

    }
    if(0 != convert_cop(raw[4]))
    {

    }

    /* DEBUGGING */
    printf("\nPFP: %d\n", pfp_val);
    printf("PTLT: %d\n", ptlt_val);
    printf("PTRT: %d\n", ptrt_val);
    printf("TCMP: %d\n", tcmp_val);
    printf("COP: %d\n", cop_val);

    return(0);
}

int32_t convert_pfp (int32_t voltage)
{
    // CHECK IF RAW VOLTAGE IS VALID AND INSIDE RANGE
    if(voltage >= pfp_raw_max || voltage <= pfp_raw_min)
    {
        syslog(LOG_ERR, "%s:%d PFP raw value outside valid range", __FUNCTION__, __LINE__);
        pfp_val = NULL;
        return(1);
    }

    // CONVERT RAW VOLTAGE TO LOGICAL VALUE
    // This algorithm is only for testing. Actual algorithm will replace it.
    pfp_val = voltage / 20;

    return(0);
}

int32_t convert_ptlt (int32_t voltage)
{
    // CHECK IF RAW VOLTAGE IS VALID AND INSIDE RANGE
    if(voltage >= ptxt_raw_max || voltage <= ptxt_raw_min)
    {
        syslog(LOG_ERR, "%s:%d PTLT raw value outside valid range", __FUNCTION__, __LINE__);
        ptlt_val = NULL;
        return(1);
    }

    // CONVERT RAW VOLTAGE TO LOGICAL VALUE
    // This algorithm is only for testing. Actual algorithm will replace it.
    ptlt_val = voltage / 20;

    return(0);
}

int32_t convert_ptrt (int32_t voltage)
{
    // CHECK IF RAW VOLTAGE IS VALID AND INSIDE RANGE
    if(voltage >= ptxt_raw_max || voltage <= ptxt_raw_min)
    {
        syslog(LOG_ERR, "%s:%d PTRT raw value outside valid range", __FUNCTION__, __LINE__);
        ptrt_val = NULL;
        return(1);
    }

    // CONVERT RAW VOLTAGE TO LOGICAL VALUE
    // This algorithm is only for testing. Actual algorithm will replace it.
    ptrt_val = voltage / 20;

    return(0);
}

int32_t convert_tcmp (int32_t voltage)
{
    // CHECK IF RAW VOLTAGE IS VALID AND INSIDE RANGE
    if(voltage >= tcmp_raw_max || voltage <= tcmp_raw_min)
    {
        syslog(LOG_ERR, "%s:%d TCMP raw value outside valid range", __FUNCTION__, __LINE__);
        tcmp_val = NULL;
        return(1);
    }

    // CONVERT RAW VOLTAGE TO LOGICAL VALUE
    // This algorithm is only for testing. Actual algorithm will replace it.
    tcmp_val = voltage / 200;

    return(0);
}

int32_t convert_cop (int32_t voltage)
{
    // CHECK IF RAW VOLTAGE IS VALID AND INSIDE RANGE
    if(voltage >= cop_raw_max || voltage <= cop_raw_min)
    {
        syslog(LOG_ERR, "%s:%d COP raw value outside valid range", __FUNCTION__, __LINE__);
        cop_val = NULL;
        return(1);
    }

    // CONVERT RAW VOLTAGE TO LOGICAL VALUE
    // This algorithm is only for testing. Actual algorithm will replace it.
    cop_val = voltage / 500 - 5;

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

void split_timestamps(int64_t *timestamps)
{
    int32_t i;
    int32_t tot_stamps = 0;

    for(i = 0; (i < 9) && (timestamps[i] != 0); i++)
    {
        // these formulas will be replaced by the actual formulas
        cam_secs[i] = timestamps[i] / 1000000000L;
        cam_nsecs[i] = timestamps[i] - cam_secs[i] * 1000000000L;
    }
    // INCREMENT TOTAL TRACKER
    tot_stamps = i;

    /* DEBUGGING */
    for(i = 0; i < tot_stamps; i++)
    {
        printf("\nSecond Stamp %d: %d\n", i + 1, cam_secs[i]);
        printf("Nano Stamp   %d: %9d\n", i + 1, cam_nsecs[i]);
    }
}