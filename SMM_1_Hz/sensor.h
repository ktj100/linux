#define pfp_raw_min -100000000
#define pfp_raw_max 100000000
#define ptxt_raw_min -100000000
#define ptxt_raw_max 100000000
#define tcmp_raw_min -100000000
#define tcmp_raw_max 100000000
#define cop_raw_min -100000000
#define cop_raw_max 100000000

int32_t pfp_val;
int32_t ptlt_val;
int32_t ptrt_val;
int32_t tcmp_val;
int32_t cop_val;

int32_t cam_secs[9], cam_nsecs[9];

int32_t get_logicals   (void);
int32_t get_timestamps (void);
int32_t convert_pfp    (int32_t voltage);
int32_t convert_ptlt   (int32_t voltage);
int32_t convert_ptrt   (int32_t voltage);
int32_t convert_tcmp   (int32_t voltage);
int32_t convert_cop    (int32_t voltage);
void split_timestamps  (int64_t *timestamp);