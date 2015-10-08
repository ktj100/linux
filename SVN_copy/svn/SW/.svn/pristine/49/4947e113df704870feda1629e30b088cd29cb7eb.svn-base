int32_t pfp_val;
int32_t ptlt_val;
int32_t ptrt_val;
int32_t tcmp_val;
int32_t cop_val;

int32_t cam_secs[9], cam_nsecs[9];

int32_t get_logicals   (int32_t toPublish[], int32_t toPublishsize);
//int32_t get_timestamps (void);
int32_t get_timestamps(int32_t sec[], int32_t nsec[], int32_t timeSize );
int32_t convert_pfp    (int32_t voltage);
int32_t convert_ptxt   (int32_t voltage);
int32_t convert_tcmp   (int32_t voltage);
int32_t convert_cop    (int32_t voltage);
//void split_timestamps  (int64_t *timestamp);
void split_timestamps  (int64_t *timestamps, int32_t getsec[], int32_t getnsec[] );