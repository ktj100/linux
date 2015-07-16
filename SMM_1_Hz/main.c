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

static pthread_t sensor_thread;

void *read_sensors(void *arg);

int32_t main(void)
{
    // DECLARE VARIABLES

    // CONFIGURE SENSOR READING VALUES

    // ADD ERROR HANDLING FOR CONFIGURATION ERRORS

    int32_t fpga_rc;

    // SPAWN THREAD FOR SENSOR READING
    // THIS THREAD WILL IMMEDIATELY START READING DATA AND PREPARING FOR SHIPPING
    errno = 0;
    fpga_rc = pthread_create(&sensor_thread, NULL, read_sensors, NULL);
    if(0 != fpga_rc)
    {
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

    // time tracking structs
    struct timespec current_time, start_time;

    // detach the thread so that main can resume
    errno = 0;
    rc = pthread_detach(pthread_self());
    if (rc != 0)
    {
        syslog(LOG_ERR, "%s:%d thread detaching unsuccessful (%d:%s)",
                   __FUNCTION__, __LINE__, errno, strerror(errno));
    }

    // LOOP FOREVER, BULDING 1 SECOND OF DATA AND EXPORTING
    while(1)
    {
        // WAIT FOR SIGNAL FROM FPGA
        // ADD ERROR FOR >1 SEC TIMEOUT
        clock_gettime(CLOCK_REALTIME, &start_time);
        for(fpga_ready = 0; 1 != fpga_ready;)
        {
            // "fpga_ready" WILL BE TRIGGERED BY AN INTERRUPT LATER
            fpga_ready = wait_for_fpga();
            // ADD ERROR SCENARIOS (WILL DEPEND ON FUNCTIONS USED TO READ ACTUAL SIGNAL BIT)
            if(-1 == fpga_ready)
            {
                syslog(LOG_ERR, "%s:%d FPGA not working properly", __FUNCTION__, __LINE__);
            }
            clock_gettime(CLOCK_REALTIME, &current_time);
            if (/* current_time >= 1.5 sec + start_time */0)
            {
                syslog(LOG_ERR, "%s:%d FPGA timeoutl", __FUNCTION__, __LINE__);
                // SLEEP TO AVOID SPAMMING OF SYSLOG ERRORS
                sleep(1);
            }
        }

        // GET LOGICAL VALUES FROM REGISTERS
        get_lv_status = get_logicals();
        // ADD ERROR CHECKING FOR STATUS
        if(1 == get_lv_status)
        {

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

        // SEND VALUES TO AACM
    }
}