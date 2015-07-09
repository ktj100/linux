#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>

static pthread_t sensor_thread;

void *read_sensors(void *arg);
/* DEBUGGING
void read_sensors(void); */
int32_t convert_pfp (int32_t voltage);
int32_t convert_ptxt (int32_t voltage);
int32_t convert_tcmp (int32_t voltage);
int32_t convert_cop (int32_t voltage);

int32_t main(void)
{
    int32_t rc;

    // spawn thread for sensor reading
    errno = 0;
    //rc = pthread_create(&sensor_thread, NULL, read_sensors, NULL);
    if(0 != rc)
    {
//#ifdef TESTING
        printf("ERROR: Thread creation failed! (%d) \n", errno);
//#else
        syslog(LOG_ERR, "%s:%d thread creation failed (%d:%s)",
                   __FUNCTION__, __LINE__, errno, strerror(errno));
//#endif
    }

    read_sensors();

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

    int32_t value, voltage[5], i, timestamp[9], logical[5], temp, success;

    int32_t data_period = 30;   // this value will be determined by AACM for actual code

    int32_t pfp_values[data_period], ptlt_values[data_period], ptrt_values[data_period]; 
    int32_t tcmp_values[data_period], cop_values[data_period];
    int32_t cam_secs[data_period*9], cam_nsecs[data_period*9];

    int32_t tot_logicals = 0, tot_stamps = 0;

//#ifdef TESTING
    FILE *rawfp, *camfp;
//#endif

    while(1)
    {
        // read in 1 Hz values and print
//#ifdef TESTING
    	//printf("\nLoop 1\n\n");

        rawfp = fopen("raw.in", "r");

        if (rawfp == NULL) {
            printf("Can't open input file raw.in!\n");
        }

        //printf("\nLoop 2\n\n");

        i = 0;
        while(!feof(rawfp))
        {
            value = fscanf(rawfp, "%d", &voltage[i]);
            if(value != 1)
            {
                printf("ERROR: Scan failed before reaching EOF\n");
                break;
            }
            else
                //printf("Scanned Value: %ld\n", voltage[i]);
            i++;
        }
//#endif
        //printf("\nLoop 3\n\n");

        // convert volatges to logical values
        logical[0] = convert_pfp(voltage[0]);
        logical[1] = convert_ptxt(voltage[1]);
        logical[2] = convert_ptxt(voltage[2]);
        logical[3] = convert_tcmp(voltage[3]);
        logical[4] = convert_cop(voltage[4]);

        //printf("\nLoop 4\n\n");

        // read in cam timestamps and print
//#ifdef TESTING
        camfp = fopen("cam.in", "r");

        //printf("File Opened\n");

        if (camfp == NULL) {
            printf("Can't open input file cam.in!\n");
        }

        //printf("\nLoop 5\n\n");

        i = 0;
        while(!feof(camfp))
        {
            value = fscanf(camfp, "%d", &timestamp[i]);
            if(value != 1)
            {
                printf("ERROR: Scan failed before reaching EOF\n");
                break;
            }
            else
                //printf("Scanned Value: %ld\n", timestamp[i]);
            i++;
        }

        //printf("\nLoop 6\n\n");

        // remove all old timestamp values and replace with NULL characters
        for(; i < 9; i++)
        {
            timestamp[i] = 0;
        }
//#endif

        /* DEBUGGING
        printf("\nLoop 7\n\n");

        // print out all values
        1 Hz Values
        printf("\nPFP:  %4d PSI\nPTLT: %4d *C\nPTRT: %4d *C\nTCMP: %4d *C\nCOP: %2d/10 in. of H2O\n\n", 
        	logical[0], logical[1], logical[2], logical[3], logical[4]);

        // Cam Timestamps
        for(i = 0; timestamp[i] != 0; i++)                   
        {
            printf("Timestamp    %d: %d\n", i + 1, timestamp[i]);
            printf("Second Stamp %d: %d\n", i + 1, cam_secs[tot_stamps-i-1]);
            printf("Nano Stamp   %d: %d\n", i + 1, cam_nsecs[tot_stamps-i-1]);
        } */

        /* DEBUGGING
        printf("logical[0] = %d\n", logical[0]);
        printf("logical[1] = %d\n", logical[1]);
		printf("logical[2] = %d\n", logical[2]);
        printf("logical[3] = %d\n", logical[3]);
        printf("logical[4] = %d\n", logical[4]); l*/

        // store all values until they are requested by AACM
        for(i = 0; i < 5; success = (i++))		// without the "success =", 'i' stops incrementing when 
        {										// this for loop is activated for the 16th time
        	/* DEBUGGING
        	printf("'i' increment success = %d\n", success);
        	printf("\ni: %d\n\n", i);
            printf("Total Logical Values: %d, Stored Here: %d\n", tot_logicals + i, logical[0]);
            printf("Total Logical Values: %d, Stored Here: %d\n", tot_logicals + i, logical[1]);
            printf("Total Logical Values: %d, Stored Here: %d\n", tot_logicals + i, logical[2]);
            printf("Total Logical Values: %d, Stored Here: %d\n", tot_logicals + i, logical[3]);
            printf("Total Logical Values: %d, Stored Here: %d\n", tot_logicals + i, logical[4]); */
        	index = tot_logicals + i;
            pfp_values[index]  = logical[0];
            ptlt_values[index] = logical[1];
            ptrt_values[index] = logical[2];
            tcmp_values[index] = logical[3];
            cop_values[index]  = logical[4];
        }

        //printf("\nLoop 8\n\n");

        // split timestamps into seconds and nanoseconds AND store
        split_timestamps(&timestamp, &cam_secs, &cam_nsecs, tot_stamps);

        //printf("\nLoop 9\n\n");

        // increment totals
        for(i = 0; timestamp[i] != 0; i++);
        tot_stamps += i;
        tot_logicals += 5;

        //printf("\nLoop 10\n\n");

        // print out all stored values
        for(i = 0; i < tot_stamps; i++)
        {
            printf("Second Stamp %d: %d\n", i + 1, cam_secs[i]);
            printf("Nano Stamp   %d: %d\n", i + 1, cam_nsecs[i]);
        }

        //printf("\nLoop 11\n\n");

        sleep(1);
    }

    printf("Infinite loop exited... \n");
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
    int32_t final;

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