/*
 * Copyright (c) 1996, by Steve Passe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: mp_machdep.c,v 1.74 1998/05/11 01:06:06 dyson Exp $
 */

#include "opt_smp.h"
#include "opt_vm86.h"
#include "opt_cpu.h"

#ifdef SMP
#include <machine/smptests.h>
#else
#error
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#ifdef BETTER_CLOCK
#include <sys/dkstat.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#ifdef BETTER_CLOCK
#include <sys/lock.h>
#include <vm/vm_map.h>
#include <sys/user.h>
#ifdef GPROF 
#include <sys/gmon.h>
#endif
#endif

#include <machine/smp.h>
#include <machine/apic.h>
#include <machine/mpapic.h>
#include <machine/segments.h>
#include <machine/smptests.h>	/** TEST_DEFAULT_CONFIG, TEST_TEST1 */
#include <machine/tss.h>
#include <machine/specialreg.h>
#include <machine/cputypes.h>
#include <machine/globaldata.h>

#include <i386/i386/cons.h>	/* cngetc() */

#if defined(APIC_IO)
#include <machine/md_var.h>		/* setidt() */
#include <i386/isa/icu.h>		/* IPIs */
#include <i386/isa/intr_machdep.h>	/* IPIs */
#endif	/* APIC_IO */

#if defined(TEST_DEFAULT_CONFIG)
#define MPFPS_MPFB1	TEST_DEFAULT_CONFIG
#else
#define MPFPS_MPFB1	mpfps->mpfb1
#endif  /* TEST_DEFAULT_CONFIG */

#define WARMBOOT_TARGET		0
#define WARMBOOT_OFF		(KERNBASE + 0x0467)
#define WARMBOOT_SEG		(KERNBASE + 0x0469)

#define BIOS_BASE		(0xf0000)
#define BIOS_SIZE		(0x10000)
#define BIOS_COUNT		(BIOS_SIZE/4)

#define CMOS_REG		(0x70)
#define CMOS_DATA		(0x71)
#define BIOS_RESET		(0x0f)
#define BIOS_WARM		(0x0a)

#define PROCENTRY_FLAG_EN	0x01
#define PROCENTRY_FLAG_BP	0x02
#define IOAPICENTRY_FLAG_EN	0x01


/* MP Floating Pointer Structure */
typedef struct MPFPS {
	char    signature[4];
	void   *pap;
	u_char  length;
	u_char  spec_rev;
	u_char  checksum;
	u_char  mpfb1;
	u_char  mpfb2;
	u_char  mpfb3;
	u_char  mpfb4;
	u_char  mpfb5;
}      *mpfps_t;

/* MP Configuration Table Header */
typedef struct MPCTH {
	char    signature[4];
	u_short base_table_length;
	u_char  spec_rev;
	u_char  checksum;
	u_char  oem_id[8];
	u_char  product_id[12];
	void   *oem_table_pointer;
	u_short oem_table_size;
	u_short entry_count;
	void   *apic_address;
	u_short extended_table_length;
	u_char  extended_table_checksum;
	u_char  reserved;
}      *mpcth_t;


typedef struct PROCENTRY {
	u_char  type;
	u_char  apic_id;
	u_char  apic_version;
	u_char  cpu_flags;
	u_long  cpu_signature;
	u_long  feature_flags;
	u_long  reserved1;
	u_long  reserved2;
}      *proc_entry_ptr;

typedef struct BUSENTRY {
	u_char  type;
	u_char  bus_id;
	char    bus_type[6];
}      *bus_entry_ptr;

typedef struct IOAPICENTRY {
	u_char  type;
	u_char  apic_id;
	u_char  apic_version;
	u_char  apic_flags;
	void   *apic_address;
}      *io_apic_entry_ptr;

typedef struct INTENTRY {
	u_char  type;
	u_char  int_type;
	u_short int_flags;
	u_char  src_bus_id;
	u_char  src_bus_irq;
	u_char  dst_apic_id;
	u_char  dst_apic_int;
}      *int_entry_ptr;

/* descriptions of MP basetable entries */
typedef struct BASETABLE_ENTRY {
	u_char  type;
	u_char  length;
	char    name[16];
}       basetable_entry;

/*
 * this code MUST be enabled here and in mpboot.s.
 * it follows the very early stages of AP boot by placing values in CMOS ram.
 * it NORMALLY will never be needed and thus the primitive method for enabling.
 *
#define CHECK_POINTS
 */

#if defined(CHECK_POINTS)
#define CHECK_READ(A)	 (outb(CMOS_REG, (A)), inb(CMOS_DATA))
#define CHECK_WRITE(A,D) (outb(CMOS_REG, (A)), outb(CMOS_DATA, (D)))

#define CHECK_INIT(D);				\
	CHECK_WRITE(0x34, (D));			\
	CHECK_WRITE(0x35, (D));			\
	CHECK_WRITE(0x36, (D));			\
	CHECK_WRITE(0x37, (D));			\
	CHECK_WRITE(0x38, (D));			\
	CHECK_WRITE(0x39, (D));

#define CHECK_PRINT(S);				\
	printf("%s: %d, %d, %d, %d, %d, %d\n",	\
	   (S),					\
	   CHECK_READ(0x34),			\
	   CHECK_READ(0x35),			\
	   CHECK_READ(0x36),			\
	   CHECK_READ(0x37),			\
	   CHECK_READ(0x38),			\
	   CHECK_READ(0x39));

#else				/* CHECK_POINTS */

#define CHECK_INIT(D)
#define CHECK_PRINT(S)

#endif				/* CHECK_POINTS */

/*
 * Values to send to the POST hardware.
 */
#define MP_BOOTADDRESS_POST	0x10
#define MP_PROBE_POST		0x11
#define MPTABLE_PASS1_POST	0x12

#define MP_START_POST		0x13
#define MP_ENABLE_POST		0x14
#define MPTABLE_PASS2_POST	0x15

#define START_ALL_APS_POST	0x16
#define INSTALL_AP_TRAMP_POST	0x17
#define START_AP_POST		0x18

#define MP_ANNOUNCE_POST	0x19


/** XXX FIXME: where does this really belong, isa.h/isa.c perhaps? */
int	current_postcode;

/** XXX FIXME: what system files declare these??? */
extern struct region_descriptor r_gdt, r_idt;

int	bsp_apic_ready = 0;	/* flags useability of BSP apic */
int	mp_ncpus;		/* # of CPUs, including BSP */
int	mp_naps;		/* # of Applications processors */
int	mp_nbusses;		/* # of busses */
int	mp_napics;		/* # of IO APICs */
int	boot_cpu_id;		/* designated BSP */
vm_offset_t cpu_apic_address;
vm_offset_t io_apic_address[NAPICID];	/* NAPICID is more than enough */
extern	int nkpt;

u_int32_t cpu_apic_versions[NCPU];
u_int32_t io_apic_versions[NAPIC];

#ifdef APIC_INTR_DIAGNOSTIC
int apic_itrace_enter[32];
int apic_itrace_tryisrlock[32];
int apic_itrace_gotisrlock[32];
int apic_itrace_active[32];
int apic_itrace_masked[32];
int apic_itrace_noisrlock[32];
int apic_itrace_masked2[32];
int apic_itrace_unmask[32];
int apic_itrace_noforward[32];
int apic_itrace_leave[32];
int apic_itrace_enter2[32];
int apic_itrace_doreti[32];
int apic_itrace_splz[32];
int apic_itrace_eoi[32];
#ifdef APIC_INTR_DIAGNOSTIC_IRQ
unsigned short apic_itrace_debugbuffer[32768];
int apic_itrace_debugbuffer_idx;
struct simplelock apic_itrace_debuglock;
#endif
#endif

#ifdef APIC_INTR_REORDER
struct {
	volatile int *location;
	int bit;
} apic_isrbit_location[32];
#endif

/*
 * APIC ID logical/physical mapping structures.
 * We oversize these to simplify boot-time config.
 */
int     cpu_num_to_apic_id[NAPICID];
int     io_num_to_apic_id[NAPICID];
int     apic_id_to_logical[NAPICID];


/* Bitmap of all available CPUs */
u_int	all_cpus;

/* AP uses this PTD during bootstrap.  Do not staticize.  */
pd_entry_t *bootPTD;

/* Hotwire a 0->4MB V==P mapping */
extern pt_entry_t *KPTphys;

/* Virtual address of per-cpu common_tss */
extern struct i386tss common_tss;
#ifdef VM86
extern struct segment_descriptor common_tssd;
extern u_int private_tss;		/* flag indicating private tss */
extern u_int my_tr;
#endif /* VM86 */

/* IdlePTD per cpu */
pd_entry_t *IdlePTDS[NCPU];

/* "my" private page table page, for BSP init */
extern pt_entry_t SMP_prvpt[];

/* Private page pointer to curcpu's PTD, used during BSP init */
extern pd_entry_t *my_idlePTD;

static int smp_started;		/* has the system started? */

/*
 * Local data and functions.
 */

static int	mp_capable;
static u_int	boot_address;
static u_int	base_memory;

static int	picmode;		/* 0: virtual wire mode, 1: PIC mode */
static mpfps_t	mpfps;
static int	search_for_sig(u_int32_t target, int count);
static void	mp_enable(u_int boot_addr);

static int	mptable_pass1(void);
static int	mptable_pass2(void);
static void	default_mp_table(int type);
static void	fix_mp_table(void);
static void	init_locks(void);
static int	start_all_aps(u_int boot_addr);
static void	install_ap_tramp(u_int boot_addr);
static int	start_ap(int logicalCpu, u_int boot_addr);

