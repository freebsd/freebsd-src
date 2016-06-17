/*
 * Routines providing a simple monitor for use on the PowerMac.
 *
 * Copyright (C) 1996 Paul Mackerras.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/mm.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <asm/ptrace.h>
#include <asm/string.h>
#include <asm/prom.h>
#include <asm/machdep.h>
#include <asm/processor.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/mmu_context.h>
#include <asm/naca.h>
#include <asm/paca.h>
#include <asm/ppcdebug.h>
#include <asm/cputable.h>
#include "nonstdio.h"
#include "privinst.h"

#define scanhex	xmon_scanhex
#define skipbl	xmon_skipbl

#ifdef CONFIG_SMP
static volatile unsigned long cpus_in_xmon = 0;
static volatile unsigned long got_xmon = 0;
static volatile int take_xmon = -1;
static volatile int leaving_xmon = 0;

#endif /* CONFIG_SMP */

static unsigned long adrs;
static int size = 1;
static unsigned long ndump = 64;
static unsigned long nidump = 16;
static unsigned long ncsum = 4096;
static int termch;

static u_int bus_error_jmp[100];
#define setjmp xmon_setjmp
#define longjmp xmon_longjmp

/* Max number of stack frames we are willing to produce on a backtrace. */
#define MAXFRAMECOUNT 50

/* Breakpoint stuff */
struct bpt {
	unsigned long address;
	unsigned instr;
	unsigned long count;
	unsigned char enabled;
	char funcname[64];	/* function name for humans */
};

#define NBPTS	16
static struct bpt bpts[NBPTS];
static struct bpt dabr;
static struct bpt iabr;
static unsigned bpinstr = 0x7fe00008;	/* trap */

/* Prototypes */
extern void (*debugger_fault_handler)(struct pt_regs *);
static int cmds(struct pt_regs *);
static int mread(unsigned long, void *, int);
static int mwrite(unsigned long, void *, int);
static void handle_fault(struct pt_regs *);
static void byterev(unsigned char *, int);
static void memex(void);
static int bsesc(void);
static void dump(void);
static void prdump(unsigned long, long);
#ifdef __MWERKS__
static void prndump(unsigned, int);
static int nvreadb(unsigned);
#endif
static int ppc_inst_dump(unsigned long, long);
void print_address(unsigned long);
static int getsp(void);
static void dump_hash_table(void);
static void backtrace(struct pt_regs *);
static void excprint(struct pt_regs *);
static void prregs(struct pt_regs *);
static void memops(int);
static void memlocate(void);
static void memzcan(void);
static void memdiffs(unsigned char *, unsigned char *, unsigned, unsigned);
int skipbl(void);
int scanhex(unsigned long *valp);
static void scannl(void);
static int hexdigit(int);
void getstring(char *, int);
static void flush_input(void);
static int inchar(void);
static void take_input(char *);
/* static void openforth(void); */
static unsigned long read_spr(int);
static void write_spr(int, unsigned long);
static void super_regs(void);
static void print_sysmap(void);
static void remove_bpts(void);
static void insert_bpts(void);
static struct bpt *at_breakpoint(unsigned long pc);
static void bpt_cmds(void);
static void cacheflush(void);
#ifdef CONFIG_SMP
static void cpu_cmd(void);
#endif /* CONFIG_SMP */
static void csum(void);
static void bootcmds(void);
static void mem_translate(void);
static void mem_check(void);
static void mem_find_real(void);
static void mem_find_vsid(void);

static void debug_trace(void);

extern int print_insn_big_powerpc(FILE *, unsigned long, unsigned long);
extern void printf(const char *fmt, ...);
extern void xmon_vfprintf(void *f, const char *fmt, va_list ap);
extern int xmon_putc(int c, void *f);
extern int putchar(int ch);
extern int xmon_read_poll(void);
extern int setjmp(u_int *);
extern void longjmp(u_int *, int);
extern unsigned long _ASR;

pte_t *find_linux_pte(pgd_t *pgdir, unsigned long va);	/* from htab.c */

#define GETWORD(v)	(((v)[0] << 24) + ((v)[1] << 16) + ((v)[2] << 8) + (v)[3])

#define isxdigit(c)	(('0' <= (c) && (c) <= '9') \
			 || ('a' <= (c) && (c) <= 'f') \
			 || ('A' <= (c) && (c) <= 'F'))
#define isalnum(c)	(('0' <= (c) && (c) <= '9') \
			 || ('a' <= (c) && (c) <= 'z') \
			 || ('A' <= (c) && (c) <= 'Z'))
#define isspace(c)	(c == ' ' || c == '\t' || c == 10 || c == 13 || c == 0)

static char *help_string = "\
Commands:\n\
  b	show breakpoints\n\
  bd	set data breakpoint\n\
  bi	set instruction breakpoint\n\
  bc	clear breakpoint\n\
  d	dump bytes\n\
  di	dump instructions\n\
  df	dump float values\n\
  dd	dump double values\n\
  e	print exception information\n\
  f	flush cache\n\
  h	dump hash table\n\
  m	examine/change memory\n\
  mm	move a block of memory\n\
  ms	set a block of memory\n\
  md	compare two blocks of memory\n\
  ml	locate a block of memory\n\
  mz	zero a block of memory\n\
  mx	translation information for an effective address\n\
  mi	show information about memory allocation\n\
  M	print System.map\n\
  p 	show the task list\n\
  r	print registers\n\
  s	single step\n\
  S	print special registers\n\
  t	print backtrace\n\
  T	Enable/Disable PPCDBG flags\n\
  x	exit monitor\n\
";

static int xmon_trace[NR_CPUS];
#define SSTEP	1		/* stepping because of 's' command */
#define BRSTEP	2		/* stepping over breakpoint */

static struct pt_regs *xmon_regs[NR_CPUS];

/*
 * Stuff for reading and writing memory safely
 */
extern inline void sync(void)
{
	asm volatile("sync; isync");
}

/* (Ref: 64-bit PowerPC ELF ABI Supplement; Ian Lance Taylor, Zembu Labs).
 A PPC stack frame looks like this:

 High Address
    Back Chain
    FP reg save area
    GP reg save area
    Local var space
    Parameter save area		(SP+48)
    TOC save area		(SP+40)
    link editor doubleword	(SP+32)
    compiler doubleword		(SP+24)
    LR save			(SP+16)
    CR save			(SP+8)
    Back Chain			(SP+0)

 Note that the LR (ret addr) may not be saved in the current frame if
 no functions have been called from the current function.
 */

/*
 A traceback table typically follows each function.
 The find_tb_table() func will fill in this struct.  Note that the struct
 is not an exact match with the encoded table defined by the ABI.  It is
 defined here more for programming convenience.
 */
struct tbtable {
	unsigned long	flags;		/* flags: */
#define TBTAB_FLAGSGLOBALLINK	(1L<<47)
#define TBTAB_FLAGSISEPROL	(1L<<46)
#define TBTAB_FLAGSHASTBOFF	(1L<<45)
#define TBTAB_FLAGSINTPROC	(1L<<44)
#define TBTAB_FLAGSHASCTL	(1L<<43)
#define TBTAB_FLAGSTOCLESS	(1L<<42)
#define TBTAB_FLAGSFPPRESENT	(1L<<41)
#define TBTAB_FLAGSNAMEPRESENT	(1L<<38)
#define TBTAB_FLAGSUSESALLOCA	(1L<<37)
#define TBTAB_FLAGSSAVESCR	(1L<<33)
#define TBTAB_FLAGSSAVESLR	(1L<<32)
#define TBTAB_FLAGSSTORESBC	(1L<<31)
#define TBTAB_FLAGSFIXUP	(1L<<30)
#define TBTAB_FLAGSPARMSONSTK	(1L<<0)
	unsigned char	fp_saved;	/* num fp regs saved f(32-n)..f31 */
	unsigned char	gpr_saved;	/* num gpr's saved */
	unsigned char	fixedparms;	/* num fixed point parms */
	unsigned char	floatparms;	/* num float parms */
	unsigned char	parminfo[32];	/* types of args.  null terminated */
#define TBTAB_PARMFIXED 1
#define TBTAB_PARMSFLOAT 2
#define TBTAB_PARMDFLOAT 3
	unsigned int	tb_offset;	/* offset from start of func */
	unsigned long	funcstart;	/* addr of start of function */
	char		name[64];	/* name of function (null terminated)*/
};
static int find_tb_table(unsigned long codeaddr, struct tbtable *tab);

