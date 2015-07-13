size_t data_period = 30;   // this value will be determined by AACM for actual code
size_t timestamps_period = data_period * 9;

FILE *sigfp;

int32_t wait_for_fpga(void);