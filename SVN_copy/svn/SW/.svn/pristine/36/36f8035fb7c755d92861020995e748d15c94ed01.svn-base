#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "sensor.h"
#include "fpga_read.h"

/* This file will replace fpga_sim.c as the actual file that will be 
 * used to read from the FPGA. Three functions are required to be 
 * implemented in order to replace: one to wait for the FPGA to activate
 * the interrupt register, one to obtain the raw value for the logical
 * 1 Hz values, and one to obtain the timestamps. */

int32_t device_fd = 0;
uint32_t *fpga_regs;

// TESTING ONLY -----------------------------------------------------------------------------------------------------
int32_t fpga_timer_fd;
// ------------------------------------------------------------------------------------------------------------------

/* This function goes through the configuration for the FPGA half of the SIMMM, 
 * and checks to see if any significant errors have occured. */
bool fpga_init(void)
{
    bool success = true;

    // CALCULATE TIMEBASE OFFSET
    timestamp_offset_config();  
    // no error checking necessary here

    // ADJUST INTERRUPT REGISTERS
    if ( false == setup_fpga_comm() )
    {
        success = false;
    }
    if ( false == send_fpga_config() )
    {
        success = false;
    }

    // CONFIGURE SENSOR READING VALUES
    if ( false == subscribe_config() )
    {
        success = false;
    }

    printf("Config Complete!\n");
    return (success);
}

/* This function is needed to set up the interface between the FPGA and the software through UIO.
 * In testing, an mmap'ed file is used rather than an mmap'ed device, and a periodic timer and 
 * blocking read() funcion are used. */
bool setup_fpga_comm(void)
{
    bool success = true;
    struct timespec start, interval;
    struct itimerspec fpga_timer_setup;
    int32_t delay_finished = 1, new_fd;

    // The two following functions will be replaced for the actual FPGA interface.
    errno = 0;

    // TESTING ONLY ------------------------------------------------------------------------------------------------------
    device_fd = open("src/simulation_file.bin", O_RDWR);
    if ( -1 == device_fd )
    {
        printf("\nERROR: open() failed for FPGA device file simulation_file.bin! (%d: %s) \n\n", errno, strerror(errno));
        syslog(LOG_ERR, "%s:%d ERROR: open() failed for FPGA device file simulation_file.bin! (%d: %s)", \
            __FUNCTION__, __LINE__, errno, strerror(errno));
        success = false;
    }
    // ACTUAL FPGA CODE --------------------------------------------------------------------------------------------------

    // insert code here:

    // -------------------------------------------------------------------------------------------------------------------

    errno = 0;
    fpga_regs = (uint32_t*) mmap(0, 512, PROT_READ | PROT_WRITE, MAP_SHARED, device_fd ,0);
    if ( -1 == *fpga_regs )
    {
        printf("\nERROR: mmap() failed for FPGA interface! (%d: %s) \n\n", errno, strerror(errno));
        syslog(LOG_ERR, "%s:%d ERROR: mmap() failed for FPGA interface! (%d: %s)", \
        __FUNCTION__, __LINE__, errno, strerror(errno));
        success = false;
    }

    // TESTING ONLY ------------------------------------------------------------------------------------------------------
    // create timer
    errno = 0;
    fpga_timer_fd = timerfd_create(CLOCK_REALTIME, 0);
    if ( -1 == fpga_timer_fd )
    {
        printf("\nERROR: timerfd_create() failed! (%d: %s)\n\n", errno, strerror(errno));
        syslog(LOG_ERR, "%s:%d ERROR: timerfd_create() failed! (%d: %s)", __FUNCTION__, __LINE__, errno, strerror(errno));
        success = false;
    }

    // time before first timer expiration
    start.tv_sec = 1;
    start.tv_nsec = 0;

    // interval between all following expirations
    interval.tv_sec = 1;
    interval.tv_nsec = 0;

    // start and interval structs will be used for the timerfd_settime setup
    fpga_timer_setup.it_interval = interval;
    fpga_timer_setup.it_value = start;

    
    // timer will be set up to start immediately after FPGA sets up, then at 1 sec intervals
    while ( delay_finished != 0 )
    {
        delay_finished = fpga_regs[IAR];
    }
    printf("I'm guessing I get here?\n");
    errno = 0;
    new_fd = timerfd_settime(fpga_timer_fd, 0, &fpga_timer_setup, NULL);
    if ( -1 == new_fd )
    {
        printf("ERROR: timerfd_settime() failed! (%d: %s)\n", errno, strerror(errno));
        syslog(LOG_ERR, "%s:%d ERROR: timerfd_create() failed! (%d: %s)", __FUNCTION__, __LINE__, errno, strerror(errno));
        success = false;
    }
    fpga_regs[IAR] = 1;
    // printf("Timerfd_settime result: %d\n", success);
    // ACTUAL FPGA CODE --------------------------------------------------------------------------------------------------

    // insert code here:

    // -------------------------------------------------------------------------------------------------------------------

    return(success);
}

