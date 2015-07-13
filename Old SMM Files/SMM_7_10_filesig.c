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
void split_timestamps(int64_t *timestamp, int32_t *cam_secs, int32_t *cam_nsecs, int32_t total);

int32_t main(void)
{
    int32_t rc;

    // spawn thread for sensor reading
    errno = 0;
    rc = pthread_create(&sensor_thread, NULL, read_sensors, NULL);
    if(0 != rc)
    {
        printf("ERROR: Thread creation failed! (%d) \n", errno);
        syslog(LOG_ERR, "%s:%d thread creation failed (%d:%s)",
                   __FUNCTION__, __LINE__, errno, strerror(errno));
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

    // temporary variables
    int32_t delay_finished;
    int32_t voltage[5], logical[5];
    int64_t timestamp[9];
    int32_t temp, i;

    // long term tracking variables
    time_t run_time = 0;
    int32_t tot_logicals = 0, tot_stamps = 0;

    // long term storage variables for 5 1 Hz sensors
    size_t data_period = 30;   // this value will be determined by AACM for actual code
    int32_t pfp_values[data_period];
    int32_t ptlt_values[data_period];
    int32_t ptrt_values[data_period];
    int32_t tcmp_values[data_period];
    int32_t cop_values[data_period];

    // long term storage variables for cam timestamps
    size_t timestamp_values = data_period * 9;
    int32_t cam_secs[timestamp_values], cam_nsecs[timestamp_values];

    // file declarations
    FILE *rawfp, *camfp, *sigfp;

    while(1)
    {
        /* ADD INTERRUPT HERE ------------------------------------------------------- */

        // check if the signal bit has been changed to 1
        for(delay_finished = 0; 0 == delay_finished;)
        {
            sigfp = fopen("sig.in", "r");
            if (sigfp == NULL)
            {
                printf("Can't open input file sig.in!\n");
            }
            fscanf(sigfp, "%d", &delay_finished);
            fclose(sigfp);
        }

        // change the signal bit back to 0
        sigfp = fopen("sig.in", "w+");
        if (sigfp == NULL) 
        {
            printf("Can't open input file sig.in!\n");
        }
        fprintf(sigfp, "0");
        fclose(sigfp);

        /* TO OCCUR IN RESPONSE TO THE INTERRUPT SIGNAL (BELOW) --------------------- */

        // read in 1 Hz values and print
        rawfp = fopen("raw.in", "r");
        if (rawfp == NULL) {
            printf("Can't open input file raw.in!\n");
        }
        for(i = 0; !feof(rawfp); i++)
        {
            temp = fscanf(rawfp, "%d", &voltage[i]);
            if(1 != temp)
            {
                printf("ERROR: Scan failed before reaching EOF\n");
                break;
            }
            else
            {
                //printf("Scanned Value: %d\n", voltage[i]);
            }
        }
        fclose(rawfp);

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
            temp = fscanf(camfp, "%ld", &timestamp[i]);
            if(temp != 1)
            {
                printf("ERROR: Scan failed before reaching EOF\n");
                break;
            }
            else
            {
                //printf("Scanned Value: %ld\n", timestamp[i]);
            }
        }
        fclose(camfp);

        // split timestamps into seconds and nanoseconds AND store in arrays
        split_timestamps(&timestamp[0], &cam_secs[0], &cam_nsecs[0], tot_stamps);

        // increment totals
        tot_stamps += i;
        tot_logicals++;

        // remove all old timestamp values and replace with NULL characters in case number of timestamps changed
        for(; i < 9; i++)
        {
            timestamp[i] = 0;
        }

        /* DEBUGGING */
        // print out all stored values
        if(9 < tot_stamps)
            i = tot_stamps - 9;
        else
            i = 0;
        for(; i < tot_stamps; i++)
        {
            printf("\nSecond Stamp %d: %d\n", i + 1, cam_secs[i]);
            printf("Nano Stamp   %d: %9d\n", i + 1, cam_nsecs[i]);
        }
        if(3 < tot_logicals)
            i = tot_logicals - 3;
        else
            i = 0;
        for(; i < tot_logicals; i++)
        {
            if(i < tot_logicals)
            {
                printf("\nPFP %d: %d\n", i + 1, pfp_values[i]);
                printf("PTLT %d: %d\n", i + 1, ptlt_values[i]);
                printf("PTRT %d: %d\n", i + 1, ptrt_values[i]);
                printf("TCMP %d: %d\n", i + 1, tcmp_values[i]);
                printf("COP %d: %d\n", i + 1, cop_values[i]);
            }
        }

        /* DEBUGGING */
        printf("\nTotal Logicals: %d\nTotal Timestamps: %d\n", tot_logicals, tot_stamps);

        // send out all old values at end of period when AACM requests it
        if(data_period <= tot_logicals)
        {
            tot_logicals = 0;
            tot_stamps = 0;

            for(i = 0; i < data_period; i++)
            {
                pfp_values[i] = 0;
                ptlt_values[i] = 0;
                ptrt_values[i] = 0;
                tcmp_values[i] = 0;
                cop_values[i] = 0;
            }
            for(i = 0; i < timestamp_values; i++)
            {
                cam_secs[i] = 0;
                cam_nsecs[i] = 0;
            }
        }
        /* TO OCCUR IN RESPONSE TO THE INTERRUPT SIGNAL (ABOVE) --------------------- */
    }
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

void split_timestamps(int64_t *timestamp, int32_t *cam_secs, int32_t *cam_nsecs, int32_t total)
{
    int32_t i;
    for(i = 0; i < 9; i++)
    {
        // these formulas will be replaced by the actual formulas
        // they currently use the tens place for the seconds and the ones for the nsecs.
        cam_secs[total + i] = timestamp[i] / 1000000000L;
        cam_nsecs[total + i] = timestamp[i] - cam_secs[total + i] * 1000000000L;
    }
}