int32_t get_logicals   (void);
int32_t get_timestamps (void);
int32_t convert_pfp    (int32_t voltage);
int32_t convert_ptxt   (int32_t voltage);
int32_t convert_tcmp   (int32_t voltage);
int32_t convert_cop    (int32_t voltage);
void split_timestamps  (int64_t *timestamp, int32_t *cam_secs, int32_t *cam_nsecs, int32_t total);
void clear_logicals(void);
void clear_timestamps(void);