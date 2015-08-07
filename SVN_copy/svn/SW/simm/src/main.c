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

static pthread_t sensor_thread;

void *read_sensors(void *arg);

int32_t main(void)
{
    // DECLARE VARIABLES
    int32_t config_status;

    // SET UP EXPORT FREQUENCY FOR AACM
    // these formulas will be replaced by reading SUBSCRIBE messages -------------------------------------------------
    data_period[0] = 1;
    data_period[1] = 1;
    data_period[2] = 1;
    data_period[3] = 1;
    data_period[4] = 1;
    timestamps_period[0] = 9;
    timestamps_period[1] = 9;
    // ---------------------------------------------------------------------------------------------------------------

    // CONFIGURE SENSOR READING VALUES
    config_status = subscribe_config();

    printf("Config Complete!\n");
    // ADD ERROR HANDLING FOR CONFIGURATION ERRORS
    if(config_status)
    {

    }

    int32_t rc;

    // SPAWN THREAD FOR SENSOR READING
    // THIS THREAD WILL IMMEDIATELY START READING DATA AND PREPARING FOR SHIPPING
    errno = 0;
    rc = pthread_create(&sensor_thread, NULL, read_sensors, NULL);
    if(0 != rc)
    {
        printf("ERROR: Thread creation failed! (%d) \n", errno);
        syslog(LOG_ERR, "%s:%d thread creation failed (%d:%s)",
                   __FUNCTION__, __LINE__, errno, strerror(errno));
    }
    printf("Thread Created!\n");

    // CONTINUE WITH OTHER TASKS
    while(1)
    {
        // sleep(1);
        // printf("Main is running...\n");
    }
}

void *read_sensors(void *arg)
/* DEBUGGING
void read_sensors() */
{
    int32_t rc, fpga_ready, i;
    int32_t get_lv_status, get_ts_status;
    int32_t reset[7] = {0,0,0,0,0,0,0};

    printf("Thread Started!\n");

    // detach the thread so that main can resume
    errno = 0;
    rc = pthread_detach(pthread_self());
    if (rc != 0)
    {
        printf("ERROR: Thread detaching unsuccessful: (%d)\n", errno);
    }

    // ADJUST INTERRUPT REGISTERS
    setup_registers();
    fpga_config();

    // CALCULATE TIMEBASE OFFSET
    clock_config();

    // LOOP FOREVER, BULDING X SECONDS WORTH OF DATA AND EXPORTING
    while(1)
    {
        // printf("Loop Started!\n");
        // WAIT FOR SIGNAL FROM FPGA
        // ADD ERROR FOR >1 SEC TIMEOUT
        for(fpga_ready = 0; 1 != fpga_ready;)
        {
            // printf("Waiting on FPGA!\n");
            // "fpga_ready" WILL BE TRIGGERED BY AN INTERRUPT LATER
            fpga_ready = wait_for_fpga();
            // ADD ERROR SCENARIOS
            if(0 == fpga_ready)
            {

            }
        }
        printf("FPGA Ready!\n");

        // GET LOGICAL VALUES FROM REGISTERS
        get_lv_status = get_logicals(&reset[0]);
        // ADD ERROR CHECKING FOR STATUS
        if(1 == get_lv_status)
        {

        }
        else if (0 == get_lv_status)
        {
            
        }

        // GET TIMESTAMPS FOM REGISTERS
        get_ts_status = get_timestamps(&reset[0]);
        // ADD ERROR CHECKING FOR STATUS
        if(get_ts_status)
        {
            
        }

        // CHECK FOR FULL VALUE LISTS, AND CLEAR THEM OUT IF NEEDED
        for (i = 0; 7 > i; i++)
        {
            if ( 1 == reset[i] && 5 > i )
            {
                clear_logicals(i);
                reset[i] = 0;
            }
            else if ( 1 == reset[i] )
            {
                clear_timestamps(i);
                reset[i] = 0;
            }
        }
    }
}