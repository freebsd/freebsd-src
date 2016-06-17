/*
 * balo.c: BAget LOader
 *
 * Copyright (C) 1998 Gleb Raiko & Vladimir Roganov
 */
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/ptrace.h>
#include <asm/addrspace.h>

#include <asm/baget/baget.h>

#include "balo.h"  /* Includes some kernel symbol values */

static char *banner = "\nBaget Linux Loader v0.2\n";

static void mem_move (long *to, long *from, long size)
{
	while (size > 0) {
		*to++ = *from++;
		size -= sizeof(long);
	}
}

static volatile int *mem_limit     = (volatile int*)KSEG1;
static volatile int *mem_limit_dbe = (volatile int*)KSEG1;

static int can_write (volatile int* p) {
        return p <  (int*)(KSEG1+BALO_OFFSET) ||
               p >= (int*)(KSEG1+BALO_OFFSET+BALO_SIZE);
}

static volatile enum balo_state_enum {
	BALO_INIT,
	MEM_INIT,
	MEM_PROBE,
	START_KERNEL
} balo_state = BALO_INIT;


static __inline__ void reset_and_jump(int start, int mem_upper)
{
	unsigned long tmp;

	__asm__ __volatile__(
                ".set\tnoreorder\n\t"
                ".set\tnoat\n\t"
                "mfc0\t$1, $12\n\t"
                "nop\n\t"
                "nop\n\t"
                "nop\n\t"
                "ori\t$1, $1, 0xff00\n\t"
                "xori\t$1, $1, 0xff00\n\t"
                "mtc0\t$1, $12\n\t"
                "nop\n\t"
                "nop\n\t"
                "nop\n\t"
		"move\t%0, %2\n\t"
		"jr\t%1\n\t"
		"nop\n\t"
                ".set\tat\n\t"
                ".set\treorder"
                : "=&r" (tmp)
                : "Ir" (start), "Ir" (mem_upper)
                : "memory");
}

static void start_kernel(void)
{
	extern char _vmlinux_start, _vmlinux_end;
	extern char _ramdisk_start, _ramdisk_end;

        outs( "Relocating Linux... " );
	mem_move((long*)KSEG0, (long*)&_vmlinux_start,
                 &_vmlinux_end-&_vmlinux_start);
	outs("done.\n");

	if (&_ramdisk_start != &_ramdisk_end) {
		outs("Setting up RAMDISK... ");
		if (*(unsigned long*)RAMDISK_BASE != 0xBA) {
			outs("Bad RAMDISK_BASE signature in system image.\n");
                        balo_hungup();
		}
		*(unsigned long*)RAMDISK_BASE = (unsigned long)&_ramdisk_start;
		*(unsigned long*)RAMDISK_SIZE = &_ramdisk_end -&_ramdisk_start;
		outs("done.\n");
	}

	{
		extern void flush_cache_low(int isize, int dsize);
		flush_cache_low(256*1024,256*1024);
	}

        balo_printf( "Kernel entry: %x\n\n", START);
	balo_state = START_KERNEL;
	reset_and_jump(START, (int)mem_limit-KSEG1+KSEG0);
}


static void mem_probe(void)
{
	balo_state = MEM_PROBE;
	outs("RAM: <");
	while(mem_limit < mem_limit_dbe) {
                if (can_write(mem_limit) && *mem_limit != 0)
                        break; /* cycle found */
		outc('.');
		if (can_write(mem_limit))
                        *mem_limit = -1; /* mark */
                mem_limit += 0x40000;
	}
	outs(">\n");
	start_kernel();
}

volatile unsigned int int_cause;
volatile unsigned int epc;
volatile unsigned int badvaddr;

static void print_regs(void)
{
        balo_printf("CAUSE=%x EPC=%x BADVADDR=%x\n",
                    int_cause, epc, badvaddr);
}

void int_handler(struct pt_regs *regs)
{
        switch (balo_state) {
	case BALO_INIT:
                balo_printf("\nBALO: trap in balo itself.\n");
		print_regs();
                balo_hungup();
		break;
	case MEM_INIT:
                if ((int_cause & CAUSE_MASK) != CAUSE_DBE) {
                        balo_printf("\nBALO: unexpected trap during memory init.\n");
			print_regs();
                        balo_hungup();
		} else {
			mem_probe();
		}
		break;
	case MEM_PROBE:
                balo_printf("\nBALO: unexpected trap during memory probe.\n");
		print_regs();
                balo_hungup();
		break;
	case START_KERNEL:
                balo_printf("\nBALO: unexpected kernel trap.\n");
		print_regs();
                balo_hungup();
		break;
	}
        balo_printf("\nBALO: unexpected return from handler.\n");
	print_regs();
        balo_hungup();
}

static void mem_init(void)
{
	balo_state = MEM_INIT;

	while(1) {
		*mem_limit_dbe;
		if (can_write(mem_limit_dbe))
			*mem_limit_dbe = 0;

		mem_limit_dbe += 0x40000; /* +1M */
	}
        /*  no return: must go to int_handler */
}

void balo_entry(void)
{
        extern void except_vec3_generic(void);

	cli();
	outs(banner);
        memcpy((void *)(KSEG0 + 0x80), &except_vec3_generic, 0x80);
	mem_init();
}

/* Needed for linking */

int vsprintf(char *buf, const char *fmt, va_list arg)
{
	outs("BALO: vsprintf called.\n");
	balo_hungup();
	return 0;
}
