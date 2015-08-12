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

pthread_mutex_t mp_storage_mutex = PTHREAD_MUTEX_INITIALIZER;

void *read_sensors(void *arg);

int32_t main(void)
{
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
    int32_t config_status;
    int32_t rc, fpga_ready, i;
    int32_t get_lv_status, get_ts_status;
    int32_t reset[7] = {0,0,0,0,0,0,0};
    total_ts = 0;

    printf("Thread Started!\n");

    // detach the thread so that main can resume
    errno = 0;
    rc = pthread_detach(pthread_self());
    if (rc != 0)
    {
        printf("ERROR: Thread detaching unsuccessful: (%d)\n", errno);
    }

    // CONFIGURE SENSOR READING VALUES
    config_status = subscribe_config();

    printf("Config Complete!\n");
    // ADD ERROR HANDLING FOR CONFIGURATION ERRORS
    if(config_status)
    {

    }

    // ADJUST INTERRUPT REGISTERS
    setup_registers();
    fpga_config();

    // CALCULATE TIMEBASE OFFSET
    clock_config();

    // LOOP FOREVER, BULDING X SECONDS WORTH OF DATA AND EXPORTING
    while(1)
    {
        // CONTAINS A BLOCKING READ FOR THE FPGA INTERRUPT REGISTERS
        fpga_ready = wait_for_fpga();
        // ADD ERROR SCENARIOS
        if(0 == fpga_ready)
        {

        }
        printf("\nFPGA Ready!\n");

        // GET LOGICAL VALUES FROM REGISTERS
        pthread_mutex_lock(&mp_storage_mutex);
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
        pthread_mutex_unlock(&mp_storage_mutex);
    }
}