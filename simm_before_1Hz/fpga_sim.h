
FILE *sigfp, *rawfp, *camfp;

int32_t wait_for_fpga(void);
void fpga_sim_voltages(int32_t *voltage);
void fpga_sim_timestamps(int64_t *timestamp);