#define data_period (30)            // this value will be determined by AACM for actual code
#define timestamps_period (270)

FILE *sigfp, *rawfp, *camfp;

int32_t wait_for_fpga(void);
void fpga_sim_voltages(int32_t *voltage);
void fpga_sim_timestamps(int64_t *timestamp);