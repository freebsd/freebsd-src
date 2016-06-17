/*
 * This file contains the code to configure and utilize the ppc64 pmc hardware
 * Copyright (C) 2002 David Engebretsen <engebret@us.ibm.com>
 */

#ifndef __KERNEL__
#define INLINE_SYSCALL(arg1, arg2)       \
  ({                                            \
    register long r0 __asm__ ("r0");     \
    register long r3 __asm__ ("r3"); \
    register long r4 __asm__ ("r4"); \
    long ret, err;                              \
    r0 = 208; \
    r3 = (long) (arg1); \
    r4 = (long) (arg2); \
    __asm__ ("sc\n\t"                           \
             "mfcr      %1\n\t"                 \
             : "=r" (r3), "=r" (err)            \
             : "r" (r0), "r" (r3), "r" (r4) \
             : "cc", "memory");                 \
    ret = r3;                                   \
  })
#endif

#ifndef __ASSEMBLY__
struct perfmon_base_struct {
	u64 profile_buffer;
	u64 profile_length;
	u64 trace_buffer;
	u64 trace_length;
	u64 trace_end;
	u64 timeslice_buffer;
	u64 timeslice_length;
	u64 state;
};

struct pmc_header {
	int subcmd;
	union {
		int type;
		int pid;	/* PID to trace */
		int slice; 	/* Timeslice ID */
	} vdata;
	int resv[30];
};

struct pmc_struct {
        unsigned long pmc[11];
};

struct pmc_info_struct {
	unsigned int mode, cpu;

	unsigned int  pmc_base[11];
	unsigned long pmc_cumulative[8];
};

struct perfmon_struct {
	struct pmc_header header;

	union {
		struct pmc_struct      pmc;
		struct pmc_info_struct pmc_info;
 	} vdata;
};

enum {
	PMC_CMD_BUFFER       = 1,
	PMC_CMD_DUMP         = 2,
	PMC_CMD_DECR_PROFILE = 3,
	PMC_CMD_PROFILE      = 4,
	PMC_CMD_TRACE        = 5,
	PMC_CMD_TIMESLICE    = 6
};

enum {
	PMC_SUBCMD_BUFFER_ALLOC         = 1,
	PMC_SUBCMD_BUFFER_FREE          = 2,
	PMC_SUBCMD_BUFFER_CLEAR         = 3 
};

enum {
	PMC_SUBCMD_DUMP_COUNTERS        = 1,
	PMC_SUBCMD_DUMP_HARDWARE        = 2
};

enum {
	PMC_SUBCMD_PROFILE_CYCLE        = 1,
};

enum {
	PMC_SUBCMD_TIMESLICE_ENABLE     = 1,
	PMC_SUBCMD_TIMESLICE_DISABLE    = 2,
	PMC_SUBCMD_TIMESLICE_SET        = 3
};

#define	PMC_TRACE_CMD 0xFF

/*
 * The following types are not used by the kernel; they are put into the
 * trace as flag records for the user space tools to interpret.
 */
enum  {
	PMC_TYPE_DERC_PROFILE   = 1,
	PMC_TYPE_CYCLE          = 2,
	PMC_TYPE_PROFILE        = 3,
	PMC_TYPE_DCACHE         = 4,
	PMC_TYPE_L2_MISS        = 5,
	PMC_TYPE_LWARCX         = 6,
	PMC_TYPE_TIMESLICE      = 7,
        PMC_TYPE_TIMESLICE_DUMP = 8,
	PMC_TYPE_END            = 8
};
#endif

#define	PMC_STATE_INITIAL         0x00
#define	PMC_STATE_READY           0x01
#define	PMC_STATE_DECR_PROFILE    0x10
#define	PMC_STATE_PROFILE_KERN    0x11
#define	PMC_STATE_TRACE_KERN      0x20
#define	PMC_STATE_TRACE_USER      0x21
#define	PMC_STATE_TIMESLICE       0x40

