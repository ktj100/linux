int dev = 0;
uint32_t *fpga_regs;


init:
/* save dev so it can be shutdown gracefully when SIMM exits */
dev = open("simulation_file.bin", O_RDWR);
fpga_regs = (uint32_t*) mmap(0, 512, PROT_READ | PROT_WRITE, MAP_PRIVATE, dev ,0);

/* for simulation, we must maintain a seperate file descriptor that we call read() to get the 1 second time */
int_dev = timerfd_create(.....);

/* for the real system we would do this instead of creating a timerfd: */
int_dev = dev;


while read(int_dev) doesn't fail
	/* every second */
	value = fpga_regs[TCMP_OFFSET];
	/* if the FPGA is big endian we will have to do this: */
	value = be32toh(fpga_regs[TCMP_OFFSET]);
	...