void
xmon(struct pt_regs *excp)
{
	struct pt_regs regs;
	int cmd = 0;
	unsigned long msr;

	if (excp == NULL) {
		/* Ok, grab regs as they are now.
		 This won't do a particularly good job because the
		 prologue has already been executed.
		 ToDo: We could reach back into the callers save
		 area to do a better job of representing the
		 caller's state.
		 */
		asm volatile ("std	0,0(%0)\n\
			std	1,8(%0)\n\
			std	2,16(%0)\n\
			std	3,24(%0)\n\
			std	4,32(%0)\n\
			std	5,40(%0)\n\
			std	6,48(%0)\n\
			std	7,56(%0)\n\
			std	8,64(%0)\n\
			std	9,72(%0)\n\
			std	10,80(%0)\n\
			std	11,88(%0)\n\
			std	12,96(%0)\n\
			std	13,104(%0)\n\
			std	14,112(%0)\n\
			std	15,120(%0)\n\
			std	16,128(%0)\n\
			std	17,136(%0)\n\
			std	18,144(%0)\n\
			std	19,152(%0)\n\
			std	20,160(%0)\n\
			std	21,168(%0)\n\
			std	22,176(%0)\n\
			std	23,184(%0)\n\
			std	24,192(%0)\n\
			std	25,200(%0)\n\
			std	26,208(%0)\n\
			std	27,216(%0)\n\
			std	28,224(%0)\n\
			std	29,232(%0)\n\
			std	30,240(%0)\n\
			std	31,248(%0)" : : "b" (&regs));
		/* Fetch the link reg for this stack frame.
		 NOTE: the prev printf fills in the lr. */
		regs.nip = regs.link = ((unsigned long *)(regs.gpr[1]))[2];
		regs.msr = get_msr();
		regs.ctr = get_ctr();
		regs.xer = get_xer();
		regs.ccr = get_cr();
		regs.trap = 0;
		excp = &regs;
	}

	msr = get_msr();
	set_msrd(msr & ~MSR_EE);	/* disable interrupts */
	xmon_regs[smp_processor_id()] = excp;
	excprint(excp);
#ifdef CONFIG_SMP
	/* possible race condition here if a CPU is held up and gets
	 * here while we are exiting */
	leaving_xmon = 0;
	if (test_and_set_bit(smp_processor_id(), &cpus_in_xmon)) {
		/* xmon probably caused an exception itself */
		printf("We are already in xmon\n");
		for (;;)
			;
	}
	while (test_and_set_bit(0, &got_xmon)) {
		if (take_xmon == smp_processor_id()) {
			take_xmon = -1;
			break;
		}
	}
	/*
	 * XXX: breakpoints are removed while any cpu is in xmon
	 */
#endif /* CONFIG_SMP */
	remove_bpts();
	cmd = cmds(excp);
	if (cmd == 's') {
		xmon_trace[smp_processor_id()] = SSTEP;
		excp->msr |= MSR_SE;
#ifdef CONFIG_SMP		
		take_xmon = smp_processor_id();
#endif		
	} else if (at_breakpoint(excp->nip)) {
		xmon_trace[smp_processor_id()] = BRSTEP;
		excp->msr |= MSR_SE;
	} else {
		xmon_trace[smp_processor_id()] = 0;
		insert_bpts();
	}
	xmon_regs[smp_processor_id()] = 0;
#ifdef CONFIG_SMP
	leaving_xmon = 1;
	if (cmd != 's')
		clear_bit(0, &got_xmon);
	clear_bit(smp_processor_id(), &cpus_in_xmon);
#endif /* CONFIG_SMP */
	set_msrd(msr);		/* restore interrupt enable */
}

void
xmon_irq(int irq, void *d, struct pt_regs *regs)
{
	unsigned long flags;
	__save_flags(flags);
	__cli();
	printf("Keyboard interrupt\n");
	xmon(regs);
	__restore_flags(flags);
}

int
xmon_bpt(struct pt_regs *regs)
{
	struct bpt *bp;

	bp = at_breakpoint(regs->nip);
	if (!bp)
		return 0;
	if (bp->count) {
		--bp->count;
		remove_bpts();
		excprint(regs);
		xmon_trace[smp_processor_id()] = BRSTEP;
		regs->msr |= MSR_SE;
	} else {
		printf("Stopped at breakpoint %x (%lx %s)\n", (bp - bpts)+1, bp->address, bp->funcname);
		xmon(regs);
	}
	return 1;
}

int
xmon_sstep(struct pt_regs *regs)
{
	if (!xmon_trace[smp_processor_id()])
		return 0;
	if (xmon_trace[smp_processor_id()] == BRSTEP) {
		xmon_trace[smp_processor_id()] = 0;
		insert_bpts();
	} else {
		xmon(regs);
	}
	return 1;
}

int
xmon_dabr_match(struct pt_regs *regs)
{
	if (dabr.enabled && dabr.count) {
		--dabr.count;
		remove_bpts();
		excprint(regs);
		xmon_trace[smp_processor_id()] = BRSTEP;
		regs->msr |= MSR_SE;
	} else {
		dabr.instr = regs->nip;
		xmon(regs);
	}
	return 1;
}

int
xmon_iabr_match(struct pt_regs *regs)
{
	if (iabr.enabled && iabr.count) {
		--iabr.count;
		remove_bpts();
		excprint(regs);
		xmon_trace[smp_processor_id()] = BRSTEP;
		regs->msr |= MSR_SE;
	} else {
		xmon(regs);
	}
	return 1;
}

static struct bpt *
at_breakpoint(unsigned long pc)
{
	int i;
	struct bpt *bp;

	if (dabr.enabled && pc == dabr.instr)
		return &dabr;
	if (iabr.enabled && pc == iabr.address)
		return &iabr;
	bp = bpts;
	for (i = 0; i < NBPTS; ++i, ++bp)
		if (bp->enabled && pc == bp->address)
			return bp;
	return 0;
}

static void
insert_bpts()
{
	int i;
	struct bpt *bp;

	if (!(systemcfg->platform & PLATFORM_PSERIES))
		return;
	bp = bpts;
	for (i = 0; i < NBPTS; ++i, ++bp) {
		if (!bp->enabled)
			continue;
		if (mread(bp->address, &bp->instr, 4) != 4
		    || mwrite(bp->address, &bpinstr, 4) != 4) {
			printf("Couldn't insert breakpoint at %x, disabling\n",
			       bp->address);
			bp->enabled = 0;
		} else {
			store_inst((void *)bp->address);
		}
	}

	if (!(cur_cpu_spec->cpu_features & CPU_FTR_SLB)) {
		if (dabr.enabled)
			set_dabr(dabr.address);
		if (iabr.enabled)
			set_iabr(iabr.address);
	}
}

static void
remove_bpts()
{
	int i;
	struct bpt *bp;
	unsigned instr;

	if (!(systemcfg->platform & PLATFORM_PSERIES))
		return;
	if (!(cur_cpu_spec->cpu_features & CPU_FTR_SLB)) {
		if (dabr.enabled)
			set_dabr(0);
		if (iabr.enabled)
			set_iabr(0);
	}

	bp = bpts;
	for (i = 0; i < NBPTS; ++i, ++bp) {
		if (!bp->enabled)
			continue;
		if (mread(bp->address, &instr, 4) == 4
		    && instr == bpinstr
		    && mwrite(bp->address, &bp->instr, 4) != 4)
			printf("Couldn't remove breakpoint at %x\n",
			       bp->address);
		else
			store_inst((void *)bp->address);
	}
}

static char *last_cmd;

/* Command interpreting routine */
static int
cmds(struct pt_regs *excp)
{
	int cmd = 0;

	last_cmd = NULL;
	for(;;) {
#ifdef CONFIG_SMP
		/* Need to check if we should take any commands on
		   this CPU. */
		if (leaving_xmon)
			return cmd;
		printf("%d:", smp_processor_id());
#endif /* CONFIG_SMP */
		printf("mon> ");
		fflush(stdout);
		flush_input();
		termch = 0;
		cmd = skipbl();
		if( cmd == '\n' ) {
			if (last_cmd == NULL)
				continue;
			take_input(last_cmd);
			last_cmd = NULL;
			cmd = inchar();
		}
		switch (cmd) {
		case 'm':
			cmd = inchar();
			switch (cmd) {
			case 'm':
			case 's':
			case 'd':
				memops(cmd);
				break;
			case 'l':
				memlocate();
				break;
			case 'z':
				memzcan();
				break;
			case 'x':
				mem_translate();
				break;
			case 'c':
				mem_check();
				break;
			case 'f':
				mem_find_real();
				break;
			case 'e':
				mem_find_vsid();
				break;
			case 'i':
				show_mem();
				break;
			default:
				termch = cmd;
				memex();
			}
			break;
		case 'd':
			dump();
			break;
		case 'r':
			if (excp != NULL)
				prregs(excp);	/* print regs */
			break;
		case 'e':
			if (excp == NULL)
				printf("No exception information\n");
			else
				excprint(excp);
			break;
		case 'M':
			print_sysmap();
			break;
		case 'S':
			super_regs();
			break;
		case 't':
			backtrace(excp);
			break;
		case 'f':
			cacheflush();
			break;
		case 'h':
			dump_hash_table();
			break;
		case 's':
		case 'x':
		case EOF:
			return cmd;
		case '?':
			printf(help_string);
			break;
		case 'p':
			show_state();
			break;
		case 'b':
			bpt_cmds();
			break;
		case 'C':
			csum();
			break;
#ifdef CONFIG_SMP
		case 'c':
			cpu_cmd();
			break;
#endif /* CONFIG_SMP */
		case 'z':
			bootcmds();
		case 'T':
			debug_trace();
			break;
		default:
			printf("Unrecognized command: ");
		        do {
				if( ' ' < cmd && cmd <= '~' )
					putchar(cmd);
				else
					printf("\\x%x", cmd);
				cmd = inchar();
		        } while (cmd != '\n'); 
			printf(" (type ? for help)\n");
			break;
		}
	}
}

