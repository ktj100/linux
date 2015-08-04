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
#include <stdint.h>

#include "sensor.h"
#include "fpga_read.h"

/* This file will replace fpga_sim.c as the actual file that will be 
 * used to read from the FPGA. Three functions are required to be 
 * implemented in order to replace: one to wait for the FPGA to activate
 * the interrupt register, one to obtain the raw value for the logical
 * 1 Hz values, and one to obtain the timestamps. */

int32_t dev = 0;
uint32_t *fpga_regs;

void setup_registers(void)
{
    dev = open("simulation_file.bin", O_RDWR);
    fpga_regs = (uint32_t*) mmap(0, 512, PROT_READ, MAP_PRIVATE, dev ,0);
}

int32_t wait_for_fpga(void)
{
    int32_t delay_finished;

    // RESET BIT TO '1' WHEN IT BECOMES A '0'
    delay_finished = fpga_regs[IAR];
    if (0 == delay_finished)
    {
        fpga_regs[IAR] = 1;
        return(1);
    }
    else
    {
        return(0);
    }
}

void fpga_sim_voltages(int *voltage)
{
    voltage[0] = fpga_regs[PFP_VAL];
    voltage[1] = fpga_regs[PTLT_VAL];
    voltage[2] = fpga_regs[PTRT_VAL];
    voltage[3] = fpga_regs[TCMP_VAL];
    voltage[4] = fpga_regs[COP_VAL];
}

void fpga_sim_timestamps(int64_t *timestamps)
{
    timestamps[0] = fpga_regs[CAM_TS_VAL_0];
    timestamps[1] = fpga_regs[CAM_TS_VAL_1];
    timestamps[2] = fpga_regs[CAM_TS_VAL_2];
    timestamps[3] = fpga_regs[CAM_TS_VAL_3];
    timestamps[4] = fpga_regs[CAM_TS_VAL_4];
    timestamps[5] = fpga_regs[CAM_TS_VAL_5];
    timestamps[6] = fpga_regs[CAM_TS_VAL_6];
    timestamps[7] = fpga_regs[CAM_TS_VAL_7];
    timestamps[8] = fpga_regs[CAM_TS_VAL_8];
}