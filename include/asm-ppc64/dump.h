/*
 * Kernel header file for Linux crash dumps.
 *
 * Created by: Todd Inglett <tinglett@vnet.ibm.com>
 *
 * Copyright 2002 International Business Machines
 *
 * This code is released under version 2 of the GNU GPL.
 */

/* This header file holds the architecture specific crash dump header */
#ifndef _ASM_DUMP_H
#define _ASM_DUMP_H

/* necessary header files */
#include <asm/ptrace.h>                          /* for pt_regs             */
#include <linux/threads.h>

/* definitions */
#define DUMP_ASM_MAGIC_NUMBER     0xdeaddeadULL  /* magic number            */
#define DUMP_ASM_VERSION_NUMBER   0x1            /* version number          */


/*
 * Structure: dump_header_asm_t
 *  Function: This is the header for architecture-specific stuff.  It
 *            follows right after the dump header.
 */
typedef struct _dump_header_asm_s {

        /* the dump magic number -- unique to verify dump is valid */
        uint64_t             dha_magic_number;

        /* the version number of this dump */
        uint32_t             dha_version;

        /* the size of this header (in case we can't read it) */
        uint32_t             dha_header_size;

	/* the dump registers */
	struct pt_regs       dha_regs;

	/* smp specific */
	uint32_t	     dha_smp_num_cpus;
	int		     dha_dumping_cpu;	
	struct pt_regs	     dha_smp_regs[NR_CPUS];
	void *		     dha_smp_current_task[NR_CPUS];
	void *		     dha_stack[NR_CPUS];
} dump_header_asm_t;

#ifdef __KERNEL__
static inline void get_current_regs(struct pt_regs *regs)
{
	__asm__ __volatile__ (
		"std	0,0(%0)\n"
		"std	1,8(%0)\n"
		"std	2,16(%0)\n"
		"std	3,24(%0)\n"
		"std	4,32(%0)\n"
		"std	5,40(%0)\n"
		"std	6,48(%0)\n"
		"std	7,56(%0)\n"
		"std	8,64(%0)\n"
		"std	9,72(%0)\n"
		"std	10,80(%0)\n"
		"std	11,88(%0)\n"
		"std	12,96(%0)\n"
		"std	13,104(%0)\n"
		"std	14,112(%0)\n"
		"std	15,120(%0)\n"
		"std	16,128(%0)\n"
		"std	17,136(%0)\n"
		"std	18,144(%0)\n"
		"std	19,152(%0)\n"
		"std	20,160(%0)\n"
		"std	21,168(%0)\n"
		"std	22,176(%0)\n"
		"std	23,184(%0)\n"
		"std	24,192(%0)\n"
		"std	25,200(%0)\n"
		"std	26,208(%0)\n"
		"std	27,216(%0)\n"
		"std	28,224(%0)\n"
		"std	29,232(%0)\n"
		"std	30,240(%0)\n"
		"std	31,248(%0)\n"
		"mfmsr	0\n"
		"std	0, 264(%0)\n"
		"mfctr	0\n"
		"std	0, 280(%0)\n"
		"mflr	0\n"
		"std	0, 288(%0)\n"
		"bl	1f\n"
	"1:	 mflr	5\n"
		"std	5, 256(%0)\n"
		"mtlr	0\n"
		"mfxer	0\n"
		"std	0, 296(%0)\n"
			      : : "b" (&regs));
}

extern volatile int dump_in_progress;
extern dump_header_asm_t dump_header_asm;

#ifdef CONFIG_SMP
extern void dump_send_ipi(int (*dump_ipi_callback)(struct pt_regs *));
#else
#define dump_send_ipi()
#endif
#endif /* __KERNEL__ */

#endif /* _ASM_DUMP_H */