static void bootcmds(void)
{
	int cmd;

	cmd = inchar();
	if (cmd == 'r')
		ppc_md.restart(NULL);
	else if (cmd == 'h')
		ppc_md.halt();
	else if (cmd == 'p')
		ppc_md.power_off();
}

#ifdef CONFIG_SMP
static void cpu_cmd(void)
{
	unsigned long cpu;
	int timeout;
	int cmd;

	cmd = inchar();
	if (cmd == 'i') {
		printf("stopping all cpus\n");
		/* interrupt other cpu(s) */
		cpu = MSG_ALL_BUT_SELF;
		smp_send_xmon_break(cpu);
		return;
	}
	termch = cmd;
	if (!scanhex(&cpu)) {
		/* print cpus waiting or in xmon */
		printf("cpus stopped:");
		for (cpu = 0; cpu < NR_CPUS; ++cpu) {
			if (test_bit(cpu, &cpus_in_xmon)) {
				printf(" %x", cpu);
				if (cpu == smp_processor_id())
					printf("*", cpu);
			}
		}
		printf("\n");
		return;
	}
	/* try to switch to cpu specified */
	take_xmon = cpu;
	timeout = 10000000;
	while (take_xmon >= 0) {
		if (--timeout == 0) {
			/* yes there's a race here */
			take_xmon = -1;
			printf("cpu %u didn't take control\n", cpu);
			return;
		}
	}
	/* now have to wait to be given control back */
	while (test_and_set_bit(0, &got_xmon)) {
		if (take_xmon == smp_processor_id()) {
			take_xmon = -1;
			break;
		}
	}
}
#endif /* CONFIG_SMP */

