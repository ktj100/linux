// gcc main.c sensor.c fpga_read.c -o fpga -g -lpthread
// valgrind ./smm --tool=memcheck --read-var-info=yes --leak-check=full --track-origins=yes --show-reachable=yes --show-possibly-lost=yes --malloc-fill=B5 --free-fill=4A

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#include "sensor.h"
#include "fpga_read.h"

;

static pthread_t sensor_thread;

pthread_mutex_t mp_storage_mutex = PTHREAD_MUTEX_INITIALIZER;

void *read_sensors(void *arg);
void cleanup(void);

int32_t main(void)
{
    bool success = true;
    int32_t rc;

    // THIS CHUNK SHOULD BE PLACED INTO simm_init() -------------------------------------------------------------------
    if ( false == fpga_init() )
    {
        success == false;
    }
    // ----------------------------------------------------------------------------------------------------------------

    errno = 0;
    if ( success )
    {
        rc = pthread_create(&sensor_thread, NULL, read_sensors, NULL);
        if( 0 != rc )
        {
            printf("ERROR: Thread creation failed! (%d) \n", errno);
            syslog(LOG_ERR, "%s:%d thread creation failed (%d:%s)",
                       __FUNCTION__, __LINE__, errno, strerror(errno));
        }
        printf("Thread Created!\n");
    }

    // CONTINUE WITH OTHER TASKS
    while( success )
    {
        // sleep(1);
        // printf("Main is running...\n");
    }

    // THIS CHUNK SHOULD BE PLACED INTO simm_init() -------------------------------------------------------------------
    cleanup();
    // ----------------------------------------------------------------------------------------------------------------
}

void *read_sensors(void *arg)
/* DEBUGGING
void read_sensors() */
{
    bool success = true;
    int32_t rc;

    uint32_t voltages[5] = {0,0,0,0,0};
    uint32_t timestamps[9] = {0,0,0,0,0,0,0,0,0};
    uint32_t ts_HiLoCnt[3] = {0,0,0};

    printf("Thread Started!\n");

    // detach the thread so that main can resume
    errno = 0;
    rc = pthread_detach(pthread_self());
    if (rc != 0)
    {
        printf("ERROR: Thread detaching unsuccessful: (%d)\n", errno);
    }

    // LOOP FOREVER, BULDING X SECONDS WORTH OF DATA AND EXPORTING
    while(success)
    {
        // CONTAINS A BLOCKING READ FOR THE FPGA INTERRUPT REGISTERS
        if ( false == wait_for_fpga() )
        {
            success = false;
        }
        // printf("\nFPGA Ready!\n");

        /* GET FPGA DATA IMMEDIATELY */
        get_fpga_data(&voltages[0], &timestamps[0], &ts_HiLoCnt[0]);

        pthread_mutex_lock(&mp_storage_mutex);

        // GET LOGICAL VALUES FROM REGISTERS
        make_logicals(&voltages[0]);
        // ADD ERROR CHECKING FOR STATUS

        // GET TIMESTAMPS FOM REGISTERS
        calculate_timestamps(&timestamps[0], &ts_HiLoCnt[0]);
        // ADD ERROR CHECKING FOR STATUS

        pthread_mutex_unlock(&mp_storage_mutex);
    }
}

void cleanup(void)
{
    free(pfp_values);
    free(ptlt_values);
    free(ptrt_values);
    free(tcmp_values);
    free(cop_values);
    free(cam_secs);
    free(cam_nsecs);
}