/*
 * Calculate usable address in base memory for AP trampoline code.
 */
u_int
mp_bootaddress(u_int basemem)
{
	POSTCODE(MP_BOOTADDRESS_POST);

	base_memory = basemem * 1024;	/* convert to bytes */

	boot_address = base_memory & ~0xfff;	/* round down to 4k boundary */
	if ((base_memory - boot_address) < bootMP_size)
		boot_address -= 4096;	/* not enough, lower by 4k */

	return boot_address;
}


/*
 * Look for an Intel MP spec table (ie, SMP capable hardware).
 */
int
mp_probe(void)
{
	int     x;
	u_long  segment;
	u_int32_t target;

	POSTCODE(MP_PROBE_POST);

	/* see if EBDA exists */
	if (segment = (u_long) * (u_short *) (KERNBASE + 0x40e)) {
		/* search first 1K of EBDA */
		target = (u_int32_t) (segment << 4);
		if ((x = search_for_sig(target, 1024 / 4)) >= 0)
			goto found;
	} else {
		/* last 1K of base memory, effective 'top of base' passed in */
		target = (u_int32_t) (base_memory - 0x400);
		if ((x = search_for_sig(target, 1024 / 4)) >= 0)
			goto found;
	}

	/* search the BIOS */
	target = (u_int32_t) BIOS_BASE;
	if ((x = search_for_sig(target, BIOS_COUNT)) >= 0)
		goto found;

	/* nothing found */
	mpfps = (mpfps_t)0;
	mp_capable = 0;
	return 0;

found:
	/* calculate needed resources */
	mpfps = (mpfps_t)x;
	if (mptable_pass1())
		panic("you must reconfigure your kernel");

	/* flag fact that we are running multiple processors */
	mp_capable = 1;
	return 1;
}


/*
 * Startup the SMP processors.
 */
void
mp_start(void)
{
	POSTCODE(MP_START_POST);

	/* look for MP capable motherboard */
	if (mp_capable)
		mp_enable(boot_address);
	else
		panic("MP hardware not found!");
}


/*
 * Print various information about the SMP system hardware and setup.
 */
void
mp_announce(void)
{
	int     x;

	POSTCODE(MP_ANNOUNCE_POST);

	printf("FreeBSD/SMP: Multiprocessor motherboard\n");
	printf(" cpu0 (BSP): apic id: %2d", CPU_TO_ID(0));
	printf(", version: 0x%08x", cpu_apic_versions[0]);
	printf(", at 0x%08x\n", cpu_apic_address);
	for (x = 1; x <= mp_naps; ++x) {
		printf(" cpu%d (AP):  apic id: %2d", x, CPU_TO_ID(x));
		printf(", version: 0x%08x", cpu_apic_versions[x]);
		printf(", at 0x%08x\n", cpu_apic_address);
	}

#if defined(APIC_IO)
	for (x = 0; x < mp_napics; ++x) {
		printf(" io%d (APIC): apic id: %2d", x, IO_TO_ID(x));
		printf(", version: 0x%08x", io_apic_versions[x]);
		printf(", at 0x%08x\n", io_apic_address[x]);
	}
#else
	printf(" Warning: APIC I/O disabled\n");
#endif	/* APIC_IO */
}

/*
 * AP cpu's call this to sync up protected mode.
 */
void
init_secondary(void)
{
	int	gsel_tss;
#ifndef VM86
	u_int	my_tr;
#endif

	r_gdt.rd_limit = sizeof(gdt[0]) * (NGDT + NCPU) - 1;
	r_gdt.rd_base = (int) gdt;
	lgdt(&r_gdt);			/* does magic intra-segment return */
	lidt(&r_idt);
	lldt(_default_ldt);

	my_tr = NGDT + cpuid;
	gsel_tss = GSEL(my_tr, SEL_KPL);
	gdt[my_tr].sd.sd_type = SDT_SYS386TSS;
	common_tss.tss_esp0 = 0;	/* not used until after switch */
	common_tss.tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);
	common_tss.tss_ioopt = (sizeof common_tss) << 16;
#ifdef VM86
	common_tssd = gdt[my_tr].sd;
	private_tss = 0;
#endif /* VM86 */
	ltr(gsel_tss);

	load_cr0(0x8005003b);		/* XXX! */

	PTD[0] = 0;
	pmap_set_opt((unsigned *)PTD);

	putmtrr();
	pmap_setvidram();

	invltlb();
}


#if defined(APIC_IO)
/*
 * Final configuration of the BSP's local APIC:
 *  - disable 'pic mode'.
 *  - disable 'virtual wire mode'.
 *  - enable NMI.
 */
void
bsp_apic_configure(void)
{
	u_char		byte;
	u_int32_t	temp;

	/* leave 'pic mode' if necessary */
	if (picmode) {
		outb(0x22, 0x70);	/* select IMCR */
		byte = inb(0x23);	/* current contents */
		byte |= 0x01;		/* mask external INTR */
		outb(0x23, byte);	/* disconnect 8259s/NMI */
	}

	/* mask lint0 (the 8259 'virtual wire' connection) */
	temp = lapic.lvt_lint0;
	temp |= APIC_LVT_M;		/* set the mask */
	lapic.lvt_lint0 = temp;

        /* setup lint1 to handle NMI */
        temp = lapic.lvt_lint1;
        temp &= ~APIC_LVT_M;		/* clear the mask */
        lapic.lvt_lint1 = temp;

	if (bootverbose)
		apic_dump("bsp_apic_configure()");
}
#endif  /* APIC_IO */


/*******************************************************************
 * local functions and data
 */

/*
 * start the SMP system
 */
