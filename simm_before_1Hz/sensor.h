int32_t *pfp_values;
int32_t *ptlt_values;
int32_t *ptrt_values;
int32_t *tcmp_values;
int32_t *cop_values;

int32_t *cam_secs, *cam_nsecs;

int32_t data_period[5];
int32_t timestamps_period[2];

int32_t subscribe_config(void);
int32_t get_logicals(int32_t *reset);
int32_t get_timestamps (int32_t *reset);
int32_t convert_pfp    (int32_t voltage);
int32_t convert_ptxt   (int32_t voltage);
int32_t convert_tcmp   (int32_t voltage);
int32_t convert_cop    (int32_t voltage);
void split_timestamps  (int64_t *timestamp);
void clear_logicals(int32_t mp);
void clear_timestamps(int32_t mp);