static unsigned short fcstab[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

#define FCS(fcs, c)	(((fcs) >> 8) ^ fcstab[((fcs) ^ (c)) & 0xff])

static void
csum(void)
{
	unsigned int i;
	unsigned short fcs;
	unsigned char v;

	if (!scanhex(&adrs))
		return;
	if (!scanhex(&ncsum))
		return;
	fcs = 0xffff;
	for (i = 0; i < ncsum; ++i) {
		if (mread(adrs+i, &v, 1) == 0) {
			printf("csum stopped at %x\n", adrs+i);
			break;
		}
		fcs = FCS(fcs, v);
	}
	printf("%x\n", fcs);
}

static char *breakpoint_help_string = 
    "Breakpoint command usage:\n"
    "b                show breakpoints\n"
    "b <addr> [cnt]   set breakpoint at given instr addr\n"
    "bc               clear all breakpoints\n"
    "bc <n/addr>      clear breakpoint number n or at addr\n"
    "bi <addr> [cnt]  set hardware instr breakpoint (broken?)\n"
    "bd <addr> [cnt]  set hardware data breakpoint (broken?)\n"
    "";

static void
bpt_cmds(void)
{
	int cmd;
	unsigned long a;
	int mode, i;
	struct bpt *bp;
	struct tbtable tab;

	cmd = inchar();
	switch (cmd) {
	case 'd':	/* bd - hardware data breakpoint */
		if (cur_cpu_spec->cpu_features & CPU_FTR_SLB) {
			printf("Not implemented on POWER4\n");
			break;
		}
			
		mode = 7;
		cmd = inchar();
		if (cmd == 'r')
			mode = 5;
		else if (cmd == 'w')
			mode = 6;
		else
			termch = cmd;
		dabr.address = 0;
		dabr.count = 0;
		dabr.enabled = scanhex(&dabr.address);
		scanhex(&dabr.count);
		if (dabr.enabled)
			dabr.address = (dabr.address & ~7) | mode;
		break;
	case 'i':	/* bi - hardware instr breakpoint */
		if (cur_cpu_spec->cpu_features & CPU_FTR_SLB) {
			printf("Not implemented on POWER4\n");
			break;
		}
		iabr.address = 0;
		iabr.count = 0;
		iabr.enabled = scanhex(&iabr.address);
		if (iabr.enabled)
			iabr.address |= 3;
		scanhex(&iabr.count);
		break;
	case 'c':
		if (!scanhex(&a)) {
			/* clear all breakpoints */
			for (i = 0; i < NBPTS; ++i)
				bpts[i].enabled = 0;
			iabr.enabled = 0;
			dabr.enabled = 0;
			printf("All breakpoints cleared\n");
		} else {
			if (a <= NBPTS && a >= 1) {
				/* assume a breakpoint number */
				--a;	/* bp nums are 1 based */
				bp = &bpts[a];
			} else {
				/* assume a breakpoint address */
				bp = at_breakpoint(a);
			}
			if (bp == 0) {
				printf("No breakpoint at %x\n", a);
			} else {
				printf("Cleared breakpoint %x (%lx %s)\n", (bp - bpts)+1, bp->address, bp->funcname);
				bp->enabled = 0;
			}
		}
		break;
	case '?':
	        printf(breakpoint_help_string);
	        break;
	default:
		termch = cmd;
	        cmd = skipbl();
		if (cmd == '?') {
			printf(breakpoint_help_string);
			break;
		}
		termch = cmd;
		if (!scanhex(&a)) {
			/* print all breakpoints */
			int bpnum;

			printf("   type            address    count\n");
			if (dabr.enabled) {
				printf("   data   %.16lx %8x [", dabr.address & ~7,
				       dabr.count);
				if (dabr.address & 1)
					printf("r");
				if (dabr.address & 2)
					printf("w");
				printf("]\n");
			}
			if (iabr.enabled)
				printf("   inst   %.16lx %8x\n", iabr.address & ~3,
				       iabr.count);
			for (bp = bpts, bpnum = 1; bp < &bpts[NBPTS]; ++bp, ++bpnum)
				if (bp->enabled)
					printf("%2x trap   %.16lx %8x  %s\n", bpnum, bp->address, bp->count, bp->funcname);
			break;
		}
		bp = at_breakpoint(a);
		if (bp == 0) {
			for (bp = bpts; bp < &bpts[NBPTS]; ++bp)
				if (!bp->enabled)
					break;
			if (bp >= &bpts[NBPTS]) {
				printf("Sorry, no free breakpoints.  Please clear one first.\n");
				break;
			}
		}
		bp->enabled = 1;
		bp->address = a;
		bp->count = 0;
		scanhex(&bp->count);
		/* Find the function name just once. */
		bp->funcname[0] = '\0';
		if (find_tb_table(bp->address, &tab) && tab.name[0]) {
			/* Got a nice name for it. */
			int delta = bp->address - tab.funcstart;
			sprintf(bp->funcname, "%s+0x%x", tab.name, delta);
		}
		printf("Set breakpoint %2x trap   %.16lx %8x  %s\n", (bp-bpts)+1, bp->address, bp->count, bp->funcname);
		break;
	}
}

/* Very cheap human name for vector lookup. */
static
const char *getvecname(unsigned long vec)
{
	char *ret;
	switch (vec) {
	case 0x100:	ret = "(System Reset)"; break; 
	case 0x200:	ret = "(Machine Check)"; break; 
	case 0x300:	ret = "(Data Access)"; break; 
	case 0x380:	ret = "(Data SLB Access)"; break;
	case 0x400:	ret = "(Instruction Access)"; break; 
	case 0x480:	ret = "(Instruction SLB Access)"; break;
	case 0x500:	ret = "(Hardware Interrupt)"; break; 
	case 0x600:	ret = "(Alignment)"; break; 
	case 0x700:	ret = "(Program Check)"; break; 
	case 0x800:	ret = "(FPU Unavailable)"; break; 
	case 0x900:	ret = "(Decrementer)"; break; 
	case 0xc00:	ret = "(System Call)"; break; 
	case 0xd00:	ret = "(Single Step)"; break; 
	case 0xf00:	ret = "(Performance Monitor)"; break; 
	default: ret = "";
	}
	return ret;
}

static void
backtrace(struct pt_regs *excp)
{
	unsigned long sp;
	unsigned long lr;
	unsigned long stack[3];
	struct pt_regs regs;
	struct tbtable tab;
	int framecount;
	char *funcname;
	/* declare these as raw ptrs so we don't get func descriptors */
	extern void *ret_from_except, *ret_from_syscall_1;

	if (excp != NULL) {
	        lr = excp->link;
		sp = excp->gpr[1];
	} else {
	        /* Use care not to call any function before this point
		 so the saved lr has a chance of being good. */
	        asm volatile ("mflr %0" : "=r" (lr) :);
		sp = getsp();
	}
	scanhex(&sp);
	scannl();
	for (framecount = 0;
	     sp != 0 && framecount < MAXFRAMECOUNT;
	     sp = stack[0], framecount++) {
		if (mread(sp, stack, sizeof(stack)) != sizeof(stack))
			break;
#if 0
		if (lr != 0) {
		    stack[2] = lr;	/* fake out the first saved lr.  It may not be saved yet. */
		    lr = 0;
		}
#endif
		printf("%.16lx  %.16lx", sp, stack[2]);
		/* TAI -- for now only the ones cast to unsigned long will match.
		 * Need to test the rest...
		 */
		if ((stack[2] == (unsigned long)ret_from_except &&
		            (funcname = "ret_from_except"))
		    || (stack[2] == (unsigned long)ret_from_syscall_1 &&
		            (funcname = "ret_from_syscall_1"))
#if 0
		    || stack[2] == (unsigned) &ret_from_syscall_2
		    || stack[2] == (unsigned) &do_signal_ret
#endif
		    ) {
			printf("  %s\n", funcname);
			if (mread(sp+112, &regs, sizeof(regs)) != sizeof(regs))
				break;
			printf("exception: %lx %s regs %lx\n", regs.trap, getvecname(regs.trap), sp+112);
			printf("                  %.16lx", regs.nip);
			if ((regs.nip & 0xffffffff00000000UL) &&
			    find_tb_table(regs.nip, &tab)) {
				int delta = regs.nip-tab.funcstart;
				if (delta < 0)
					printf("  <unknown code>");
				else
					printf("  %s+0x%x", tab.name, delta);
			}
			printf("\n");
                        if (regs.gpr[1] < sp) {
                            printf("<Stack drops into 32-bit userspace %.16lx>\n", regs.gpr[1]);
                            break;
			}

			sp = regs.gpr[1];
			if (mread(sp, stack, sizeof(stack)) != sizeof(stack))
				break;
		} else {
			if (stack[2] && find_tb_table(stack[2], &tab)) {
				int delta = stack[2]-tab.funcstart;
				if (delta < 0)
					printf("  <unknown code>");
				else
					printf("  %s+0x%x", tab.name, delta);
			}
			printf("\n");
		}
		if (stack[0] && stack[0] <= sp) {
			if ((stack[0] & 0xffffffff00000000UL) == 0)
				printf("<Stack drops into 32-bit userspace %.16lx>\n", stack[0]);
			else
				printf("<Corrupt stack.  Next backchain is %.16lx>\n", stack[0]);
			break;
		}
	}
	if (framecount >= MAXFRAMECOUNT)
		printf("<Punt. Too many stack frames>\n");
}

int
getsp()
{
	int x;

	asm("mr %0,1" : "=r" (x) :);
	return x;
}

spinlock_t exception_print_lock = SPIN_LOCK_UNLOCKED;

void
excprint(struct pt_regs *fp)
{
	struct task_struct *c;
	struct tbtable tab;
	unsigned long flags;

	spin_lock_irqsave(&exception_print_lock, flags);

#ifdef CONFIG_SMP
	printf("cpu %d: ", smp_processor_id());
#endif /* CONFIG_SMP */

	printf("Vector: %lx %s at  [%lx]\n", fp->trap, getvecname(fp->trap), fp);
	printf("    pc: %lx", fp->nip);
	if (find_tb_table(fp->nip, &tab) && tab.name[0]) {
		/* Got a nice name for it */
		int delta = fp->nip - tab.funcstart;
		printf(" (%s+0x%x)", tab.name, delta);
	}
	printf("\n");
	printf("    lr: %lx", fp->link);
	if (find_tb_table(fp->link, &tab) && tab.name[0]) {
		/* Got a nice name for it */
		int delta = fp->link - tab.funcstart;
		printf(" (%s+0x%x)", tab.name, delta);
	}
	printf("\n");
	printf("    sp: %lx\n", fp->gpr[1]);
	printf("   msr: %lx\n", fp->msr);

	if (fp->trap == 0x300 || fp->trap == 0x380 || fp->trap == 0x600) {
		printf("   dar: %lx\n", fp->dar);
		printf(" dsisr: %lx\n", fp->dsisr);
	}

	/* XXX: need to copy current or we die.  Why? */
	c = current;
	printf("  current = 0x%lx\n", c);
	printf("  paca    = 0x%lx\n", get_paca());
	if (c) {
		printf("  current = %lx, pid = %ld, comm = %s\n",
		       c, c->pid, c->comm);
	}

	spin_unlock_irqrestore(&exception_print_lock, flags);
}

void
prregs(struct pt_regs *fp)
{
	int n;
	unsigned long base;

	if (scanhex((void *)&base))
		fp = (struct pt_regs *) base;
	for (n = 0; n < 16; ++n)
		printf("R%.2ld = %.16lx   R%.2ld = %.16lx\n", n, fp->gpr[n],
		       n+16, fp->gpr[n+16]);
	printf("pc  = %.16lx   msr = %.16lx\nlr  = %.16lx   cr  = %.16lx\n",
	       fp->nip, fp->msr, fp->link, fp->ccr);
	printf("ctr = %.16lx   xer = %.16lx   trap = %8lx\n",
	       fp->ctr, fp->xer, fp->trap);
}

void
cacheflush(void)
{
	int cmd;
	unsigned long nflush;

	cmd = inchar();
	if (cmd != 'i')
		termch = cmd;
	scanhex((void *)&adrs);
	if (termch != '\n')
		termch = 0;
	nflush = 1;
	scanhex(&nflush);
	nflush = (nflush + 31) / 32;
	if (cmd != 'i') {
		for (; nflush > 0; --nflush, adrs += 0x20)
			cflush((void *) adrs);
	} else {
		for (; nflush > 0; --nflush, adrs += 0x20)
			cinval((void *) adrs);
	}
}

unsigned long
read_spr(int n)
{
	unsigned int instrs[2];
	unsigned long (*code)(void);
	unsigned long opd[3];

	instrs[0] = 0x7c6002a6 + ((n & 0x1F) << 16) + ((n & 0x3e0) << 6);
	instrs[1] = 0x4e800020;
	opd[0] = (unsigned long)instrs;
	opd[1] = 0;
	opd[2] = 0;
	store_inst(instrs);
	store_inst(instrs+1);
	code = (unsigned long (*)(void)) opd;

	return code();
}

void
write_spr(int n, unsigned long val)
{
	unsigned int instrs[2];
	unsigned long (*code)(unsigned long);
	unsigned long opd[3];

	instrs[0] = 0x7c6003a6 + ((n & 0x1F) << 16) + ((n & 0x3e0) << 6);
	instrs[1] = 0x4e800020;
	opd[0] = (unsigned long)instrs;
	opd[1] = 0;
	opd[2] = 0;
	store_inst(instrs);
	store_inst(instrs+1);
	code = (unsigned long (*)(unsigned long)) opd;

	code(val);
}

static unsigned long regno;
extern char exc_prolog;
extern char dec_exc;

void
print_sysmap(void)
{
	extern char *sysmap;
	if ( sysmap )
		printf("System.map: \n%s", sysmap);
}

void
super_regs()
{
	int i, cmd;
	unsigned long val;
	struct paca_struct*  ptrPaca = NULL;
	struct ItLpPaca*  ptrLpPaca = NULL;
	struct ItLpRegSave*  ptrLpRegSave = NULL;

	cmd = skipbl();
	if (cmd == '\n') {
	        unsigned long sp, toc;
		asm("mr %0,1" : "=r" (sp) :);
		asm("mr %0,2" : "=r" (toc) :);

		printf("msr  = %.16lx  sprg0= %.16lx\n", get_msr(), get_sprg0());
		printf("pvr  = %.16lx  sprg1= %.16lx\n", get_pvr(), get_sprg1()); 
		printf("dec  = %.16lx  sprg2= %.16lx\n", get_dec(), get_sprg2());
		printf("sp   = %.16lx  sprg3= %.16lx\n", sp, get_sprg3());
		printf("toc  = %.16lx  dar  = %.16lx\n", toc, get_dar());
		printf("srr0 = %.16lx  srr1 = %.16lx\n", get_srr0(), get_srr1());
		printf("asr  = %.16lx\n", mfasr());
		for (i = 0; i < 8; ++i)
			printf("sr%.2ld = %.16lx  sr%.2ld = %.16lx\n", i, get_sr(i), i+8, get_sr(i+8));

		// Dump out relevant Paca data areas.
		printf("Paca: \n");
		ptrPaca = get_paca();
    
		printf("  Local Processor Control Area (LpPaca): \n");
		ptrLpPaca = ptrPaca->xLpPacaPtr;
		printf("    Saved Srr0=%.16lx  Saved Srr1=%.16lx \n", ptrLpPaca->xSavedSrr0, ptrLpPaca->xSavedSrr1);
		printf("    Saved Gpr3=%.16lx  Saved Gpr4=%.16lx \n", ptrLpPaca->xSavedGpr3, ptrLpPaca->xSavedGpr4);
		printf("    Saved Gpr5=%.16lx \n", ptrLpPaca->xSavedGpr5);
    
		printf("  Local Processor Register Save Area (LpRegSave): \n");
		ptrLpRegSave = ptrPaca->xLpRegSavePtr;
		printf("    Saved Sprg0=%.16lx  Saved Sprg1=%.16lx \n", ptrLpRegSave->xSPRG0, ptrLpRegSave->xSPRG0);
		printf("    Saved Sprg2=%.16lx  Saved Sprg3=%.16lx \n", ptrLpRegSave->xSPRG2, ptrLpRegSave->xSPRG3);
		printf("    Saved Msr  =%.16lx  Saved Nia  =%.16lx \n", ptrLpRegSave->xMSR, ptrLpRegSave->xNIA);
    
		return;
	}

	scanhex(&regno);
	switch (cmd) {
	case 'w':
		val = read_spr(regno);
		scanhex(&val);
		write_spr(regno, val);
		/* fall through */
	case 'r':
		printf("spr %lx = %lx\n", regno, read_spr(regno));
		break;
	case 's':
		val = get_sr(regno);
		scanhex(&val);
		set_sr(regno, val);
		break;
	case 'm':
		val = get_msr();
		scanhex(&val);
		set_msrd(val);
		break;
	}
	scannl();
}

#ifndef CONFIG_PPC64BRIDGE
static void
dump_hash_table_seg(unsigned seg, unsigned start, unsigned end)
{
	extern void *Hash;
	extern unsigned long Hash_size;
	unsigned *htab = Hash;
	unsigned hsize = Hash_size;
	unsigned v, hmask, va, last_va;
	int found, last_found, i;
	unsigned *hg, w1, last_w2, last_va0;

	last_found = 0;
	hmask = hsize / 64 - 1;
	va = start;
	start = (start >> 12) & 0xffff;
	end = (end >> 12) & 0xffff;
	for (v = start; v < end; ++v) {
		found = 0;
		hg = htab + (((v ^ seg) & hmask) * 16);
		w1 = 0x80000000 | (seg << 7) | (v >> 10);
		for (i = 0; i < 8; ++i, hg += 2) {
			if (*hg == w1) {
				found = 1;
				break;
			}
		}
		if (!found) {
			w1 ^= 0x40;
			hg = htab + ((~(v ^ seg) & hmask) * 16);
			for (i = 0; i < 8; ++i, hg += 2) {
				if (*hg == w1) {
					found = 1;
					break;
				}
			}
		}
		if (!(last_found && found && (hg[1] & ~0x180) == last_w2 + 4096)) {
			if (last_found) {
				if (last_va != last_va0)
					printf(" ... %x", last_va);
				printf("\n");
			}
			if (found) {
				printf("%x to %x", va, hg[1]);
				last_va0 = va;
			}
			last_found = found;
		}
		if (found) {
			last_w2 = hg[1] & ~0x180;
			last_va = va;
		}
		va += 4096;
	}
	if (last_found)
		printf(" ... %x\n", last_va);
}

#else /* CONFIG_PPC64BRIDGE */
static void
dump_hash_table_seg(unsigned seg, unsigned start, unsigned end)
{
	extern void *Hash;
	extern unsigned long Hash_size;
	unsigned *htab = Hash;
	unsigned hsize = Hash_size;
	unsigned v, hmask, va, last_va;
	int found, last_found, i;
	unsigned *hg, w1, last_w2, last_va0;

	last_found = 0;
	hmask = hsize / 128 - 1;
	va = start;
	start = (start >> 12) & 0xffff;
	end = (end >> 12) & 0xffff;
	for (v = start; v < end; ++v) {
		found = 0;
		hg = htab + (((v ^ seg) & hmask) * 32);
		w1 = 1 | (seg << 12) | ((v & 0xf800) >> 4);
		for (i = 0; i < 8; ++i, hg += 4) {
			if (hg[1] == w1) {
				found = 1;
				break;
			}
		}
		if (!found) {
			w1 ^= 2;
			hg = htab + ((~(v ^ seg) & hmask) * 32);
			for (i = 0; i < 8; ++i, hg += 4) {
				if (hg[1] == w1) {
					found = 1;
					break;
				}
			}
		}
		if (!(last_found && found && (hg[3] & ~0x180) == last_w2 + 4096)) {
			if (last_found) {
				if (last_va != last_va0)
					printf(" ... %x", last_va);
				printf("\n");
			}
			if (found) {
				printf("%x to %x", va, hg[3]);
				last_va0 = va;
			}
			last_found = found;
		}
		if (found) {
			last_w2 = hg[3] & ~0x180;
			last_va = va;
		}
		va += 4096;
	}
	if (last_found)
		printf(" ... %x\n", last_va);
}
#endif /* CONFIG_PPC64BRIDGE */

static unsigned long hash_ctx;
static unsigned long hash_start;
static unsigned long hash_end;

static void
dump_hash_table()
{
	int seg;
	unsigned seg_start, seg_end;

	hash_ctx = 0;
	hash_start = 0;
	hash_end = 0xfffff000;
	scanhex(&hash_ctx);
	scanhex(&hash_start);
	scanhex(&hash_end);
	printf("Mappings for context %x\n", hash_ctx);
	seg_start = hash_start;
	for (seg = hash_start >> 28; seg <= hash_end >> 28; ++seg) {
		seg_end = (seg << 28) | 0x0ffff000;
		if (seg_end > hash_end)
			seg_end = hash_end;
		dump_hash_table_seg((hash_ctx << 4) + seg, seg_start, seg_end);
		seg_start = seg_end + 0x1000;
	}
}

int
mread(unsigned long adrs, void *buf, int size)
{
	volatile int n;
	char *p, *q;

	n = 0;
	if( setjmp(bus_error_jmp) == 0 ){
		debugger_fault_handler = handle_fault;
		sync();
		p = (char *) adrs;
		q = (char *) buf;
		switch (size) {
		case 2: *(short *)q = *(short *)p;	break;
		case 4: *(int *)q = *(int *)p;		break;
		default:
			for( ; n < size; ++n ) {
				*q++ = *p++;
				sync();
			}
		}
		sync();
		/* wait a little while to see if we get a machine check */
		__delay(200);
		n = size;
	}
	debugger_fault_handler = 0;
	return n;
}

int
mwrite(unsigned long adrs, void *buf, int size)
{
	volatile int n;
	char *p, *q;

	n = 0;
	if( setjmp(bus_error_jmp) == 0 ){
		debugger_fault_handler = handle_fault;
		sync();
		p = (char *) adrs;
		q = (char *) buf;
		switch (size) {
		case 2: *(short *)p = *(short *)q;	break;
		case 4: *(int *)p = *(int *)q;		break;
		default:
			for( ; n < size; ++n ) {
				*p++ = *q++;
				sync();
			}
		}
		sync();
		/* wait a little while to see if we get a machine check */
		__delay(200);
		n = size;
	} else {
		printf("*** Error writing address %x\n", adrs + n);
	}
	debugger_fault_handler = 0;
	return n;
}

static int fault_type;
static char *fault_chars[] = { "--", "**", "##" };

static void
handle_fault(struct pt_regs *regs)
{
	switch (regs->trap) {
	case 0x200:
		fault_type = 0;
		break;
	case 0x300:
	case 0x380:
		fault_type = 1;
		break;
	default:
		fault_type = 2;
	}

	longjmp(bus_error_jmp, 1);
}

#define SWAP(a, b, t)	((t) = (a), (a) = (b), (b) = (t))

void
byterev(unsigned char *val, int size)
{
	int t;
	
	switch (size) {
	case 2:
		SWAP(val[0], val[1], t);
		break;
	case 4:
		SWAP(val[0], val[3], t);
		SWAP(val[1], val[2], t);
		break;
	case 8: /* is there really any use for this? */
		SWAP(val[0], val[7], t);
		SWAP(val[1], val[6], t);
		SWAP(val[2], val[5], t);
		SWAP(val[3], val[4], t);
		break;
	}
}

static int brev;
static int mnoread;

static char *memex_help_string = 
    "Memory examine command usage:\n"
    "m [addr] [flags] examine/change memory\n"
    "  addr is optional.  will start where left off.\n"
    "  flags may include chars from this set:\n"
    "    b   modify by bytes (default)\n"
    "    w   modify by words (2 byte)\n"
    "    l   modify by longs (4 byte)\n"
    "    d   modify by doubleword (8 byte)\n"
    "    r   toggle reverse byte order mode\n"
    "    n   do not read memory (for i/o spaces)\n"
    "    .   ok to read (default)\n"
    "NOTE: flags are saved as defaults\n"
    "";

static char *memex_subcmd_help_string = 
    "Memory examine subcommands:\n"
    "  hexval   write this val to current location\n"
    "  'string' write chars from string to this location\n"
    "  '        increment address\n"
    "  ^        decrement address\n"
    "  /        increment addr by 0x10.  //=0x100, ///=0x1000, etc\n"
    "  \\        decrement addr by 0x10.  \\\\=0x100, \\\\\\=0x1000, etc\n"
    "  `        clear no-read flag\n"
    "  ;        stay at this addr\n"
    "  v        change to byte mode\n"
    "  w        change to word (2 byte) mode\n"
    "  l        change to long (4 byte) mode\n"
    "  u        change to doubleword (8 byte) mode\n"
    "  m addr   change current addr\n"
    "  n        toggle no-read flag\n"
    "  r        toggle byte reverse flag\n"
    "  < count  back up count bytes\n"
    "  > count  skip forward count bytes\n"
    "  x        exit this mode\n"
    "";

void
memex()
{
	int cmd, inc, i, nslash;
	unsigned long n;
	unsigned char val[16];

	scanhex((void *)&adrs);
	cmd = skipbl();
	if (cmd == '?') {
		printf(memex_help_string);
		return;
	} else {
		termch = cmd;
	}
	last_cmd = "m\n";
	while ((cmd = skipbl()) != '\n') {
		switch( cmd ){
		case 'b':	size = 1;	break;
		case 'w':	size = 2;	break;
		case 'l':	size = 4;	break;
		case 'd':	size = 8;	break;
		case 'r': 	brev = !brev;	break;
		case 'n':	mnoread = 1;	break;
		case '.':	mnoread = 0;	break;
		}
	}
	if( size <= 0 )
		size = 1;
	else if( size > 8 )
		size = 8;
	for(;;){
		if (!mnoread)
			n = mread(adrs, val, size);
		printf("%.16x%c", adrs, brev? 'r': ' ');
		if (!mnoread) {
			if (brev)
				byterev(val, size);
			putchar(' ');
			for (i = 0; i < n; ++i)
				printf("%.2x", val[i]);
			for (; i < size; ++i)
				printf("%s", fault_chars[fault_type]);
		}
		putchar(' ');
		inc = size;
		nslash = 0;
		for(;;){
			if( scanhex(&n) ){
				for (i = 0; i < size; ++i)
					val[i] = n >> (i * 8);
				if (!brev)
					byterev(val, size);
				mwrite(adrs, val, size);
				inc = size;
			}
			cmd = skipbl();
			if (cmd == '\n')
				break;
			inc = 0;
			switch (cmd) {
			case '\'':
				for(;;){
					n = inchar();
					if( n == '\\' )
						n = bsesc();
					else if( n == '\'' )
						break;
					for (i = 0; i < size; ++i)
						val[i] = n >> (i * 8);
					if (!brev)
						byterev(val, size);
					mwrite(adrs, val, size);
					adrs += size;
				}
				adrs -= size;
				inc = size;
				break;
			case ',':
				adrs += size;
				break;
			case '.':
				mnoread = 0;
				break;
			case ';':
				break;
			case 'x':
			case EOF:
				scannl();
				return;
			case 'b':
			case 'v':
				size = 1;
				break;
			case 'w':
				size = 2;
				break;
			case 'l':
				size = 4;
				break;
			case 'u':
				size = 8;
				break;
			case '^':
				adrs -= size;
				break;
				break;
			case '/':
				if (nslash > 0)
					adrs -= 1 << nslash;
				else
					nslash = 0;
				nslash += 4;
				adrs += 1 << nslash;
				break;
			case '\\':
				if (nslash < 0)
					adrs += 1 << -nslash;
				else
					nslash = 0;
				nslash -= 4;
				adrs -= 1 << -nslash;
				break;
			case 'm':
				scanhex((void *)&adrs);
				break;
			case 'n':
				mnoread = 1;
				break;
			case 'r':
				brev = !brev;
				break;
			case '<':
				n = size;
				scanhex(&n);
				adrs -= n;
				break;
			case '>':
				n = size;
				scanhex(&n);
				adrs += n;
				break;
			case '?':
				printf(memex_subcmd_help_string);
				break;
			}
		}
		adrs += inc;
	}
}

int
bsesc()
{
	int c;

	c = inchar();
	switch( c ){
	case 'n':	c = '\n';	break;
	case 'r':	c = '\r';	break;
	case 'b':	c = '\b';	break;
	case 't':	c = '\t';	break;
	}
	return c;
}

#define isxdigit(c)	(('0' <= (c) && (c) <= '9') \
			 || ('a' <= (c) && (c) <= 'f') \
			 || ('A' <= (c) && (c) <= 'F'))
