#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "fpga_read.h"

void main(void)
{
    // time tracking structs
    struct timespec real_clock, start_time;

    // temporary variables
    int delay_finished = 0, i;
    int num_timestamps;
    long int timestamps[9] = {0,0,0,0,0,0,0,0,0};

    // long term tracking variables
    time_t run_time_s = 0, run_time_ns = 0;
    double delta_time = 800000000;
    double pfp_voltage = 0.4;
    double ptlt_voltage = 1.0;
    double ptrt_voltage = 1.0;
    double tcmp_voltage = 1.0;
    double cop_voltage = 0.3;

    // file declarations
    //FILE *rawfp, *camfp, *sigfp;
    int dev = 0, result;
    unsigned int *fpga_regs;

    dev = open("simulation_file.bin", O_RDWR);

    // lseek(dev, 511, SEEK_SET);
    // result = write(dev, "", 1);
    // printf("%d\n", result);

    fpga_regs = (unsigned int*) mmap(0, 512, PROT_READ | PROT_WRITE, MAP_PRIVATE, dev ,0);
    printf("fpga_regs = %p\n", fpga_regs);

    // using a clock to simulate the interrupt
    clock_gettime(CLOCK_REALTIME, &start_time);

    // set initial delta time value

    while(1)
    {

        // using a clock to simulate the interrupt
        for(delay_finished = 0; 1 != delay_finished;)
        {
            // read in the current time to see if it is a second multiple of the start time
            clock_gettime(CLOCK_REALTIME, &real_clock);
            if(start_time.tv_sec + run_time_s <= real_clock.tv_sec)
            {
                // exit the while loop and read in the values
                delay_finished = 1;
            }
        }

        /* Timestamp Calculation ------------------------------------------------------------------- */

        // calculate all timestamps for the next second, fill places over 1 second with 0's
        for(i = 0 ;run_time_ns < 1000000000; i++)
        {
            delta_time = delta_time + 0.01 * delta_time * (1 - delta_time / 114285714);
                                                             // (1 / 8.75 = 114285714)
            printf("Delta-Time: %lf\n", delta_time);
 
            run_time_ns += delta_time;
            timestamps[i] = 1000000000L * run_time_s + run_time_ns;
        }
        num_timestamps = i;

        // remove one second's worth of ns and add to s counter
        run_time_ns -= 1000000000;
        run_time_s++;

        // write timestamps into file cam.in
        // camfp = fopen("cam.in", "w+");
        // if (camfp == NULL) {
        //     printf("Can't open input file cam.in!\n");
        // }

        // fprintf(camfp, "%ld", timestamps[0]);

        // for(i = 0; i < num_timestamps; i++)
        // {
        //     // fprintf(camfp, "\n%ld", timestamps[i]);
        // }
        // fclose(camfp);

        printf("Address 1: %d\n", &fpga_regs[CAM_TS_VAL_0]);
        printf("Address 2: %d\n", &fpga_regs[CAM_TS_VAL_1]);
        printf("Address 3: %d\n", &fpga_regs[CAM_TS_VAL_2]);
        printf("Address 4: %d\n", &fpga_regs[CAM_TS_VAL_3]);
        printf("Address 5: %d\n", &fpga_regs[CAM_TS_VAL_4]);
        printf("Address 6: %d\n", &fpga_regs[CAM_TS_VAL_5]);
        printf("Address 7: %d\n", &fpga_regs[CAM_TS_VAL_6]);
        printf("Address 8: %d\n", &fpga_regs[CAM_TS_VAL_7]);
        printf("Address 9: %d\n", &fpga_regs[CAM_TS_VAL_8]);

        fpga_regs[CAM_TS_VAL_0] = timestamps[0];                                                                       
        fpga_regs[CAM_TS_VAL_1] = timestamps[1];
        fpga_regs[CAM_TS_VAL_2] = timestamps[2];
        fpga_regs[CAM_TS_VAL_3] = timestamps[3];
        fpga_regs[CAM_TS_VAL_4] = timestamps[4];
        fpga_regs[CAM_TS_VAL_5] = timestamps[5];
        fpga_regs[CAM_TS_VAL_6] = timestamps[6];
        fpga_regs[CAM_TS_VAL_7] = timestamps[7];
        fpga_regs[CAM_TS_VAL_8] = timestamps[8];

        printf("Value 1: %u\n", (unsigned int)fpga_regs[CAM_TS_VAL_0]);
        printf("Value 2: %u\n", (unsigned int)fpga_regs[CAM_TS_VAL_1]);
        printf("Value 3: %u\n", (unsigned int)fpga_regs[CAM_TS_VAL_2]);
        printf("Value 4: %u\n", (unsigned int)fpga_regs[CAM_TS_VAL_3]);
        printf("Value 5: %u\n", (unsigned int)fpga_regs[CAM_TS_VAL_4]);
        printf("Value 6: %u\n", (unsigned int)fpga_regs[CAM_TS_VAL_5]);
        printf("Value 7: %u\n", (unsigned int)fpga_regs[CAM_TS_VAL_6]);
        printf("Value 8: %u\n", (unsigned int)fpga_regs[CAM_TS_VAL_7]);
        printf("Value 9: %u\n", (unsigned int)fpga_regs[CAM_TS_VAL_8]);

        /* Logical Value Calculation --------------------------------------------------------------- */

        pfp_voltage  = pfp_voltage + 0.5 * pfp_voltage* (1 - pfp_voltage / 4.234);
        ptlt_voltage = ptlt_voltage + 0.1 * ptlt_voltage* (1 - ptlt_voltage / 23.34);
        ptrt_voltage = ptrt_voltage + 0.1 * ptrt_voltage* (1 - ptrt_voltage / 20.23);
        tcmp_voltage = tcmp_voltage + 0.1 * tcmp_voltage* (1 - tcmp_voltage / 17.2);
        cop_voltage  = cop_voltage + 0.2 * cop_voltage* (1 - cop_voltage / 3.345);

        // DEBUGGING
        printf("PFP: %lf\n", pfp_voltage);
        printf("PTLT: %lf\n", ptlt_voltage);
        printf("PTRT: %lf\n", ptrt_voltage);
        printf("TCMP: %lf\n", tcmp_voltage);
        printf("COP: %lf\n", cop_voltage);

        // rawfp = fopen("raw.in", "w+");
        // if (rawfp == NULL) 
        // {
        //     printf("Can't open input file raw.in!\n");
        // }

        // fprintf(rawfp, "%d\n%d\n%d\n%d\n%d", (int) (pfp_voltage * 1000), (int) (ptlt_voltage * 1000), 
        //     (int) (ptrt_voltage * 1000), (int) (tcmp_voltage * 1000), (int) (cop_voltage * 1000)); 
        // fclose(rawfp);

        fpga_regs[PFP_VAL] = pfp_voltage;
        fpga_regs[PTLT_VAL] = ptlt_voltage;
        fpga_regs[PTRT_VAL] = ptrt_voltage;
        fpga_regs[TCMP_VAL] = tcmp_voltage;
        fpga_regs[COP_VAL] = cop_voltage;

        /* Send Boolean Signal --------------------------------------------------------------------- */

        // sigfp = fopen("sig.in", "w+");
        // if (sigfp == NULL)
        // {
        //     printf("Can't open input file sig.in!\n");
        // }
        // fprintf(sigfp, "1");
        // fclose(sigfp);

        // NOTE: THIS IS NOT THE ACTUAL FUNCTIONALITY OF REGISTER 61!!
        // set register 61 to 0 to signal data ready
        fpga_regs[IAR] = 0;

        printf("'1' written to file.\n");
    }
}