static void
mp_enable(u_int boot_addr)
{
	int     x;
#if defined(APIC_IO)
	int     apic;
	u_int   ux;
#endif	/* APIC_IO */

	getmtrr();
	pmap_setvidram();

	POSTCODE(MP_ENABLE_POST);

	/* turn on 4MB of V == P addressing so we can get to MP table */
	*(int *)PTD = PG_V | PG_RW | ((u_long)KPTphys & PG_FRAME);
	invltlb();

	/* examine the MP table for needed info, uses physical addresses */
	x = mptable_pass2();

	*(int *)PTD = 0;
	invltlb();

	/* can't process default configs till the CPU APIC is pmapped */
	if (x)
		default_mp_table(x);

	/* post scan cleanup */
	fix_mp_table();

#if defined(APIC_IO)

	/* fill the LOGICAL io_apic_versions table */
	for (apic = 0; apic < mp_napics; ++apic) {
		ux = io_apic_read(apic, IOAPIC_VER);
		io_apic_versions[apic] = ux;
	}

	/* program each IO APIC in the system */
	for (apic = 0; apic < mp_napics; ++apic)
		if (io_apic_setup(apic) < 0)
			panic("IO APIC setup failure");

	/* install a 'Spurious INTerrupt' vector */
	setidt(XSPURIOUSINT_OFFSET, Xspuriousint,
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

	/* install an inter-CPU IPI for TLB invalidation */
	setidt(XINVLTLB_OFFSET, Xinvltlb,
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

#ifdef BETTER_CLOCK
	/* install an inter-CPU IPI for reading processor state */
	setidt(XCPUCHECKSTATE_OFFSET, Xcpucheckstate,
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#endif
	
	/* install an inter-CPU IPI for forcing an additional software trap */
	setidt(XCPUAST_OFFSET, Xcpuast,
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	
	/* install an inter-CPU IPI for interrupt forwarding */
	setidt(XFORWARD_IRQ_OFFSET, Xforward_irq,
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

	/* install an inter-CPU IPI for CPU stop/restart */
	setidt(XCPUSTOP_OFFSET, Xcpustop,
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

#if defined(TEST_TEST1)
	/* install a "fake hardware INTerrupt" vector */
	setidt(XTEST1_OFFSET, Xtest1,
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#endif  /** TEST_TEST1 */

#endif	/* APIC_IO */

	/* initialize all SMP locks */
	init_locks();

	/* start each Application Processor */
	start_all_aps(boot_addr);

	/* 
	 * The init process might be started on a different CPU now,
	 * and the boot CPU might not call prepare_usermode to get
	 * cr0 correctly configured. Thus we initialize cr0 here.
	 */
	load_cr0(rcr0() | CR0_WP | CR0_AM);
}


/*
 * look for the MP spec signature
 */

/* string defined by the Intel MP Spec as identifying the MP table */
#define MP_SIG		0x5f504d5f	/* _MP_ */
#define NEXT(X)		((X) += 4)
static int
search_for_sig(u_int32_t target, int count)
{
	int     x;
	u_int32_t *addr = (u_int32_t *) (KERNBASE + target);

	for (x = 0; x < count; NEXT(x))
		if (addr[x] == MP_SIG)
			/* make array index a byte index */
			return (target + (x * sizeof(u_int32_t)));

	return -1;
}


static basetable_entry basetable_entry_types[] =
{
	{0, 20, "Processor"},
	{1, 8, "Bus"},
	{2, 8, "I/O APIC"},
	{3, 8, "I/O INT"},
	{4, 8, "Local INT"}
};

typedef struct BUSDATA {
	u_char  bus_id;
	enum busTypes bus_type;
}       bus_datum;

typedef struct INTDATA {
	u_char  int_type;
	u_short int_flags;
	u_char  src_bus_id;
	u_char  src_bus_irq;
	u_char  dst_apic_id;
	u_char  dst_apic_int;
}       io_int, local_int;

typedef struct BUSTYPENAME {
	u_char  type;
	char    name[7];
}       bus_type_name;

static bus_type_name bus_type_table[] =
{
	{CBUS, "CBUS"},
	{CBUSII, "CBUSII"},
	{EISA, "EISA"},
	{UNKNOWN_BUSTYPE, "---"},
	{UNKNOWN_BUSTYPE, "---"},
	{ISA, "ISA"},
	{UNKNOWN_BUSTYPE, "---"},
	{UNKNOWN_BUSTYPE, "---"},
	{UNKNOWN_BUSTYPE, "---"},
	{UNKNOWN_BUSTYPE, "---"},
	{UNKNOWN_BUSTYPE, "---"},
	{UNKNOWN_BUSTYPE, "---"},
	{PCI, "PCI"},
	{UNKNOWN_BUSTYPE, "---"},
	{UNKNOWN_BUSTYPE, "---"},
	{UNKNOWN_BUSTYPE, "---"},
	{UNKNOWN_BUSTYPE, "---"},
	{XPRESS, "XPRESS"},
	{UNKNOWN_BUSTYPE, "---"}
};
/* from MP spec v1.4, table 5-1 */
static int default_data[7][5] =
{
/*   nbus, id0, type0, id1, type1 */
	{1, 0, ISA, 255, 255},
	{1, 0, EISA, 255, 255},
	{1, 0, EISA, 255, 255},
	{0, 255, 255, 255, 255},/* MCA not supported */
	{2, 0, ISA, 1, PCI},
	{2, 0, EISA, 1, PCI},
	{0, 255, 255, 255, 255}	/* MCA not supported */
};


/* the bus data */
static bus_datum bus_data[NBUS];

/* the IO INT data, one entry per possible APIC INTerrupt */
static io_int  io_apic_ints[NINTR];

static int nintrs;

static int processor_entry	__P((proc_entry_ptr entry, int cpu));
static int bus_entry		__P((bus_entry_ptr entry, int bus));
static int io_apic_entry	__P((io_apic_entry_ptr entry, int apic));
static int int_entry		__P((int_entry_ptr entry, int intr));
static int lookup_bus_type	__P((char *name));


/*
 * 1st pass on motherboard's Intel MP specification table.
 *
 * initializes:
 *	mp_ncpus = 1
 *
 * determines:
 *	cpu_apic_address (common to all CPUs)
 *	io_apic_address[N]
 *	mp_naps
 *	mp_nbusses
 *	mp_napics
 *	nintrs
 */
static int
mptable_pass1(void)
{
	int	x;
	mpcth_t	cth;
	int	totalSize;
	void*	position;
	int	count;
	int	type;
	int	mustpanic;

	POSTCODE(MPTABLE_PASS1_POST);

	mustpanic = 0;

	/* clear various tables */
	for (x = 0; x < NAPICID; ++x) {
		io_apic_address[x] = ~0;	/* IO APIC address table */
	}

	/* init everything to empty */
	mp_naps = 0;
	mp_nbusses = 0;
	mp_napics = 0;
	nintrs = 0;

	/* check for use of 'default' configuration */
	if (MPFPS_MPFB1 != 0) {
		/* use default addresses */
		cpu_apic_address = DEFAULT_APIC_BASE;
		io_apic_address[0] = DEFAULT_IO_APIC_BASE;

		/* fill in with defaults */
		mp_naps = 2;		/* includes BSP */
		mp_nbusses = default_data[MPFPS_MPFB1 - 1][0];
#if defined(APIC_IO)
		mp_napics = 1;
		nintrs = 16;
#endif	/* APIC_IO */
	}
	else {
		if ((cth = mpfps->pap) == 0)
			panic("MP Configuration Table Header MISSING!");

		cpu_apic_address = (vm_offset_t) cth->apic_address;

		/* walk the table, recording info of interest */
		totalSize = cth->base_table_length - sizeof(struct MPCTH);
		position = (u_char *) cth + sizeof(struct MPCTH);
		count = cth->entry_count;

		while (count--) {
			switch (type = *(u_char *) position) {
			case 0: /* processor_entry */
				if (((proc_entry_ptr)position)->cpu_flags
					& PROCENTRY_FLAG_EN)
					++mp_naps;
				break;
			case 1: /* bus_entry */
				++mp_nbusses;
				break;
			case 2: /* io_apic_entry */
				if (((io_apic_entry_ptr)position)->apic_flags
					& IOAPICENTRY_FLAG_EN)
					io_apic_address[mp_napics++] =
					    (vm_offset_t)((io_apic_entry_ptr)
						position)->apic_address;
				break;
			case 3: /* int_entry */
				++nintrs;
				break;
			case 4:	/* int_entry */
				break;
			default:
				panic("mpfps Base Table HOSED!");
				/* NOTREACHED */
			}

			totalSize -= basetable_entry_types[type].length;
			(u_char*)position += basetable_entry_types[type].length;
		}
	}

	/* qualify the numbers */
	if (mp_naps > NCPU)
#if 0 /* XXX FIXME: kern/4255 */
		printf("Warning: only using %d of %d available CPUs!\n",
			NCPU, mp_naps);
#else
	{
		printf("NCPU cannot be different than actual CPU count.\n");
		printf(" add 'options NCPU=%d' to your kernel config file,\n",
			mp_naps);
		printf(" then rerun config & rebuild your SMP kernel\n");
		mustpanic = 1;
	}
#endif /* XXX FIXME: kern/4255 */
	if (mp_nbusses > NBUS) {
		printf("found %d busses, increase NBUS\n", mp_nbusses);
		mustpanic = 1;
	}
	if (mp_napics > NAPIC) {
		printf("found %d apics, increase NAPIC\n", mp_napics);
		mustpanic = 1;
	}
	if (nintrs > NINTR) {
		printf("found %d intrs, increase NINTR\n", nintrs);
		mustpanic = 1;
	}

	/*
	 * Count the BSP.
	 * This is also used as a counter while starting the APs.
	 */
	mp_ncpus = 1;

	--mp_naps;	/* subtract the BSP */

	return mustpanic;
}


/*
 * 2nd pass on motherboard's Intel MP specification table.
 *
 * sets:
 *	boot_cpu_id
 *	ID_TO_IO(N), phy APIC ID to log CPU/IO table
 *	CPU_TO_ID(N), logical CPU to APIC ID table
 *	IO_TO_ID(N), logical IO to APIC ID table
 *	bus_data[N]
 *	io_apic_ints[N]
 */
static int
mptable_pass2(void)
{
	int     x;
	mpcth_t cth;
	int     totalSize;
	void*   position;
	int     count;
	int     type;
	int     apic, bus, cpu, intr;

	POSTCODE(MPTABLE_PASS2_POST);

	/* clear various tables */
	for (x = 0; x < NAPICID; ++x) {
		ID_TO_IO(x) = -1;	/* phy APIC ID to log CPU/IO table */
		CPU_TO_ID(x) = -1;	/* logical CPU to APIC ID table */
		IO_TO_ID(x) = -1;	/* logical IO to APIC ID table */
	}

	/* clear bus data table */
	for (x = 0; x < NBUS; ++x)
		bus_data[x].bus_id = 0xff;

	/* clear IO APIC INT table */
	for (x = 0; x < NINTR; ++x)
		io_apic_ints[x].int_type = 0xff;

	/* setup the cpu/apic mapping arrays */
	boot_cpu_id = -1;

	/* record whether PIC or virtual-wire mode */
	picmode = (mpfps->mpfb2 & 0x80) ? 1 : 0;

	/* check for use of 'default' configuration */
	if (MPFPS_MPFB1 != 0)
		return MPFPS_MPFB1;	/* return default configuration type */

	if ((cth = mpfps->pap) == 0)
		panic("MP Configuration Table Header MISSING!");

	/* walk the table, recording info of interest */
	totalSize = cth->base_table_length - sizeof(struct MPCTH);
	position = (u_char *) cth + sizeof(struct MPCTH);
	count = cth->entry_count;
	apic = bus = intr = 0;
	cpu = 1;				/* pre-count the BSP */

	while (count--) {
		switch (type = *(u_char *) position) {
		case 0:
			if (processor_entry(position, cpu))
				++cpu;
			break;
		case 1:
			if (bus_entry(position, bus))
				++bus;
			break;
		case 2:
			if (io_apic_entry(position, apic))
				++apic;
			break;
		case 3:
			if (int_entry(position, intr))
				++intr;
			break;
		case 4:
			/* int_entry(position); */
			break;
		default:
			panic("mpfps Base Table HOSED!");
			/* NOTREACHED */
		}

		totalSize -= basetable_entry_types[type].length;
		(u_char *) position += basetable_entry_types[type].length;
	}

	if (boot_cpu_id == -1)
		panic("NO BSP found!");

	/* report fact that its NOT a default configuration */
	return 0;
}


/*
 * parse an Intel MP specification table
 */
static void
fix_mp_table(void)
{
	int	x;
	int	id;
	int	bus_0;
	int	bus_pci;
	int	num_pci_bus;

	/*
	 * Fix mis-numbering of the PCI bus and its INT entries if the BIOS
	 * did it wrong.  The MP spec says that when more than 1 PCI bus
	 * exists the BIOS must begin with bus entries for the PCI bus and use
	 * actual PCI bus numbering.  This implies that when only 1 PCI bus
	 * exists the BIOS can choose to ignore this ordering, and indeed many
	 * MP motherboards do ignore it.  This causes a problem when the PCI
	 * sub-system makes requests of the MP sub-system based on PCI bus
	 * numbers.	So here we look for the situation and renumber the
	 * busses and associated INTs in an effort to "make it right".
	 */

	/* find bus 0, PCI bus, count the number of PCI busses */
	for (num_pci_bus = 0, x = 0; x < mp_nbusses; ++x) {
		if (bus_data[x].bus_id == 0) {
			bus_0 = x;
		}
		if (bus_data[x].bus_type == PCI) {
			++num_pci_bus;
			bus_pci = x;
		}
	}
	/*
	 * bus_0 == slot of bus with ID of 0
	 * bus_pci == slot of last PCI bus encountered
	 */

	/* check the 1 PCI bus case for sanity */
	if (num_pci_bus == 1) {

		/* if it is number 0 all is well */
		if (bus_data[bus_pci].bus_id == 0)
			return;

		/* mis-numbered, swap with whichever bus uses slot 0 */

		/* swap the bus entry types */
		bus_data[bus_pci].bus_type = bus_data[bus_0].bus_type;
		bus_data[bus_0].bus_type = PCI;

		/* swap each relavant INTerrupt entry */
		id = bus_data[bus_pci].bus_id;
		for (x = 0; x < nintrs; ++x) {
			if (io_apic_ints[x].src_bus_id == id) {
				io_apic_ints[x].src_bus_id = 0;
			}
			else if (io_apic_ints[x].src_bus_id == 0) {
				io_apic_ints[x].src_bus_id = id;
			}
		}
	}
	/* sanity check if more than 1 PCI bus */
	else if (num_pci_bus > 1) {
		for (x = 0; x < mp_nbusses; ++x) {
			if (bus_data[x].bus_type != PCI)
				continue;
			if (bus_data[x].bus_id >= num_pci_bus)
				panic("bad PCI bus numbering");
		}
	}
}


static int
processor_entry(proc_entry_ptr entry, int cpu)
{
	/* check for usability */
	if ((cpu >= NCPU) || !(entry->cpu_flags & PROCENTRY_FLAG_EN))
		return 0;

	/* check for BSP flag */
	if (entry->cpu_flags & PROCENTRY_FLAG_BP) {
		boot_cpu_id = entry->apic_id;
		CPU_TO_ID(0) = entry->apic_id;
		ID_TO_CPU(entry->apic_id) = 0;
		return 0;	/* its already been counted */
	}

	/* add another AP to list, if less than max number of CPUs */
	else {
		CPU_TO_ID(cpu) = entry->apic_id;
		ID_TO_CPU(entry->apic_id) = cpu;
		return 1;
	}
}


static int
bus_entry(bus_entry_ptr entry, int bus)
{
	int     x;
	char    c, name[8];

	/* encode the name into an index */
	for (x = 0; x < 6; ++x) {
		if ((c = entry->bus_type[x]) == ' ')
			break;
		name[x] = c;
	}
	name[x] = '\0';

	if ((x = lookup_bus_type(name)) == UNKNOWN_BUSTYPE)
		panic("unknown bus type: '%s'", name);

	bus_data[bus].bus_id = entry->bus_id;
	bus_data[bus].bus_type = x;

	return 1;
}


static int
io_apic_entry(io_apic_entry_ptr entry, int apic)
{
	if (!(entry->apic_flags & IOAPICENTRY_FLAG_EN))
		return 0;

	IO_TO_ID(apic) = entry->apic_id;
	ID_TO_IO(entry->apic_id) = apic;

	return 1;
}


static int
lookup_bus_type(char *name)
{
	int     x;

	for (x = 0; x < MAX_BUSTYPE; ++x)
		if (strcmp(bus_type_table[x].name, name) == 0)
			return bus_type_table[x].type;

	return UNKNOWN_BUSTYPE;
}


static int
int_entry(int_entry_ptr entry, int intr)
{
	io_apic_ints[intr].int_type = entry->int_type;
	io_apic_ints[intr].int_flags = entry->int_flags;
	io_apic_ints[intr].src_bus_id = entry->src_bus_id;
	io_apic_ints[intr].src_bus_irq = entry->src_bus_irq;
	io_apic_ints[intr].dst_apic_id = entry->dst_apic_id;
	io_apic_ints[intr].dst_apic_int = entry->dst_apic_int;

	return 1;
}


static int
apic_int_is_bus_type(int intr, int bus_type)
{
	int     bus;

	for (bus = 0; bus < mp_nbusses; ++bus)
		if ((bus_data[bus].bus_id == io_apic_ints[intr].src_bus_id)
		    && ((int) bus_data[bus].bus_type == bus_type))
			return 1;

	return 0;
}


/*
 * Given a traditional ISA INT mask, return an APIC mask.
 */
u_int
isa_apic_mask(u_int isa_mask)
{
	int isa_irq;
	int apic_pin;

#if defined(SKIP_IRQ15_REDIRECT)
	if (isa_mask == (1 << 15)) {
		printf("skipping ISA IRQ15 redirect\n");
		return isa_mask;
	}
#endif  /* SKIP_IRQ15_REDIRECT */

	isa_irq = ffs(isa_mask);		/* find its bit position */
	if (isa_irq == 0)			/* doesn't exist */
		return 0;
	--isa_irq;				/* make it zero based */

	apic_pin = isa_apic_pin(isa_irq);	/* look for APIC connection */
	if (apic_pin == -1)
		return 0;

	return (1 << apic_pin);			/* convert pin# to a mask */
}


/*
 * Determine which APIC pin an ISA/EISA INT is attached to.
 */
#define INTTYPE(I)	(io_apic_ints[(I)].int_type)
#define INTPIN(I)	(io_apic_ints[(I)].dst_apic_int)

#define SRCBUSIRQ(I)	(io_apic_ints[(I)].src_bus_irq)
int
isa_apic_pin(int isa_irq)
{
	int     intr;

	for (intr = 0; intr < nintrs; ++intr) {		/* check each record */
		if (INTTYPE(intr) == 0) {		/* standard INT */
			if (SRCBUSIRQ(intr) == isa_irq) {
				if (apic_int_is_bus_type(intr, ISA) ||
			            apic_int_is_bus_type(intr, EISA))
					return INTPIN(intr);	/* found */
			}
		}
	}
	return -1;					/* NOT found */
}


/*
 * Determine which APIC pin a PCI INT is attached to.
 */
#define SRCBUSID(I)	(io_apic_ints[(I)].src_bus_id)
#define SRCBUSDEVICE(I)	((io_apic_ints[(I)].src_bus_irq >> 2) & 0x1f)
#define SRCBUSLINE(I)	(io_apic_ints[(I)].src_bus_irq & 0x03)
int
pci_apic_pin(int pciBus, int pciDevice, int pciInt)
{
	int     intr;

	--pciInt;					/* zero based */

	for (intr = 0; intr < nintrs; ++intr)		/* check each record */
		if ((INTTYPE(intr) == 0)		/* standard INT */
		    && (SRCBUSID(intr) == pciBus)
		    && (SRCBUSDEVICE(intr) == pciDevice)
		    && (SRCBUSLINE(intr) == pciInt))	/* a candidate IRQ */
			if (apic_int_is_bus_type(intr, PCI))
				return INTPIN(intr);	/* exact match */

	return -1;					/* NOT found */
}

int
next_apic_pin(int pin) 
{
	int intr, ointr;
	int bus, bustype;

	bus = 0;
	bustype = 0;
	for (intr = 0; intr < nintrs; intr++) {
		if (INTPIN(intr) != pin || INTTYPE(intr) != 0)
			continue;
		bus = SRCBUSID(intr);
		bustype = apic_bus_type(bus);
		if (bustype != ISA &&
		    bustype != EISA &&
		    bustype != PCI)
			continue;
		break;
	}
	if (intr >= nintrs) {
		return -1;
	}
	for (ointr = intr + 1; ointr < nintrs; ointr++) {
		if (INTTYPE(ointr) != 0)
			continue;
		if (bus != SRCBUSID(ointr))
			continue;
		if (bustype == PCI) {
			if (SRCBUSDEVICE(intr) != SRCBUSDEVICE(ointr))
				continue;
			if (SRCBUSLINE(intr) != SRCBUSLINE(ointr))
				continue;
		}
		if (bustype == ISA || bustype == EISA) {
			if (SRCBUSIRQ(intr) != SRCBUSIRQ(ointr))
				continue;
		}
		if (INTPIN(intr) == INTPIN(ointr))
			continue;
		break;
	}
	if (ointr >= nintrs) {
		return -1;
	}
	return INTPIN(ointr);
}
#undef SRCBUSLINE
#undef SRCBUSDEVICE
#undef SRCBUSID
#undef SRCBUSIRQ

#undef INTPIN
#undef INTTYPE


/*
 * Reprogram the MB chipset to NOT redirect an ISA INTerrupt.
 *
 * XXX FIXME:
 *  Exactly what this means is unclear at this point.  It is a solution
 *  for motherboards that redirect the MBIRQ0 pin.  Generically a motherboard
 *  could route any of the ISA INTs to upper (>15) IRQ values.  But most would
 *  NOT be redirected via MBIRQ0, thus "undirect()ing" them would NOT be an
 *  option.
 */
int
undirect_isa_irq(int rirq)
{
#if defined(READY)
	printf("Freeing redirected ISA irq %d.\n", rirq);
	/** FIXME: tickle the MB redirector chip */
	return ???;
#else
	printf("Freeing (NOT implemented) redirected ISA irq %d.\n", rirq);
	return 0;
#endif  /* READY */
}


/*
 * Reprogram the MB chipset to NOT redirect a PCI INTerrupt
 */
int
undirect_pci_irq(int rirq)
{
#if defined(READY)
	if (bootverbose)
		printf("Freeing redirected PCI irq %d.\n", rirq);

	/** FIXME: tickle the MB redirector chip */
	return ???;
#else
	if (bootverbose)
		printf("Freeing (NOT implemented) redirected PCI irq %d.\n",
		       rirq);
	return 0;
#endif  /* READY */
}


/*
 * given a bus ID, return:
 *  the bus type if found
 *  -1 if NOT found
 */
int
apic_bus_type(int id)
{
	int     x;

	for (x = 0; x < mp_nbusses; ++x)
		if (bus_data[x].bus_id == id)
			return bus_data[x].bus_type;

	return -1;
}


/*
 * given a LOGICAL APIC# and pin#, return:
 *  the associated src bus ID if found
 *  -1 if NOT found
 */
int
apic_src_bus_id(int apic, int pin)
{
	int     x;

	/* search each of the possible INTerrupt sources */
	for (x = 0; x < nintrs; ++x)
		if ((apic == ID_TO_IO(io_apic_ints[x].dst_apic_id)) &&
		    (pin == io_apic_ints[x].dst_apic_int))
			return (io_apic_ints[x].src_bus_id);

	return -1;		/* NOT found */
}


/*
 * given a LOGICAL APIC# and pin#, return:
 *  the associated src bus IRQ if found
 *  -1 if NOT found
 */
int
apic_src_bus_irq(int apic, int pin)
{
	int     x;

	for (x = 0; x < nintrs; x++)
		if ((apic == ID_TO_IO(io_apic_ints[x].dst_apic_id)) &&
		    (pin == io_apic_ints[x].dst_apic_int))
			return (io_apic_ints[x].src_bus_irq);

	return -1;		/* NOT found */
}


/*
 * given a LOGICAL APIC# and pin#, return:
 *  the associated INTerrupt type if found
 *  -1 if NOT found
 */
int
apic_int_type(int apic, int pin)
{
	int     x;

	/* search each of the possible INTerrupt sources */
	for (x = 0; x < nintrs; ++x)
		if ((apic == ID_TO_IO(io_apic_ints[x].dst_apic_id)) &&
		    (pin == io_apic_ints[x].dst_apic_int))
			return (io_apic_ints[x].int_type);

	return -1;		/* NOT found */
}


/*
 * given a LOGICAL APIC# and pin#, return:
 *  the associated trigger mode if found
 *  -1 if NOT found
 */
int
apic_trigger(int apic, int pin)
{
	int     x;

	/* search each of the possible INTerrupt sources */
	for (x = 0; x < nintrs; ++x)
		if ((apic == ID_TO_IO(io_apic_ints[x].dst_apic_id)) &&
		    (pin == io_apic_ints[x].dst_apic_int))
			return ((io_apic_ints[x].int_flags >> 2) & 0x03);

	return -1;		/* NOT found */
}


/*
 * given a LOGICAL APIC# and pin#, return:
 *  the associated 'active' level if found
 *  -1 if NOT found
 */
int
apic_polarity(int apic, int pin)
{
	int     x;

	/* search each of the possible INTerrupt sources */
	for (x = 0; x < nintrs; ++x)
		if ((apic == ID_TO_IO(io_apic_ints[x].dst_apic_id)) &&
		    (pin == io_apic_ints[x].dst_apic_int))
			return (io_apic_ints[x].int_flags & 0x03);

	return -1;		/* NOT found */
}


/*
 * set data according to MP defaults
 * FIXME: probably not complete yet...
 */
static void
default_mp_table(int type)
{
	int     ap_cpu_id;
#if defined(APIC_IO)
	u_int32_t ux;
	int     io_apic_id;
	int     pin;
#endif	/* APIC_IO */

#if 0
	printf("  MP default config type: %d\n", type);
	switch (type) {
	case 1:
		printf("   bus: ISA, APIC: 82489DX\n");
		break;
	case 2:
		printf("   bus: EISA, APIC: 82489DX\n");
		break;
	case 3:
		printf("   bus: EISA, APIC: 82489DX\n");
		break;
	case 4:
		printf("   bus: MCA, APIC: 82489DX\n");
		break;
	case 5:
		printf("   bus: ISA+PCI, APIC: Integrated\n");
		break;
	case 6:
		printf("   bus: EISA+PCI, APIC: Integrated\n");
		break;
	case 7:
		printf("   bus: MCA+PCI, APIC: Integrated\n");
		break;
	default:
		printf("   future type\n");
		break;
		/* NOTREACHED */
	}
#endif	/* 0 */

	boot_cpu_id = (lapic.id & APIC_ID_MASK) >> 24;
	ap_cpu_id = (boot_cpu_id == 0) ? 1 : 0;

	/* BSP */
	CPU_TO_ID(0) = boot_cpu_id;
	ID_TO_CPU(boot_cpu_id) = 0;

	/* one and only AP */
	CPU_TO_ID(1) = ap_cpu_id;
	ID_TO_CPU(ap_cpu_id) = 1;

#if defined(APIC_IO)
	/* one and only IO APIC */
	io_apic_id = (io_apic_read(0, IOAPIC_ID) & APIC_ID_MASK) >> 24;

	/*
	 * sanity check, refer to MP spec section 3.6.6, last paragraph
	 * necessary as some hardware isn't properly setting up the IO APIC
	 */
#if defined(REALLY_ANAL_IOAPICID_VALUE)
	if (io_apic_id != 2) {
#else
	if ((io_apic_id == 0) || (io_apic_id == 1) || (io_apic_id == 15)) {
#endif	/* REALLY_ANAL_IOAPICID_VALUE */
		ux = io_apic_read(0, IOAPIC_ID);	/* get current contents */
		ux &= ~APIC_ID_MASK;	/* clear the ID field */
		ux |= 0x02000000;	/* set it to '2' */
		io_apic_write(0, IOAPIC_ID, ux);	/* write new value */
		ux = io_apic_read(0, IOAPIC_ID);	/* re-read && test */
		if ((ux & APIC_ID_MASK) != 0x02000000)
			panic("can't control IO APIC ID, reg: 0x%08x", ux);
		io_apic_id = 2;
	}
	IO_TO_ID(0) = io_apic_id;
	ID_TO_IO(io_apic_id) = 0;
#endif	/* APIC_IO */

	/* fill out bus entries */
	switch (type) {
	case 1:
	case 2:
	case 3:
	case 5:
	case 6:
		bus_data[0].bus_id = default_data[type - 1][1];
		bus_data[0].bus_type = default_data[type - 1][2];
		bus_data[1].bus_id = default_data[type - 1][3];
		bus_data[1].bus_type = default_data[type - 1][4];
		break;

	/* case 4: case 7:		   MCA NOT supported */
	default:		/* illegal/reserved */
		panic("BAD default MP config: %d", type);
		/* NOTREACHED */
	}

#if defined(APIC_IO)
	/* general cases from MP v1.4, table 5-2 */
	for (pin = 0; pin < 16; ++pin) {
		io_apic_ints[pin].int_type = 0;
		io_apic_ints[pin].int_flags = 0x05;	/* edge/active-hi */
		io_apic_ints[pin].src_bus_id = 0;
		io_apic_ints[pin].src_bus_irq = pin;	/* IRQ2 caught below */
		io_apic_ints[pin].dst_apic_id = io_apic_id;
		io_apic_ints[pin].dst_apic_int = pin;	/* 1-to-1 */
	}

	/* special cases from MP v1.4, table 5-2 */
	if (type == 2) {
		io_apic_ints[2].int_type = 0xff;	/* N/C */
		io_apic_ints[13].int_type = 0xff;	/* N/C */
#if !defined(APIC_MIXED_MODE)
		/** FIXME: ??? */
		panic("sorry, can't support type 2 default yet");
#endif	/* APIC_MIXED_MODE */
	}
	else
		io_apic_ints[2].src_bus_irq = 0;	/* ISA IRQ0 is on APIC INT 2 */

	if (type == 7)
		io_apic_ints[0].int_type = 0xff;	/* N/C */
	else
		io_apic_ints[0].int_type = 3;	/* vectored 8259 */
#endif	/* APIC_IO */
}


/*
 * initialize all the SMP locks
 */

/* critical region around IO APIC, apic_imen */
struct simplelock	imen_lock;

/* critical region around splxx(), cpl, cml, cil, ipending */
struct simplelock	cpl_lock;

/* Make FAST_INTR() routines sequential */
struct simplelock	fast_intr_lock;

/* critical region around INTR() routines */
struct simplelock	intr_lock;

/* lock regions protected in UP kernel via cli/sti */
struct simplelock	mpintr_lock;

/* lock region used by kernel profiling */
struct simplelock	mcount_lock;

#ifdef USE_COMLOCK
/* locks com (tty) data/hardware accesses: a FASTINTR() */
struct simplelock	com_lock;
#endif /* USE_COMLOCK */

#ifdef USE_CLOCKLOCK
/* lock regions around the clock hardware */
struct simplelock	clock_lock;
#endif /* USE_CLOCKLOCK */

static void
init_locks(void)
{
	/*
	 * Get the initial mp_lock with a count of 1 for the BSP.
	 * This uses a LOGICAL cpu ID, ie BSP == 0.
	 */
	mp_lock = 0x00000001;

	/* ISR uses its own "giant lock" */
	isr_lock = FREE_LOCK;

#if defined(APIC_INTR_DIAGNOSTIC) && defined(APIC_INTR_DIAGNOSTIC_IRQ)
	s_lock_init((struct simplelock*)&apic_itrace_debuglock);
#endif

	s_lock_init((struct simplelock*)&mpintr_lock);

	s_lock_init((struct simplelock*)&mcount_lock);

	s_lock_init((struct simplelock*)&fast_intr_lock);
	s_lock_init((struct simplelock*)&intr_lock);
	s_lock_init((struct simplelock*)&imen_lock);
	s_lock_init((struct simplelock*)&cpl_lock);

#ifdef USE_COMLOCK
	s_lock_init((struct simplelock*)&com_lock);
#endif /* USE_COMLOCK */
#ifdef USE_CLOCKLOCK
	s_lock_init((struct simplelock*)&clock_lock);
#endif /* USE_CLOCKLOCK */
}


/*
 * start each AP in our list
 */
static int
start_all_aps(u_int boot_addr)
{
	int     x, i;
	u_char  mpbiosreason;
	u_long  mpbioswarmvec;
	pd_entry_t *newptd;
	pt_entry_t *newpt;
	struct globaldata *gd;
	char *stack;
	pd_entry_t	*myPTD;

	POSTCODE(START_ALL_APS_POST);

	/* initialize BSP's local APIC */
	apic_initialize();
	bsp_apic_ready = 1;

	/* install the AP 1st level boot code */
	install_ap_tramp(boot_addr);


	/* save the current value of the warm-start vector */
	mpbioswarmvec = *((u_long *) WARMBOOT_OFF);
	outb(CMOS_REG, BIOS_RESET);
	mpbiosreason = inb(CMOS_DATA);

	/* record BSP in CPU map */
	all_cpus = 1;

	/* start each AP */
	for (x = 1; x <= mp_naps; ++x) {

		/* This is a bit verbose, it will go away soon.  */

		/* alloc new page table directory */
		newptd = (pd_entry_t *)(kmem_alloc(kernel_map, PAGE_SIZE));

		/* Store the virtual PTD address for this CPU */
		IdlePTDS[x] = newptd;

		/* clone currently active one (ie: IdlePTD) */
		bcopy(PTD, newptd, PAGE_SIZE);	/* inc prv page pde */

		/* set up 0 -> 4MB P==V mapping for AP boot */
		newptd[0] = (pd_entry_t) (PG_V | PG_RW |
						((u_long)KPTphys & PG_FRAME));

		/* store PTD for this AP's boot sequence */
		myPTD = (pd_entry_t *)vtophys(newptd);

		/* alloc new page table page */
		newpt = (pt_entry_t *)(kmem_alloc(kernel_map, PAGE_SIZE));

		/* set the new PTD's private page to point there */
		newptd[MPPTDI] = (pt_entry_t)(PG_V | PG_RW | vtophys(newpt));

		/* install self referential entry */
		newptd[PTDPTDI] = (pd_entry_t)(PG_V | PG_RW | vtophys(newptd));

		/* allocate a new private data page */
		gd = (struct globaldata *)kmem_alloc(kernel_map, PAGE_SIZE);

		/* wire it into the private page table page */
		newpt[0] = (pt_entry_t)(PG_V | PG_RW | vtophys(gd));

		/* wire the ptp into itself for access */
		newpt[1] = (pt_entry_t)(PG_V | PG_RW | vtophys(newpt));

		/* copy in the pointer to the local apic */
		newpt[2] = SMP_prvpt[2];

		/* and the IO apic mapping[s] */
		for (i = 16; i < 32; i++)
			newpt[i] = SMP_prvpt[i];

		/* allocate and set up an idle stack data page */
		stack = (char *)kmem_alloc(kernel_map, UPAGES*PAGE_SIZE);
		for (i = 0; i < UPAGES; i++)
			newpt[i + 3] = (pt_entry_t)(PG_V | PG_RW | vtophys(PAGE_SIZE * i + stack));

		newpt[3 + UPAGES] = 0;		/* *prv_CMAP1 */
		newpt[4 + UPAGES] = 0;		/* *prv_CMAP2 */
		newpt[5 + UPAGES] = 0;		/* *prv_CMAP3 */
		newpt[6 + UPAGES] = 0;		/* *prv_PMAP1 */

		/* prime data page for it to use */
		gd->cpuid = x;
		gd->cpu_lockid = x << 24;
		gd->my_idlePTD = myPTD;
		gd->prv_CMAP1 = &newpt[3 + UPAGES];
		gd->prv_CMAP2 = &newpt[4 + UPAGES];
		gd->prv_CMAP3 = &newpt[5 + UPAGES];
		gd->prv_PMAP1 = &newpt[6 + UPAGES];

		/* setup a vector to our boot code */
		*((volatile u_short *) WARMBOOT_OFF) = WARMBOOT_TARGET;
		*((volatile u_short *) WARMBOOT_SEG) = (boot_addr >> 4);
		outb(CMOS_REG, BIOS_RESET);
		outb(CMOS_DATA, BIOS_WARM);	/* 'warm-start' */

		bootPTD = myPTD;
		/* attempt to start the Application Processor */
		CHECK_INIT(99);	/* setup checkpoints */
		if (!start_ap(x, boot_addr)) {
			printf("AP #%d (PHY# %d) failed!\n", x, CPU_TO_ID(x));
			CHECK_PRINT("trace");	/* show checkpoints */
			/* better panic as the AP may be running loose */
			printf("panic y/n? [y] ");
			if (cngetc() != 'n')
				panic("bye-bye");
		}
		CHECK_PRINT("trace");		/* show checkpoints */

		/* record its version info */
		cpu_apic_versions[x] = cpu_apic_versions[0];

		all_cpus |= (1 << x);		/* record AP in CPU map */
	}

	/* build our map of 'other' CPUs */
	other_cpus = all_cpus & ~(1 << cpuid);

	/* fill in our (BSP) APIC version */
	cpu_apic_versions[0] = lapic.version;

	/* restore the warmstart vector */
	*(u_long *) WARMBOOT_OFF = mpbioswarmvec;
	outb(CMOS_REG, BIOS_RESET);
	outb(CMOS_DATA, mpbiosreason);

	/*
	 * Set up the idle context for the BSP.  Similar to above except
	 * that some was done by locore, some by pmap.c and some is implicit
	 * because the BSP is cpu#0 and the page is initially zero, and also
	 * because we can refer to variables by name on the BSP..
	 */
	newptd = (pd_entry_t *)(kmem_alloc(kernel_map, PAGE_SIZE));

	bcopy(PTD, newptd, PAGE_SIZE);	/* inc prv page pde */
	IdlePTDS[0] = newptd;

	/* Point PTD[] to this page instead of IdlePTD's physical page */
	newptd[PTDPTDI] = (pd_entry_t)(PG_V | PG_RW | vtophys(newptd));

	my_idlePTD = (pd_entry_t *)vtophys(newptd);

	/* Allocate and setup BSP idle stack */
	stack = (char *)kmem_alloc(kernel_map, UPAGES * PAGE_SIZE);
	for (i = 0; i < UPAGES; i++)
		SMP_prvpt[i + 3] = (pt_entry_t)(PG_V | PG_RW | vtophys(PAGE_SIZE * i + stack));

	pmap_set_opt_bsp();

	for (i = 0; i < mp_ncpus; i++) {
		bcopy( (int *) PTD + KPTDI, (int *) IdlePTDS[i] + KPTDI, NKPDE * sizeof (int));
	}

	/* number of APs actually started */
	return mp_ncpus - 1;
}


/*
 * load the 1st level AP boot code into base memory.
 */

/* targets for relocation */
extern void bigJump(void);
extern void bootCodeSeg(void);
extern void bootDataSeg(void);
extern void MPentry(void);
extern u_int MP_GDT;
extern u_int mp_gdtbase;

static void
install_ap_tramp(u_int boot_addr)
{
	int     x;
	int     size = *(int *) ((u_long) & bootMP_size);
	u_char *src = (u_char *) ((u_long) bootMP);
	u_char *dst = (u_char *) boot_addr + KERNBASE;
	u_int   boot_base = (u_int) bootMP;
	u_int8_t *dst8;
	u_int16_t *dst16;
	u_int32_t *dst32;

	POSTCODE(INSTALL_AP_TRAMP_POST);

	for (x = 0; x < size; ++x)
		*dst++ = *src++;

	/*
	 * modify addresses in code we just moved to basemem. unfortunately we
	 * need fairly detailed info about mpboot.s for this to work.  changes
	 * to mpboot.s might require changes here.
	 */

	/* boot code is located in KERNEL space */
	dst = (u_char *) boot_addr + KERNBASE;

	/* modify the lgdt arg */
	dst32 = (u_int32_t *) (dst + ((u_int) & mp_gdtbase - boot_base));
	*dst32 = boot_addr + ((u_int) & MP_GDT - boot_base);

	/* modify the ljmp target for MPentry() */
	dst32 = (u_int32_t *) (dst + ((u_int) bigJump - boot_base) + 1);
	*dst32 = ((u_int) MPentry - KERNBASE);

	/* modify the target for boot code segment */
	dst16 = (u_int16_t *) (dst + ((u_int) bootCodeSeg - boot_base));
	dst8 = (u_int8_t *) (dst16 + 1);
	*dst16 = (u_int) boot_addr & 0xffff;
	*dst8 = ((u_int) boot_addr >> 16) & 0xff;

	/* modify the target for boot data segment */
	dst16 = (u_int16_t *) (dst + ((u_int) bootDataSeg - boot_base));
	dst8 = (u_int8_t *) (dst16 + 1);
	*dst16 = (u_int) boot_addr & 0xffff;
	*dst8 = ((u_int) boot_addr >> 16) & 0xff;
}


/*
 * this function starts the AP (application processor) identified
 * by the APIC ID 'physicalCpu'.  It does quite a "song and dance"
 * to accomplish this.  This is necessary because of the nuances
 * of the different hardware we might encounter.  It ain't pretty,
 * but it seems to work.
 */
static int
start_ap(int logical_cpu, u_int boot_addr)
{
	int     physical_cpu;
	int     vector;
	int     cpus;
	u_long  icr_lo, icr_hi;

	POSTCODE(START_AP_POST);

	/* get the PHYSICAL APIC ID# */
	physical_cpu = CPU_TO_ID(logical_cpu);

	/* calculate the vector */
	vector = (boot_addr >> 12) & 0xff;

	/* used as a watchpoint to signal AP startup */
	cpus = mp_ncpus;

	/*
	 * first we do an INIT/RESET IPI this INIT IPI might be run, reseting
	 * and running the target CPU. OR this INIT IPI might be latched (P5
	 * bug), CPU waiting for STARTUP IPI. OR this INIT IPI might be
	 * ignored.
	 */

	/* setup the address for the target AP */
	icr_hi = lapic.icr_hi & ~APIC_ID_MASK;
	icr_hi |= (physical_cpu << 24);
	lapic.icr_hi = icr_hi;

	/* do an INIT IPI: assert RESET */
	icr_lo = lapic.icr_lo & 0xfff00000;
	lapic.icr_lo = icr_lo | 0x0000c500;

	/* wait for pending status end */
	while (lapic.icr_lo & APIC_DELSTAT_MASK)
		 /* spin */ ;

	/* do an INIT IPI: deassert RESET */
	lapic.icr_lo = icr_lo | 0x00008500;

	/* wait for pending status end */
	u_sleep(10000);		/* wait ~10mS */
	while (lapic.icr_lo & APIC_DELSTAT_MASK)
		 /* spin */ ;

	/*
	 * next we do a STARTUP IPI: the previous INIT IPI might still be
	 * latched, (P5 bug) this 1st STARTUP would then terminate
	 * immediately, and the previously started INIT IPI would continue. OR
	 * the previous INIT IPI has already run. and this STARTUP IPI will
	 * run. OR the previous INIT IPI was ignored. and this STARTUP IPI
	 * will run.
	 */

	/* do a STARTUP IPI */
	lapic.icr_lo = icr_lo | 0x00000600 | vector;
	while (lapic.icr_lo & APIC_DELSTAT_MASK)
		 /* spin */ ;
	u_sleep(200);		/* wait ~200uS */

	/*
	 * finally we do a 2nd STARTUP IPI: this 2nd STARTUP IPI should run IF
	 * the previous STARTUP IPI was cancelled by a latched INIT IPI. OR
	 * this STARTUP IPI will be ignored, as only ONE STARTUP IPI is
	 * recognized after hardware RESET or INIT IPI.
	 */

	lapic.icr_lo = icr_lo | 0x00000600 | vector;
	while (lapic.icr_lo & APIC_DELSTAT_MASK)
		 /* spin */ ;
	u_sleep(200);		/* wait ~200uS */

	/* wait for it to start */
	set_apic_timer(5000000);/* == 5 seconds */
	while (read_apic_timer())
		if (mp_ncpus > cpus)
			return 1;	/* return SUCCESS */

	return 0;		/* return FAILURE */
}


/*
 * Flush the TLB on all other CPU's
 *
 * XXX: Needs to handshake and wait for completion before proceding.
 */
void
smp_invltlb(void)
{
#if defined(APIC_IO)
	if (smp_started && invltlb_ok)
		all_but_self_ipi(XINVLTLB_OFFSET);
#endif  /* APIC_IO */
}

void
invlpg(u_int addr)
{
	__asm   __volatile("invlpg (%0)"::"r"(addr):"memory");

	/* send a message to the other CPUs */
	smp_invltlb();
}

void
invltlb(void)
{
	u_long  temp;

	/*
	 * This should be implemented as load_cr3(rcr3()) when load_cr3() is
	 * inlined.
	 */
	__asm __volatile("movl %%cr3, %0; movl %0, %%cr3":"=r"(temp) :: "memory");

	/* send a message to the other CPUs */
	smp_invltlb();
}


/*
 * When called the executing CPU will send an IPI to all other CPUs
 *  requesting that they halt execution.
 *
 * Usually (but not necessarily) called with 'other_cpus' as its arg.
 *
 *  - Signals all CPUs in map to stop.
 *  - Waits for each to stop.
 *
 * Returns:
 *  -1: error
 *   0: NA
 *   1: ok
 *
 * XXX FIXME: this is not MP-safe, needs a lock to prevent multiple CPUs
 *            from executing at same time.
 */
int
stop_cpus(u_int map)
{
	if (!smp_started)
		return 0;

	/* send IPI to all CPUs in map */
	stopped_cpus = 0;

	/* send the Xcpustop IPI to all CPUs in map */
	selected_apic_ipi(map, XCPUSTOP_OFFSET, APIC_DELMODE_FIXED);

	while (stopped_cpus != map)
		/* spin */ ;

	return 1;
}


/*
 * Called by a CPU to restart stopped CPUs. 
 *
 * Usually (but not necessarily) called with 'stopped_cpus' as its arg.
 *
 *  - Signals all CPUs in map to restart.
 *  - Waits for each to restart.
 *
 * Returns:
 *  -1: error
 *   0: NA
 *   1: ok
 */
int
restart_cpus(u_int map)
{
	if (!smp_started)
		return 0;

	started_cpus = map;		/* signal other cpus to restart */

	while (started_cpus)		/* wait for each to clear its bit */
		/* spin */ ;
	stopped_cpus = 0;

	return 1;
}

int smp_active = 0;	/* are the APs allowed to run? */
SYSCTL_INT(_machdep, OID_AUTO, smp_active, CTLFLAG_RW, &smp_active, 0, "");

/* XXX maybe should be hw.ncpu */
static int smp_cpus = 1;	/* how many cpu's running */
SYSCTL_INT(_machdep, OID_AUTO, smp_cpus, CTLFLAG_RD, &smp_cpus, 0, "");

int invltlb_ok = 0;	/* throttle smp_invltlb() till safe */
SYSCTL_INT(_machdep, OID_AUTO, invltlb_ok, CTLFLAG_RW, &invltlb_ok, 0, "");

/* Warning: Do not staticize.  Used from swtch.s */
int do_page_zero_idle = 1; /* bzero pages for fun and profit in idleloop */
SYSCTL_INT(_machdep, OID_AUTO, do_page_zero_idle, CTLFLAG_RW,
	   &do_page_zero_idle, 0, "");

/* Is forwarding of a interrupt to the CPU holding the ISR lock enabled ? */
int forward_irq_enabled = 1;
SYSCTL_INT(_machdep, OID_AUTO, forward_irq_enabled, CTLFLAG_RW,
	   &forward_irq_enabled, 0, "");

/* Enable forwarding of a signal to a process running on a different CPU */
int forward_signal_enabled = 1;
SYSCTL_INT(_machdep, OID_AUTO, forward_signal_enabled, CTLFLAG_RW,
	   &forward_signal_enabled, 0, "");

/*
 * This is called once the rest of the system is up and running and we're
 * ready to let the AP's out of the pen.
 */
void ap_init(void);

void
ap_init()
{
	u_int   temp;
	u_int	apic_id;

	smp_cpus++;

#if defined(I586_CPU) && !defined(NO_F00F_HACK)
	lidt(&r_idt);
#endif

	/* Build our map of 'other' CPUs. */
	other_cpus = all_cpus & ~(1 << cpuid);

	printf("SMP: AP CPU #%d Launched!\n", cpuid);

	/* XXX FIXME: i386 specific, and redundant: Setup the FPU. */
	load_cr0((rcr0() & ~CR0_EM) | CR0_MP | CR0_NE | CR0_TS);

	/* A quick check from sanity claus */
	apic_id = (apic_id_to_logical[(lapic.id & 0x0f000000) >> 24]);
	if (cpuid != apic_id) {
		printf("SMP: cpuid = %d\n", cpuid);
		printf("SMP: apic_id = %d\n", apic_id);
		printf("PTD[MPPTDI] = %08x\n", PTD[MPPTDI]);
		panic("cpuid mismatch! boom!!");
	}

	getmtrr();

	/* Init local apic for irq's */
	apic_initialize();

	/*
	 * Activate smp_invltlb, although strictly speaking, this isn't
	 * quite correct yet.  We should have a bitfield for cpus willing
	 * to accept TLB flush IPI's or something and sync them.
	 */
	invltlb_ok = 1;
	smp_started = 1;	/* enable IPI's, tlb shootdown, freezes etc */
	smp_active = 1;		/* historic */

	curproc = NULL;		/* make sure */
}

#ifdef BETTER_CLOCK

#define CHECKSTATE_USER	0
#define CHECKSTATE_SYS	1
#define CHECKSTATE_INTR	2

/* Do not staticize.  Used from apic_vector.s */
struct proc*	checkstate_curproc[NCPU];
int		checkstate_cpustate[NCPU];
u_long		checkstate_pc[NCPU];

extern long	cp_time[CPUSTATES];

#define PC_TO_INDEX(pc, prof)				\
        ((int)(((u_quad_t)((pc) - (prof)->pr_off) *	\
            (u_quad_t)((prof)->pr_scale)) >> 16) & ~1)

static void
addupc_intr_forwarded(struct proc *p, int id, int *astmap)
{
	int i;
	struct uprof *prof;
	u_long pc;

	pc = checkstate_pc[id];
	prof = &p->p_stats->p_prof;
	if (pc >= prof->pr_off &&
	    (i = PC_TO_INDEX(pc, prof)) < prof->pr_size) {
		if ((p->p_flag & P_OWEUPC) == 0) {
			prof->pr_addr = pc;
			prof->pr_ticks = 1;
			p->p_flag |= P_OWEUPC;
		}
		*astmap |= (1 << id);
	}
}

static void
forwarded_statclock(int id, int pscnt, int *astmap)
{
	struct pstats *pstats;
	long rss;
	struct rusage *ru;
	struct vmspace *vm;
	int cpustate;
	struct proc *p;
#ifdef GPROF
	register struct gmonparam *g;
	int i;
#endif

	p = checkstate_curproc[id];
	cpustate = checkstate_cpustate[id];

	switch (cpustate) {
	case CHECKSTATE_USER:
		if (p->p_flag & P_PROFIL)
			addupc_intr_forwarded(p, id, astmap);
		if (pscnt > 1)
			return;
		p->p_uticks++;
		if (p->p_nice > NZERO)
			cp_time[CP_NICE]++;
		else
			cp_time[CP_USER]++;
		break;
	case CHECKSTATE_SYS:
#ifdef GPROF
		/*
		 * Kernel statistics are just like addupc_intr, only easier.
		 */
		g = &_gmonparam;
		if (g->state == GMON_PROF_ON) {
			i = checkstate_pc[id] - g->lowpc;
			if (i < g->textsize) {
				i /= HISTFRACTION * sizeof(*g->kcount);
				g->kcount[i]++;
			}
		}
#endif
		if (pscnt > 1)
			return;

		if (!p)
			cp_time[CP_IDLE]++;
		else {
			p->p_sticks++;
			cp_time[CP_SYS]++;
		}
		break;
	case CHECKSTATE_INTR:
	default:
#ifdef GPROF
		/*
		 * Kernel statistics are just like addupc_intr, only easier.
		 */
		g = &_gmonparam;
		if (g->state == GMON_PROF_ON) {
			i = checkstate_pc[id] - g->lowpc;
			if (i < g->textsize) {
				i /= HISTFRACTION * sizeof(*g->kcount);
				g->kcount[i]++;
			}
		}
#endif
		if (pscnt > 1)
			return;
		if (p)
			p->p_iticks++;
		cp_time[CP_INTR]++;
	}
	if (p != NULL) {
		p->p_cpticks++;
		if (++p->p_estcpu == 0)
			p->p_estcpu--;
		if ((p->p_estcpu & 3) == 0) {
			resetpriority(p);
			if (p->p_priority >= PUSER)
				p->p_priority = p->p_usrpri;
		}
		
		/* Update resource usage integrals and maximums. */
		if ((pstats = p->p_stats) != NULL &&
		    (ru = &pstats->p_ru) != NULL &&
		    (vm = p->p_vmspace) != NULL) {
			ru->ru_ixrss += vm->vm_tsize * PAGE_SIZE / 1024;
			ru->ru_idrss += vm->vm_dsize * PAGE_SIZE / 1024;
			ru->ru_isrss += vm->vm_ssize * PAGE_SIZE / 1024;
			rss = vm->vm_pmap.pm_stats.resident_count *
				PAGE_SIZE / 1024;
			if (ru->ru_maxrss < rss)
				ru->ru_maxrss = rss;
        	}
	}
}

void
forward_statclock(int pscnt)
{
	int map;
	int id;
	int i;

	/* Kludge. We don't yet have separate locks for the interrupts
	 * and the kernel. This means that we cannot let the other processors
	 * handle complex interrupts while inhibiting them from entering
	 * the kernel in a non-interrupt context.
	 *
	 * What we can do, without changing the locking mechanisms yet,
	 * is letting the other processors handle a very simple interrupt
	 * (wich determines the processor states), and do the main
	 * work ourself.
	 */

	if (!smp_started || !invltlb_ok || cold || panicstr)
		return;

	/* Step 1: Probe state   (user, cpu, interrupt, spinlock, idle ) */
	
	map = other_cpus & ~stopped_cpus ;
	checkstate_probed_cpus = 0;
	if (map != 0)
		selected_apic_ipi(map,
				  XCPUCHECKSTATE_OFFSET, APIC_DELMODE_FIXED);

	i = 0;
	while (checkstate_probed_cpus != map) {
		/* spin */
		i++;
		if (i == 1000000) {
			printf("forward_statclock: checkstate %x\n",
			       checkstate_probed_cpus);
			break;
		}
	}

	/*
	 * Step 2: walk through other processors processes, update ticks and 
	 * profiling info.
	 */
	
	map = 0;
	for (id = 0; id < mp_ncpus; id++) {
		if (id == cpuid)
			continue;
		if (((1 << id) & checkstate_probed_cpus) == 0)
			continue;
		forwarded_statclock(id, pscnt, &map);
	}
	if (map != 0) {
		checkstate_need_ast |= map;
		selected_apic_ipi(map, XCPUAST_OFFSET, APIC_DELMODE_FIXED);
		i = 0;
		while ((checkstate_need_ast & map) != 0) {
			/* spin */
			i++;
			if (i > 100000) { 
#ifdef BETTER_CLOCK_DIAGNOSTIC
				printf("forward_statclock: dropped ast 0x%x\n",
				       checkstate_need_ast & map);
#endif
				break;
			}
		}
	}
}

void 
forward_hardclock(int pscnt)
{
	int map;
	int id;
	struct proc *p;
	struct pstats *pstats;
	int i;

	/* Kludge. We don't yet have separate locks for the interrupts
	 * and the kernel. This means that we cannot let the other processors
	 * handle complex interrupts while inhibiting them from entering
	 * the kernel in a non-interrupt context.
	 *
	 * What we can do, without changing the locking mechanisms yet,
	 * is letting the other processors handle a very simple interrupt
	 * (wich determines the processor states), and do the main
	 * work ourself.
	 */

	if (!smp_started || !invltlb_ok || cold || panicstr)
		return;

	/* Step 1: Probe state   (user, cpu, interrupt, spinlock, idle) */
	
	map = other_cpus & ~stopped_cpus ;
	checkstate_probed_cpus = 0;
	if (map != 0)
		selected_apic_ipi(map,
				  XCPUCHECKSTATE_OFFSET, APIC_DELMODE_FIXED);
	
	i = 0;
	while (checkstate_probed_cpus != map) {
		/* spin */
		i++;
		if (i == 1000000) {
			printf("forward_hardclock: checkstate %x\n",
			       checkstate_probed_cpus);
			break;
		}
	}

	/*
	 * Step 2: walk through other processors processes, update virtual 
	 * timer and profiling timer. If stathz == 0, also update ticks and 
	 * profiling info.
	 */
	
	map = 0;
	for (id = 0; id < mp_ncpus; id++) {
		if (id == cpuid)
			continue;
		if (((1 << id) & checkstate_probed_cpus) == 0)
			continue;
		p = checkstate_curproc[id];
		if (p) {
			pstats = p->p_stats;
			if (checkstate_cpustate[id] == CHECKSTATE_USER &&
			    timevalisset(&pstats->p_timer[ITIMER_VIRTUAL].it_value) &&
			    itimerdecr(&pstats->p_timer[ITIMER_VIRTUAL], tick) == 0) {
				psignal(p, SIGVTALRM);
				map |= (1 << id);
			}
			if (timevalisset(&pstats->p_timer[ITIMER_PROF].it_value) &&
			    itimerdecr(&pstats->p_timer[ITIMER_PROF], tick) == 0) {
				psignal(p, SIGPROF);
				map |= (1 << id);
			}
		}
		if (stathz == 0) {
			forwarded_statclock( id, pscnt, &map);
		}
	}
	if (map != 0) {
		checkstate_need_ast |= map;
		selected_apic_ipi(map, XCPUAST_OFFSET, APIC_DELMODE_FIXED);
		i = 0;
		while ((checkstate_need_ast & map) != 0) {
			/* spin */
			i++;
			if (i > 100000) { 
#ifdef BETTER_CLOCK_DIAGNOSTIC
				printf("forward_hardclock: dropped ast 0x%x\n",
				       checkstate_need_ast & map);
#endif
				break;
			}
		}
	}
}

#endif /* BETTER_CLOCK */

void 
forward_signal(struct proc *p)
{
	int map;
	int id;
	int i;

	/* Kludge. We don't yet have separate locks for the interrupts
	 * and the kernel. This means that we cannot let the other processors
	 * handle complex interrupts while inhibiting them from entering
	 * the kernel in a non-interrupt context.
	 *
	 * What we can do, without changing the locking mechanisms yet,
	 * is letting the other processors handle a very simple interrupt
	 * (wich determines the processor states), and do the main
	 * work ourself.
	 */

	if (!smp_started || !invltlb_ok || cold || panicstr)
		return;
	if (!forward_signal_enabled)
		return;
	while (1) {
		if (p->p_stat != SRUN)
			return;
		id = (u_char) p->p_oncpu;
		if (id == 0xff)
			return;
		map = (1<<id);
		checkstate_need_ast |= map;
		selected_apic_ipi(map, XCPUAST_OFFSET, APIC_DELMODE_FIXED);
		i = 0;
		while ((checkstate_need_ast & map) != 0) {
			/* spin */
			i++;
			if (i > 100000) { 
#if 0
				printf("forward_signal: dropped ast 0x%x\n",
				       checkstate_need_ast & map);
#endif
				break;
			}
		}
		if (id == (u_char) p->p_oncpu)
			return;
	}
}


#ifdef APIC_INTR_REORDER
/*
 *	Maintain mapping from softintr vector to isr bit in local apic.
 */
void
set_lapic_isrloc(int intr, int vector)
{
	if (intr < 0 || intr > 32)
		panic("set_apic_isrloc: bad intr argument: %d",intr);
	if (vector < ICU_OFFSET || vector > 255)
		panic("set_apic_isrloc: bad vector argument: %d",vector);
	apic_isrbit_location[intr].location = &lapic.isr0 + ((vector>>5)<<2);
	apic_isrbit_location[intr].bit = (1<<(vector & 31));
}
#endif