/* This funtion is for adjusting the interrupt settings in the FPGA. */
bool send_fpga_config(void)
{
    bool success = true;

    /* Set bit to '1' for edge sensing */
    fpga_regs[ISR] = 1;
    /* Set bit to one for rising edge sensing */
    fpga_regs[IEVR] = 1;
    /* Set bit to '1' to enable interrupt on input */
    fpga_regs[IMR] = 1;

    return (success);
}

bool wait_for_fpga(void)
{
    bool success = true;
    int64_t num_interrupts;

    errno = 0;

    // TESTING ONLY -------------------------------------------------------------------------------------------------------
    printf("Starting blocking read()...");
    if ( -1 == read(fpga_timer_fd, &num_interrupts, sizeof(num_interrupts)) )
    {
        printf("\nERROR: Read function failed! (%d: %s) \n\n", errno, strerror(errno));
        syslog(LOG_ERR, "%s:%d ERROR: Read function failed! (%d: %s)", __FUNCTION__, __LINE__, errno, strerror(errno));
        success = false;
    }
    printf("Blocking read() finished...");
    fpga_regs[IAR] = 1;
    // ACTUAL FPGA CODE ---------------------------------------------------------------------------------------------------

    // insert code here:

    // --------------------------------------------------------------------------------------------------------------------

    if ( 1 < num_interrupts )
    {
        printf("\nERROR: Missed %lu interrupts from FPGA! \n\n", num_interrupts-1);
        syslog(LOG_ERR, "%s:%d ERROR: Missed %lu interrupts from FPGA!", __FUNCTION__, __LINE__, num_interrupts-1);
    }

    return(success);
}

void get_fpga_data(int32_t *voltages, int32_t *timestamps, int32_t *ts_HiLoCnt)
{
    /* Bits 23:0 contain the voltage values in the given registers,
     * so the other 8 bits are masked off. */
    voltages[0] = fpga_regs[PFP_VAL] & 0x00FFFFFF;
    voltages[1] = fpga_regs[PTLT_VAL] & 0x00FFFFFF;
    voltages[2] = fpga_regs[PTRT_VAL] & 0x00FFFFFF;
    voltages[3] = fpga_regs[TCMP_VAL] & 0x00FFFFFF;
    voltages[4] = fpga_regs[COP_VAL] & 0x00FFFFFF;

    /* High and low bits are to determint the upper 32 bits of the 
     * 9 timestamps. */
    timestamps[0] = fpga_regs[CAM_TS_VAL_0];
    timestamps[1] = fpga_regs[CAM_TS_VAL_1];
    timestamps[2] = fpga_regs[CAM_TS_VAL_2];
    timestamps[3] = fpga_regs[CAM_TS_VAL_3];
    timestamps[4] = fpga_regs[CAM_TS_VAL_4];
    timestamps[5] = fpga_regs[CAM_TS_VAL_5];
    timestamps[6] = fpga_regs[CAM_TS_VAL_6];
    timestamps[7] = fpga_regs[CAM_TS_VAL_7];
    timestamps[8] = fpga_regs[CAM_TS_VAL_8];

    ts_HiLoCnt[0] = fpga_regs[TS_HIGH];
    ts_HiLoCnt[1] = fpga_regs[TS_LOW];
    ts_HiLoCnt[2] = fpga_regs[TS_COUNT];    
}