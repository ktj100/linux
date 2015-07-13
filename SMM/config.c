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

int32_t subscribe_config(void)
{
    // GET PERIOD REQUIRED FOR PUBLISH MESSAGES FOR LOGICAL VALUES
    // (done in fpga_sim.h)

    // GET PERIOD REQUIRED FOR PUBLISH MESSAGES FOR TIMESTAMP VALUES
    // (done in fpga_sim.h)

    // ALLOCATE SPACE FOR THE STORAGE OF THE LOGICAL VALUES 
    // = period of collection time in seconds
    *pfp_values = (int32_t*)malloc(data_period);
    *ptlt_values = (int32_t*)malloc(data_period);
    *ptrt_values = (int32_t*)malloc(data_period);
    *tcmp_values = (int32_t*)malloc(data_period);
    *cop_values = (int32_t*)malloc(data_period);

    // ALLOCATE SPACE FOR THE STORAGE OF THE TIMESTAMP VALUES 
    // = period of collection time in seconds * 9 values per second (max)
    *cam_secs = (int32_t*)malloc(timestamps_period);
    *cam_nsecs = (int32_t*)malloc(timestamps_period);

}