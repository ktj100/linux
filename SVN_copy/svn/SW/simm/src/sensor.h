int32_t pfp_val;
int32_t ptlt_val;
int32_t ptrt_val;
int32_t tcmp_val;
int32_t cop_val;

int32_t *pfp_values;
int32_t *ptlt_values;
int32_t *ptrt_values;
int32_t *tcmp_values;
int32_t *cop_values;

int32_t voltages[5];
int32_t returnVoltages[5];

uint32_t *cam_secs, *cam_nsecs;

int32_t total_ts;
int32_t logical_index, timestamp_index;

void timestamp_offset_config(void);
bool subscribe_config(void);
void make_logicals(uint32_t *voltages);
int32_t convert_pfp (uint32_t voltage);
int32_t convert_ptxt (uint32_t voltage);
int32_t convert_tcmp (uint32_t voltage);
int32_t convert_cop (uint32_t voltage);
int32_t calculate_timestamps(uint32_t *timestamps, uint32_t *ts_HiLoCnt);
void split_timestamps(uint64_t *timestamps);
void check_ts_values(void);

bool fpga_init(void);
bool setup_fpga_comm(void);
bool send_fpga_config(void);
bool wait_for_fpga(void);
void get_fpga_data(int32_t *voltages, int32_t *timestamps, int32_t *ts_HiLoCnt);