

// THIS STRUCT CONTAINS ALL LOGICAL VALUES AFTER THEY ARE BROUGHT
// IN FROM THE FPGA
/*struct logical_storage
{
    int32_t *pfp;
    int32_t *ptlt;
    int32_t *ptrt;
    int32_t *tcmp;
    int32_t *cop;
}logical_t;*/

int32_t *pfp_values;
int32_t *ptlt_values;
int32_t *ptrt_values;
int32_t *tcmp_values;
int32_t *cop_values;

int32_t *cam_secs, *cam_nsecs;

int32_t subscribe_config(void);