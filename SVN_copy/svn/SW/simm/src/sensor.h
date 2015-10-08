
/** @file sensor.h
 * Contains globals used in fpga_read.c and sensor.c
 *
 * Copyright (c) 2015, DornerWorks, Ltd.
 */
#ifndef __SENSORS_H__
#define __SENSORS_H__


/****************
* DATA TYPES
****************/
extern int32_t pfp_val;
extern int32_t ptlt_val;
extern int32_t ptrt_val;
extern int32_t tcmp_val;
extern int32_t cop_val;

//int32_t *pfp_values;
//int32_t *ptlt_values;
//int32_t *ptrt_values;
//int32_t *tcmp_values;
//int32_t *cop_values;
//uint32_t *cam_secs;
//uint32_t *cam_nsecs;

extern uint32_t returnVoltages[5];
extern int32_t total_ts;
extern int32_t logical_index; 
extern int32_t timestamp_index;

void timestamp_offset_config(void);
bool subscribe_config(void);
void subscribe_cleanup(void);
//void make_logicals(uint32_t *voltages);
void make_logicals(void);
int32_t convert_pfp (uint32_t voltage);
int32_t convert_ptxt (uint32_t voltage);
int32_t convert_tcmp (uint32_t voltage);
int32_t convert_cop (uint32_t voltage);
//int32_t calculate_timestamps(uint32_t *timestamps, uint32_t *ts_HiLoCnt);
int32_t calculate_timestamps(void);
//void split_timestamps(uint64_t *timestamps);
void split_timestamps(uint64_t *ts_full);
// void check_ts_values(uint64_t *new_stamps);
void check_ts_values(void);

bool fpga_init(void);
bool setup_fpga_comm(void);
bool send_fpga_config(void);
bool wait_for_fpga(void);
//void get_fpga_data(uint32_t *voltages, uint32_t *timestamps, uint32_t *ts_HiLoCnt);
void get_fpga_data(void);
void bufferFPGAdata(void);

bool calcHannWindowCo(int32_t dftN);

#endif