void
dump()
{
	int c;

	c = inchar();
	if ((isxdigit(c) && c != 'f' && c != 'd') || c == '\n')
		termch = c;
	scanhex((void *)&adrs);
	if( termch != '\n')
		termch = 0;
	if( c == 'i' ){
		scanhex(&nidump);
		if( nidump == 0 )
			nidump = 16;
		adrs += ppc_inst_dump(adrs, nidump);
		last_cmd = "di\n";
	} else {
		scanhex(&ndump);
		if( ndump == 0 )
			ndump = 64;
		prdump(adrs, ndump);
		adrs += ndump;
		last_cmd = "d\n";
	}
}

void
prdump(unsigned long adrs, long ndump)
{
	long n, m, c, r, nr;
	unsigned char temp[16];

	for( n = ndump; n > 0; ){
		printf("%.16lx", adrs);
		putchar(' ');
		r = n < 16? n: 16;
		nr = mread(adrs, temp, r);
		adrs += nr;
		for( m = 0; m < r; ++m ){
		        if ((m & 7) == 0 && m > 0)
			    putchar(' ');
			if( m < nr )
				printf("%.2x", temp[m]);
			else
				printf("%s", fault_chars[fault_type]);
		}
		for(; m < 16; ++m )
			printf("   ");
		printf("  |");
		for( m = 0; m < r; ++m ){
			if( m < nr ){
				c = temp[m];
				putchar(' ' <= c && c <= '~'? c: '.');
			} else
				putchar(' ');
		}
		n -= r;
		for(; m < 16; ++m )
			putchar(' ');
		printf("|\n");
		if( nr < r )
			break;
	}
}

