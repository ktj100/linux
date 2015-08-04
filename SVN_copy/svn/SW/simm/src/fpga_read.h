

// map fpga offset values for mmap() function
#define PFP_VAL  0x08      // register #8 * 4 bytes per register
#define PTLT_VAL 0x09     // bits 23:0 contain the voltage value
#define PTRT_VAL 0x0A
#define TCMP_VAL 0x0B
#define COP_VAL  0X0C

#define CAM_TS_VAL_0 0x28   // register #40 * 4 bytes per register                                                                                
#define CAM_TS_VAL_1 0x29   // bits 31:0 contain TS
#define CAM_TS_VAL_2 0x2A
#define CAM_TS_VAL_3 0x2B
#define CAM_TS_VAL_4 0x2C
#define CAM_TS_VAL_5 0x2D
#define CAM_TS_VAL_6 0x2E
#define CAM_TS_VAL_7 0x2F
#define CAM_TS_VAL_8 0x30

#define IAR 0x66
