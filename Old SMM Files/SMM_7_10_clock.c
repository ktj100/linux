#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <time.h>

static pthread_t sensor_thread;

void *read_sensors(void *arg);
/* DEBUGGING
void read_sensors(void); */
int32_t convert_pfp  (int32_t voltage);
int32_t convert_ptxt (int32_t voltage);
int32_t convert_tcmp (int32_t voltage);
int32_t convert_cop  (int32_t voltage);
void split_timestamps(int32_t *timestamp, int32_t *cam_secs, int32_t *cam_nsecs, int32_t total);

int32_t main(void)
{
    int32_t rc;

    // spawn thread for sensor reading
    errno = 0;
    rc = pthread_create(&sensor_thread, NULL, read_sensors, NULL);
    if(0 != rc)
    {
//#ifdef TESTING
        printf("ERROR: Thread creation failed! (%d) \n", errno);
//#else
        syslog(LOG_ERR, "%s:%d thread creation failed (%d:%s)",
                   __FUNCTION__, __LINE__, errno, strerror(errno));
//#endif
    }

    /* DEBUGGING
    read_sensors(); */

    while(1)
    {
        printf("Main is running...\n");
        sleep(1);
    }
}

void *read_sensors(void *arg)
/* DEBUGGING
void read_sensors() */
{
    int32_t rc;

    // detach the thread so that main can resume
    errno = 0;
    rc = pthread_detach(pthread_self());
    if (rc != 0)
    {
        printf("ERROR: Thread detaching unsuccessful: (%d)\n", errno);
    }

    struct timespec real_clock, start_time;

    time_t run_time = 0;

    int8_t delay_finished = 0;

    int32_t new_value, voltage[5], i, timestamp[9], logical[5], temp, success;

    size_t data_period = 30;   // this value will be determined by AACM for actual code
    size_t timestamp_values = data_period * 9;

    int32_t pfp_values[data_period], ptlt_values[data_period], ptrt_values[data_period]; 
    int32_t tcmp_values[data_period], cop_values[data_period];
    int32_t cam_secs[timestamp_values], cam_nsecs[timestamp_values];

    int32_t tot_logicals = 0, tot_stamps = 0;

//#ifdef TESTING
    FILE *rawfp, *camfp;
//#endif

    // using a clock to simulate the interrupt
    clock_gettime(CLOCK_REALTIME, &start_time);

    while(1)
    {
        /* ADD INTERRUPT HERE ------------------------------------------------------- */

        // using a clock to simulate the interrupt
        for(delay_finished = 0; 1 != delay_finished;)
        {
            // read in the current time to see if it is a second multiple of the start time
            clock_gettime(CLOCK_REALTIME, &real_clock);
            if(start_time.tv_sec + run_time <= real_clock.tv_sec)
            {
                // exit the while loop and read in the values
                delay_finished = 1;
            }
        }
        run_time++;     // increment the number of seconds that the function has been running

        /* TO OCCUR IN RESPONSE TO THE INTERRUPT SIGNAL (BELOW) --------------------- */

        // read in 1 Hz values and print
        rawfp = fopen("raw.in", "r");
        if (rawfp == NULL) {
            printf("Can't open input file raw.in!\n");
        }
        for(i = 0; !feof(rawfp); i++)
        {
            new_value = fscanf(rawfp, "%d", &voltage[i]);
            if(new_value != 1)
            {
                printf("ERROR: Scan failed before reaching EOF\n");
                break;
            }
            else
            {
                //printf("Scanned Value: %ld\n", voltage[i]);
            }
        }

        // convert volatges to logical values and store in arrays
        pfp_values[tot_logicals] = convert_pfp(voltage[0]);
        ptlt_values[tot_logicals] = convert_ptxt(voltage[1]);
        ptrt_values[tot_logicals] = convert_ptxt(voltage[2]);
        tcmp_values[tot_logicals] = convert_tcmp(voltage[3]);
        cop_values[tot_logicals] = convert_cop(voltage[4]);

        // read in cam timestamps and print
        camfp = fopen("cam.in", "r");
        if (camfp == NULL) {
            printf("Can't open input file cam.in!\n");
        }
        for(i = 0; !feof(camfp); i++)
        {
            new_value = fscanf(camfp, "%d", &timestamp[i]);
            if(new_value != 1)
            {
                printf("ERROR: Scan failed before reaching EOF\n");
                break;
            }
            else
            {
                //printf("Scanned Value: %ld\n", timestamp[i]);
            }
        }

        // remove all old timestamp values and replace with NULL characters in case number of timestamps changed
        for(; i < 9; i++)
        {
            timestamp[i] = 0;
        }

        // split timestamps into seconds and nanoseconds AND store in arrays
        split_timestamps(&timestamp[0], &cam_secs[0], &cam_nsecs[0], tot_stamps);

        // increment totals
        for(i = 0; timestamp[i] != 0; i++); // count # of additional timestamps
        tot_stamps += i;
        tot_logicals++;

        /* DEBUGGING */
        // print out all stored values
        for(i = 0; i < tot_stamps; i++)
        {
            printf("\nSecond Stamp %d: %d\n", i + 1, cam_secs[i]);
            printf("Nano Stamp   %d: %d\n", i + 1, cam_nsecs[i]);

            if(i < tot_logicals)
            {
                printf("\nPFP %d: %d\n", i + 1, pfp_values[i]);
                printf("\nPTLT %d: %d\n", i + 1, ptlt_values[i]);
                printf("\nPTRT %d: %d\n", i + 1, ptrt_values[i]);
                printf("\nTCMP %d: %d\n", i + 1, tcmp_values[i]);
                printf("\nCOP %d: %d\n", i + 1, cop_values[i]);
            }
        }

        /* TO OCCUR IN RESPONSE TO THE INTERRUPT SIGNAL (ABOVE) --------------------- */
    }
    
}

int32_t convert_pfp (int32_t voltage)
{
    int32_t pressure;

    /* This algorithm is only for testing. Actual algorithm will replace it. */
    pressure = voltage * 50;

    return (pressure);
}

int32_t convert_ptxt (int32_t voltage)
{
    int32_t temp;

    /* This algorithm is only for testing. Actual algorithm will replace it. */
    temp = voltage * 20;

    return (temp);
}

int32_t convert_tcmp (int32_t voltage)
{
    int32_t temp;

    /* This algorithm is only for testing. Actual algorithm will replace it. */
    temp = voltage * 10;

    return (temp);
}

int32_t convert_cop (int32_t voltage)
{
    int32_t pressure;

    /* This algorithm is only for testing. Actual algorithm will replace it. */
    pressure = voltage * 2 - 5;

    return (pressure);
}

void split_timestamps(int32_t *timestamp, int32_t *cam_secs, int32_t *cam_nsecs, int32_t total)
{
    int32_t i;
    for(i = 0; i < 9; i++)
    {
        // these formulas will be replaced by the actual formulas
        // they currently use the tens place for the seconds and the ones for the nsecs.
        cam_secs[total + i] = timestamp[i] / 100;
        cam_nsecs[total + i] = timestamp[i] % 100;
    }
}