int
ppc_inst_dump(unsigned long adr, long count)
{
	int nr, dotted;
	unsigned long first_adr;
	unsigned long inst, last_inst;
	unsigned char val[4];

	dotted = 0;
	for (first_adr = adr; count > 0; --count, adr += 4){
		nr = mread(adr, val, 4);
		if( nr == 0 ){
			const char *x = fault_chars[fault_type];
			printf("%.16lx  %s%s%s%s\n", adr, x, x, x, x);
			break;
		}
		inst = GETWORD(val);
		if (adr > first_adr && inst == last_inst) {
			if (!dotted) {
				printf(" ...\n");
				dotted = 1;
			}
			continue;
		}
		dotted = 0;
		last_inst = inst;
		printf("%.16lx  ", adr);
		printf("%.8x\t", inst);
		print_insn_big_powerpc(stdout, inst, adr);	/* always returns 4 */
		printf("\n");
	}
	return adr - first_adr;
}

void
print_address(unsigned long addr)
{
	printf("0x%lx", addr);
}

/*
 * Memory operations - move, set, print differences
 */
static unsigned long mdest;		/* destination address */
static unsigned long msrc;		/* source address */
static unsigned long mval;		/* byte value to set memory to */
static unsigned long mcount;		/* # bytes to affect */
static unsigned long mdiffs;		/* max # differences to print */

void
memops(int cmd)
{
	scanhex((void *)&mdest);
	if( termch != '\n' )
		termch = 0;
	scanhex((void *)(cmd == 's'? &mval: &msrc));
	if( termch != '\n' )
		termch = 0;
	scanhex((void *)&mcount);
	switch( cmd ){
	case 'm':
		memmove((void *)mdest, (void *)msrc, mcount);
		break;
	case 's':
		memset((void *)mdest, mval, mcount);
		break;
	case 'd':
		if( termch != '\n' )
			termch = 0;
		scanhex((void *)&mdiffs);
		memdiffs((unsigned char *)mdest, (unsigned char *)msrc, mcount, mdiffs);
		break;
	}
}

void
memdiffs(unsigned char *p1, unsigned char *p2, unsigned nb, unsigned maxpr)
{
	unsigned n, prt;

	prt = 0;
	for( n = nb; n > 0; --n )
		if( *p1++ != *p2++ )
			if( ++prt <= maxpr )
				printf("%.16x %.2x # %.16x %.2x\n", p1 - 1,
					p1[-1], p2 - 1, p2[-1]);
	if( prt > maxpr )
		printf("Total of %d differences\n", prt);
}

static unsigned mend;
static unsigned mask;

void
memlocate()
{
	unsigned a, n;
	unsigned char val[4];

	last_cmd = "ml";
	scanhex((void *)&mdest);
	if (termch != '\n') {
		termch = 0;
		scanhex((void *)&mend);
		if (termch != '\n') {
			termch = 0;
			scanhex((void *)&mval);
			mask = ~0;
			if (termch != '\n') termch = 0;
			scanhex((void *)&mask);
		}
	}
	n = 0;
	for (a = mdest; a < mend; a += 4) {
		if (mread(a, val, 4) == 4
			&& ((GETWORD(val) ^ mval) & mask) == 0) {
			printf("%.16x:  %.16x\n", a, GETWORD(val));
			if (++n >= 10)
				break;
		}
	}
}

static unsigned long mskip = 0x1000;
static unsigned long mlim = 0xffffffff;

void
memzcan()
{
	unsigned char v;
	unsigned a;
	int ok, ook;

	scanhex(&mdest);
	if (termch != '\n') termch = 0;
	scanhex(&mskip);
	if (termch != '\n') termch = 0;
	scanhex(&mlim);
	ook = 0;
	for (a = mdest; a < mlim; a += mskip) {
		ok = mread(a, &v, 1);
		if (ok && !ook) {
			printf("%.8x .. ", a);
			fflush(stdout);
		} else if (!ok && ook)
			printf("%.8x\n", a - mskip);
		ook = ok;
		if (a + mskip < a)
			break;
	}
	if (ook)
		printf("%.8x\n", a - mskip);
}

/* Input scanning routines */
int
skipbl()
{
	int c;

	if( termch != 0 ){
		c = termch;
		termch = 0;
	} else
		c = inchar();
	while( c == ' ' || c == '\t' )
		c = inchar();
	return c;
}

#define N_PTREGS	44
static char *regnames[N_PTREGS] = {
	"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
	"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
	"r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
	"r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
	"pc", "msr", "or3", "ctr", "lr", "xer", "ccr", "mq",
	"trap", "dar", "dsisr", "res"
};

