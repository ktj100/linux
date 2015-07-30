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
    data_period = 10;
    timestamps_period = 9 * data_period;
    // ---------------------------------------------------------------------------------------------------------------

    // CONFIGURE SENSOR READING VALUES
    config_status = subscribe_config();
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

    // CONTINUE WITH OTHER TASKS
    while(1);
}


void *read_sensors(void *arg)
/* DEBUGGING
void read_sensors() */
{
    int32_t rc, fpga_ready;
    int32_t get_lv_status, get_ts_status;
    int32_t reset = 0;

    // detach the thread so that main can resume
    errno = 0;
    rc = pthread_detach(pthread_self());
    if (rc != 0)
    {
        printf("ERROR: Thread detaching unsuccessful: (%d)\n", errno);
    }

    // LOOP FOREVER, BULDING X SECONDS WORTH OF DATA AND EXPORTING
    while(1)
    {
        // WAIT FOR SIGNAL FROM FPGA
        // ADD ERROR FOR >1 SEC TIMEOUT
        for(fpga_ready = 0; 1 != fpga_ready;)
        {
            // "fpga_ready" WILL BE TRIGGERED BY AN INTERRUPT LATER
            fpga_ready = wait_for_fpga();
            // ADD ERROR SCENARIOS
            if(0 == fpga_ready)
            {

            }
        }

        // GET LOGICAL VALUES FROM REGISTERS
        get_lv_status = get_logicals();
        // ADD ERROR CHECKING FOR STATUS
        if(1 == get_lv_status)
        {
            // STORAGE IS FULL
            // SEND VALUES OUT TO AACM

            reset = 1;
        }
        else if (0 == get_lv_status)
        {
            
        }

        // GET TIMESTAMPS FOM REGISTERS
        get_ts_status = get_timestamps();
        // ADD ERROR CHECKING FOR STATUS
        if(get_ts_status)
        {
            
        }

        // CHECK FOR FULL VALUE LISTS, AND CLEAR THEM OUT IF NEEDED
        if(1 == reset)
        {
            clear_logicals();
            clear_timestamps();

            // RESET THE RESET SIGNAL
            reset = 0;
        }
    }
}