int
scanhex(vp)
unsigned long *vp;
{
	int c, d;
	unsigned long v;

	c = skipbl();
	if (c == '%') {
		/* parse register name */
		char regname[8];
		int i;

		for (i = 0; i < sizeof(regname) - 1; ++i) {
			c = inchar();
			if (!isalnum(c)) {
				termch = c;
				break;
			}
			regname[i] = c;
		}
		regname[i] = 0;
		for (i = 0; i < N_PTREGS; ++i) {
			if (strcmp(regnames[i], regname) == 0) {
				unsigned long *rp = (unsigned long *)
					xmon_regs[smp_processor_id()];
				if (rp == NULL) {
					printf("regs not available\n");
					return 0;
				}
				*vp = rp[i];
				return 1;
			}
		}
		printf("invalid register name '%%%s'\n", regname);
		return 0;
	}

	d = hexdigit(c);
	if( d == EOF ){
		termch = c;
		return 0;
	}
	v = 0;
	do {
		v = (v << 4) + d;
		c = inchar();
		d = hexdigit(c);
	} while( d != EOF );
	termch = c;
	*vp = v;
	return 1;
}

void
scannl()
{
	int c;

	c = termch;
	termch = 0;
	while( c != '\n' )
		c = inchar();
}

int
hexdigit(int c)
{
	if( '0' <= c && c <= '9' )
		return c - '0';
	if( 'A' <= c && c <= 'F' )
		return c - ('A' - 10);
	if( 'a' <= c && c <= 'f' )
		return c - ('a' - 10);
	return EOF;
}

void
getstring(char *s, int size)
{
	int c;

	c = skipbl();
	do {
		if( size > 1 ){
			*s++ = c;
			--size;
		}
		c = inchar();
	} while( c != ' ' && c != '\t' && c != '\n' );
	termch = c;
	*s = 0;
}

static char line[256];
static char *lineptr;

void
flush_input()
{
	lineptr = NULL;
}

int
inchar()
{
	if (lineptr == NULL || *lineptr == 0) {
		if (fgets(line, sizeof(line), stdin) == NULL) {
			lineptr = NULL;
			return EOF;
		}
		lineptr = line;
	}
	return *lineptr++;
}

void
take_input(str)
char *str;
{
	lineptr = str;
}


/* Starting at codeaddr scan forward for a tbtable and fill in the
 given table.  Return non-zero if successful at doing something.
 */
static int
find_tb_table(unsigned long codeaddr, struct tbtable *tab)
{
	unsigned long codeaddr_max;
	unsigned long tbtab_start;
	int nr;
	int instr;
	int num_parms;

	if (tab == NULL)
		return 0;
	memset(tab, 0, sizeof(tab));

	/* Scan instructions starting at codeaddr for 128k max */
	for (codeaddr_max = codeaddr + 128*1024*4;
	     codeaddr < codeaddr_max;
	     codeaddr += 4) {
		nr = mread(codeaddr, &instr, 4);
		if (nr != 4)
			return 0;	/* Bad read.  Give up promptly. */
		if (instr == 0) {
			/* table should follow. */
			int version;
			unsigned long flags;
			tbtab_start = codeaddr;	/* save it to compute func start addr */
			codeaddr += 4;
			nr = mread(codeaddr, &flags, 8);
			if (nr != 8)
				return 0;	/* Bad read or no tb table. */
			tab->flags = flags;
			version = (flags >> 56) & 0xff;
			if (version != 0)
				continue;	/* No tb table here. */
			/* Now, like the version, some of the flags are values
			 that are more conveniently extracted... */
			tab->fp_saved = (flags >> 24) & 0x3f;
			tab->gpr_saved = (flags >> 16) & 0x3f;
			tab->fixedparms = (flags >> 8) & 0xff;
			tab->floatparms = (flags >> 1) & 0x7f;
			codeaddr += 8;
			num_parms = tab->fixedparms + tab->floatparms;
			if (num_parms) {
				unsigned int parminfo;
				int parm;
				if (num_parms > 32)
					return 1;	/* incomplete */
				nr = mread(codeaddr, &parminfo, 4);
				if (nr != 4)
					return 1;	/* incomplete */
				/* decode parminfo...32 bits.
				 A zero means fixed.  A one means float and the
				 following bit determines single (0) or double (1).
				 */
				for (parm = 0; parm < num_parms; parm++) {
					if (parminfo & 0x80000000) {
						parminfo <<= 1;
						if (parminfo & 0x80000000)
							tab->parminfo[parm] = TBTAB_PARMDFLOAT;
						else
							tab->parminfo[parm] = TBTAB_PARMSFLOAT;
					} else {
						tab->parminfo[parm] = TBTAB_PARMFIXED;
					}
					parminfo <<= 1;
				}
				codeaddr += 4;
			}
			if (flags & TBTAB_FLAGSHASTBOFF) {
				nr = mread(codeaddr, &tab->tb_offset, 4);
				if (nr != 4)
					return 1;	/* incomplete */
				if (tab->tb_offset > 0) {
					tab->funcstart = tbtab_start - tab->tb_offset;
				}
				codeaddr += 4;
			}
			/* hand_mask appears to be always be omitted. */
			if (flags & TBTAB_FLAGSHASCTL) {
				/* Assume this will never happen for C or asm */
				return 1;	/* incomplete */
			}
			if (flags & TBTAB_FLAGSNAMEPRESENT) {
				short namlen;
				nr = mread(codeaddr, &namlen, 2);
				if (nr != 2)
					return 1;	/* incomplete */
				if (namlen >= sizeof(tab->name))
					namlen = sizeof(tab->name)-1;
				codeaddr += 2;
				nr = mread(codeaddr, tab->name, namlen);
				tab->name[namlen] = '\0';
				codeaddr += namlen;
			}
			return 1;
		}
	}
	return 0;	/* hit max...sorry. */
}

void
mem_translate()
{
	int c;
	unsigned long ea, va, vsid, vpn, page, hpteg_slot_primary, hpteg_slot_secondary, primary_hash, i, *steg, esid, stabl;
	HPTE *  hpte;
	struct mm_struct * mm;
	pte_t  *ptep = NULL;
	void * pgdir;
 
	c = inchar();
	if ((isxdigit(c) && c != 'f' && c != 'd') || c == '\n')
		termch = c;
	scanhex((void *)&ea);
  
	if ((ea >= KRANGE_START) && (ea <= (KRANGE_START + (1UL<<60)))) {
		ptep = 0;
		vsid = get_kernel_vsid(ea);
		va = ( vsid << 28 ) | ( ea & 0x0fffffff );
	} else {
		// if in vmalloc range, use the vmalloc page directory
		if ( ( ea >= VMALLOC_START ) && ( ea <= VMALLOC_END ) ) {
			mm = &init_mm;
			vsid = get_kernel_vsid( ea );
		}
		// if in ioremap range, use the ioremap page directory
		else if ( ( ea >= IMALLOC_START ) && ( ea <= IMALLOC_END ) ) {
			mm = &ioremap_mm;
			vsid = get_kernel_vsid( ea );
		}
		// if in user range, use the current task's page directory
		else if ( ( ea >= USER_START ) && ( ea <= USER_END ) ) {
			mm = current->mm;
			vsid = get_vsid(mm->context, ea );
		}
		pgdir = mm->pgd;
		va = ( vsid << 28 ) | ( ea & 0x0fffffff );
		ptep = find_linux_pte( pgdir, ea );
	}

	vpn = ((vsid << 28) | (((ea) & 0xFFFF000))) >> 12;
	page = vpn & 0xffff;
	esid = (ea >> 28)  & 0xFFFFFFFFF;

  // Search the primary group for an available slot
	primary_hash = ( vsid & 0x7fffffffff ) ^ page;
	hpteg_slot_primary = ( primary_hash & htab_data.htab_hash_mask ) * HPTES_PER_GROUP;
	hpteg_slot_secondary = ( ~primary_hash & htab_data.htab_hash_mask ) * HPTES_PER_GROUP;

	printf("ea             : %.16lx\n", ea);
	printf("esid           : %.16lx\n", esid);
	printf("vsid           : %.16lx\n", vsid);

	printf("\nSoftware Page Table\n-------------------\n");
	printf("ptep           : %.16lx\n", ((unsigned long *)ptep));
	if(ptep) {
		printf("*ptep          : %.16lx\n", *((unsigned long *)ptep));
	}

	hpte  = htab_data.htab  + hpteg_slot_primary;
	printf("\nHardware Page Table\n-------------------\n");
	printf("htab base      : %.16lx\n", htab_data.htab);
	printf("slot primary   : %.16lx\n", hpteg_slot_primary);
	printf("slot secondary : %.16lx\n", hpteg_slot_secondary);
	printf("\nPrimary Group\n");
	for (i=0; i<8; ++i) {
		if ( hpte->dw0.dw0.v != 0 ) {
			printf("%d: (hpte)%.16lx %.16lx\n", i, hpte->dw0.dword0, hpte->dw1.dword1);
			printf("          vsid: %.13lx   api: %.2lx  hash: %.1lx\n", 
			       (hpte->dw0.dw0.avpn)>>5, 
			       (hpte->dw0.dw0.avpn) & 0x1f,
			       (hpte->dw0.dw0.h));
			printf("          rpn: %.13lx \n", (hpte->dw1.dw1.rpn));
			printf("           pp: %.1lx \n", 
			       ((hpte->dw1.dw1.pp0)<<2)|(hpte->dw1.dw1.pp));
			printf("        wimgn: %.2lx  reference: %.1lx  change: %.1lx\n", 
			       ((hpte->dw1.dw1.w)<<4)|
			       ((hpte->dw1.dw1.i)<<3)|
			       ((hpte->dw1.dw1.m)<<2)|
			       ((hpte->dw1.dw1.g)<<1)|
			       ((hpte->dw1.dw1.n)<<0),
			       hpte->dw1.dw1.r, hpte->dw1.dw1.c);
		}
		hpte++;
	}

	printf("\nSecondary Group\n");
	// Search the secondary group
	hpte  = htab_data.htab  + hpteg_slot_secondary;
	for (i=0; i<8; ++i) {
		if(hpte->dw0.dw0.v) {
			printf("%d: (hpte)%.16lx %.16lx\n", i, hpte->dw0.dword0, hpte->dw1.dword1);
			printf("          vsid: %.13lx   api: %.2lx  hash: %.1lx\n", 
			       (hpte->dw0.dw0.avpn)>>5, 
			       (hpte->dw0.dw0.avpn) & 0x1f,
			       (hpte->dw0.dw0.h));
			printf("          rpn: %.13lx \n", (hpte->dw1.dw1.rpn));
			printf("           pp: %.1lx \n", 
			       ((hpte->dw1.dw1.pp0)<<2)|(hpte->dw1.dw1.pp));
			printf("        wimgn: %.2lx  reference: %.1lx  change: %.1lx\n", 
			       ((hpte->dw1.dw1.w)<<4)|
			       ((hpte->dw1.dw1.i)<<3)|
			       ((hpte->dw1.dw1.m)<<2)|
			       ((hpte->dw1.dw1.g)<<1)|
			       ((hpte->dw1.dw1.n)<<0),
			       hpte->dw1.dw1.r, hpte->dw1.dw1.c);
		}
		hpte++;
	}

	printf("\nHardware Segment Table\n-----------------------\n");
	stabl = (unsigned long)(KERNELBASE+(_ASR&0xFFFFFFFFFFFFFFFE));
	steg = (unsigned long *)((stabl) | ((esid & 0x1f) << 7));

	printf("stab base      : %.16lx\n", stabl);
	printf("slot           : %.16lx\n", steg);

	for (i=0; i<8; ++i) {
		printf("%d: (ste) %.16lx %.16lx\n", i,
		       *((unsigned long *)(steg+i*2)),*((unsigned long *)(steg+i*2+1)) );
	}
}

void mem_check()
{
	unsigned long htab_size_bytes;
	unsigned long htab_end;
	unsigned long last_rpn;
	HPTE *hpte1, *hpte2;

	htab_size_bytes = htab_data.htab_num_ptegs * 128; // 128B / PTEG
	htab_end = (unsigned long)htab_data.htab + htab_size_bytes;
	// last_rpn = (naca->physicalMemorySize-1) >> PAGE_SHIFT;
	last_rpn = 0xfffff;

	printf("\nHardware Page Table Check\n-------------------\n");
	printf("htab base      : %.16lx\n", htab_data.htab);
	printf("htab size      : %.16lx\n", htab_size_bytes);

#if 1
	for(hpte1 = htab_data.htab; hpte1 < (HPTE *)htab_end; hpte1++) {
		if ( hpte1->dw0.dw0.v != 0 ) {
			if ( hpte1->dw1.dw1.rpn <= last_rpn ) {
				for(hpte2 = hpte1+1; hpte2 < (HPTE *)htab_end; hpte2++) {
					if ( hpte2->dw0.dw0.v != 0 ) {
						if(hpte1->dw1.dw1.rpn == hpte2->dw1.dw1.rpn) {
							printf(" Duplicate rpn: %.13lx \n", (hpte1->dw1.dw1.rpn));
							printf("   hpte1: %16.16lx  *hpte1: %16.16lx %16.16lx\n",
							       hpte1, hpte1->dw0.dword0, hpte1->dw1.dword1);
							printf("   hpte2: %16.16lx  *hpte2: %16.16lx %16.16lx\n",
							       hpte2, hpte2->dw0.dword0, hpte2->dw1.dword1);
						}
					}
				}
			} else {
				printf(" Bogus rpn: %.13lx \n", (hpte1->dw1.dw1.rpn));
				printf("   hpte: %16.16lx  *hpte: %16.16lx %16.16lx\n",
				       hpte1, hpte1->dw0.dword0, hpte1->dw1.dword1);
			}
		}
	}
#endif
	printf("\nDone -------------------\n");
}

void mem_find_real()
{
	unsigned long htab_size_bytes;
	unsigned long htab_end;
	unsigned long last_rpn;
	HPTE *hpte1;
	unsigned long pa, rpn;
	int c;

	c = inchar();
	if ((isxdigit(c) && c != 'f' && c != 'd') || c == '\n')
		termch = c;
	scanhex((void *)&pa);
	rpn = pa >> 12;
  
	htab_size_bytes = htab_data.htab_num_ptegs * 128; // 128B / PTEG
	htab_end = (unsigned long)htab_data.htab + htab_size_bytes;
	// last_rpn = (naca->physicalMemorySize-1) >> PAGE_SHIFT;
	last_rpn = 0xfffff;

	printf("\nMem Find RPN\n-------------------\n");
	printf("htab base      : %.16lx\n", htab_data.htab);
	printf("htab size      : %.16lx\n", htab_size_bytes);

	for(hpte1 = htab_data.htab; hpte1 < (HPTE *)htab_end; hpte1++) {
		if ( hpte1->dw0.dw0.v != 0 ) {
			if ( hpte1->dw1.dw1.rpn == rpn ) {
				printf(" Found rpn: %.13lx \n", (hpte1->dw1.dw1.rpn));
				printf("      hpte: %16.16lx  *hpte1: %16.16lx %16.16lx\n",
				       hpte1, hpte1->dw0.dword0, hpte1->dw1.dword1);
			}
		}
	}
	printf("\nDone -------------------\n");
}

void mem_find_vsid()
{
	unsigned long htab_size_bytes;
	unsigned long htab_end;
	HPTE *hpte1;
	unsigned long vsid;
	int c;

	c = inchar();
	if ((isxdigit(c) && c != 'f' && c != 'd') || c == '\n')
		termch = c;
	scanhex((void *)&vsid);
  
	htab_size_bytes = htab_data.htab_num_ptegs * 128; // 128B / PTEG
	htab_end = (unsigned long)htab_data.htab + htab_size_bytes;

	printf("\nMem Find VSID\n-------------------\n");
	printf("htab base      : %.16lx\n", htab_data.htab);
	printf("htab size      : %.16lx\n", htab_size_bytes);

	for(hpte1 = htab_data.htab; hpte1 < (HPTE *)htab_end; hpte1++) {
		if ( hpte1->dw0.dw0.v != 0 ) {
			if ( ((hpte1->dw0.dw0.avpn)>>5) == vsid ) {
				printf(" Found vsid: %.16lx \n", ((hpte1->dw0.dw0.avpn) >> 5));
				printf("       hpte: %16.16lx  *hpte1: %16.16lx %16.16lx\n",
				       hpte1, hpte1->dw0.dword0, hpte1->dw1.dword1);
			}
		}
	}
	printf("\nDone -------------------\n");
}

static void debug_trace(void) {
        unsigned long val, cmd, on;

	cmd = skipbl();
	if (cmd == '\n') {
		/* show current state */
		unsigned long i;
		printf("naca->debug_switch = 0x%lx\n", naca->debug_switch);
		for (i = 0; i < PPCDBG_NUM_FLAGS ;i++) {
			on = PPCDBG_BITVAL(i) & naca->debug_switch;
			printf("%02x %s %12s   ", i, on ? "on " : "off",  trace_names[i] ? trace_names[i] : "");
			if (((i+1) % 3) == 0)
				printf("\n");
		}
		printf("\n");
		return;
	}
	while (cmd != '\n') {
		on = 1;	/* default if no sign given */
		while (cmd == '+' || cmd == '-') {
			on = (cmd == '+');
			cmd = inchar();
			if (cmd == ' ' || cmd == '\n') {  /* Turn on or off based on + or - */
				naca->debug_switch = on ? PPCDBG_ALL:PPCDBG_NONE;
				printf("Setting all values to %s...\n", on ? "on" : "off");
				if (cmd == '\n') return;
				else cmd = skipbl(); 
			}
			else
				termch = cmd;
		}
		termch = cmd;	/* not +/- ... let scanhex see it */
		scanhex((void *)&val);
		if (val >= 64) {
			printf("Value %x out of range:\n", val);
			return;
		}
		if (on) {
			naca->debug_switch |= PPCDBG_BITVAL(val);
			printf("enable debug %x %s\n", val, trace_names[val] ? trace_names[val] : "");
		} else {
			naca->debug_switch &= ~PPCDBG_BITVAL(val);
			printf("disable debug %x %s\n", val, trace_names[val] ? trace_names[val] : "");
		}
		cmd = skipbl();
	}
}
