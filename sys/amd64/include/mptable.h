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
 * $FreeBSD$
 */

#include "opt_cpu.h"
#include "opt_kstack_pages.h"

#ifdef SMP
#include <machine/smptests.h>
#else
#error
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cons.h>	/* cngetc() */
#include <sys/dkstat.h>
#ifdef GPROF 
#include <sys/gmon.h>
#endif
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>

#include <machine/apic.h>
#include <machine/atomic.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/mpapic.h>
#include <machine/psl.h>
#include <machine/segments.h>
#include <machine/smp.h>
#include <machine/smptests.h>	/** TEST_DEFAULT_CONFIG, TEST_TEST1 */
#include <machine/tss.h>
#include <machine/specialreg.h>
#include <machine/privatespace.h>

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

#ifdef PC98
#define BIOS_BASE		(0xe8000)
#define BIOS_SIZE		(0x18000)
#else
#define BIOS_BASE		(0xf0000)
#define BIOS_SIZE		(0x10000)
#endif
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

#if defined(CHECK_POINTS) && !defined(PC98)
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

static int need_hyperthreading_fixup;
static u_int logical_cpus;

/* used to hold the AP's until we are ready to release them */
static struct mtx ap_boot_mtx;

/** XXX FIXME: where does this really belong, isa.h/isa.c perhaps? */
int	current_postcode;

/** XXX FIXME: what system files declare these??? */
extern struct region_descriptor r_gdt, r_idt;

int	bsp_apic_ready = 0;	/* flags useability of BSP apic */
int	mp_naps;		/* # of Applications processors */
int	mp_nbusses;		/* # of busses */
int	mp_napics;		/* # of IO APICs */
int	boot_cpu_id;		/* designated BSP */
vm_offset_t cpu_apic_address;
vm_offset_t io_apic_address[NAPICID];	/* NAPICID is more than enough */
extern	int nkpt;

u_int32_t cpu_apic_versions[MAXCPU];
u_int32_t *io_apic_versions;

#ifdef APIC_INTR_REORDER
struct {
	volatile int *location;
	int bit;
} apic_isrbit_location[32];
#endif

struct apic_intmapinfo	int_to_apicintpin[APIC_INTMAPSIZE];

/*
 * APIC ID logical/physical mapping structures.
 * We oversize these to simplify boot-time config.
 */
int     cpu_num_to_apic_id[NAPICID];
int     io_num_to_apic_id[NAPICID];
int     apic_id_to_logical[NAPICID];


/* AP uses this during bootstrap.  Do not staticize.  */
char *bootSTK;
static int bootAP;

/* Hotwire a 0->4MB V==P mapping */
extern pt_entry_t *KPTphys;

/* SMP page table page */
extern pt_entry_t *SMPpt;

struct pcb stoppcbs[MAXCPU];

#ifdef APIC_IO
/* Variables needed for SMP tlb shootdown. */
vm_offset_t smp_tlb_addr1;
vm_offset_t smp_tlb_addr2;
volatile int smp_tlb_wait;
static struct mtx smp_tlb_mtx;
#endif

/*
 * Local data and functions.
 */

/* Set to 1 once we're ready to let the APs out of the pen. */
static volatile int aps_ready = 0;

static int	mp_capable;
static u_int	boot_address;
static u_int	base_memory;

static int	picmode;		/* 0: virtual wire mode, 1: PIC mode */
static mpfps_t	mpfps;
static int	search_for_sig(u_int32_t target, int count);
static void	mp_enable(u_int boot_addr);

static void	mptable_hyperthread_fixup(u_int id_mask);
static void	mptable_pass1(void);
static int	mptable_pass2(void);
static void	default_mp_table(int type);
static void	fix_mp_table(void);
static void	setup_apic_irq_mapping(void);
static void	init_locks(void);
static int	start_all_aps(u_int boot_addr);
static void	install_ap_tramp(u_int boot_addr);
static int	start_ap(int logicalCpu, u_int boot_addr);
void		ap_init(void);
static int	apic_int_is_bus_type(int intr, int bus_type);
static void	release_aps(void *dummy);

/*
 * initialize all the SMP locks
 */

/* lock region used by kernel profiling */
int	mcount_lock;

#ifdef USE_COMLOCK
/* locks com (tty) data/hardware accesses: a FASTINTR() */
struct mtx		com_mtx;
#endif /* USE_COMLOCK */

static void
init_locks(void)
{

#ifdef USE_COMLOCK
	mtx_init(&com_mtx, "com", NULL, MTX_SPIN);
#endif /* USE_COMLOCK */
#ifdef APIC_IO
	mtx_init(&smp_tlb_mtx, "tlb", NULL, MTX_SPIN);
#endif
}

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
void
i386_mp_probe(void)
{
	int     x;
	u_long  segment;
	u_int32_t target;

	POSTCODE(MP_PROBE_POST);

	/* see if EBDA exists */
	if ((segment = (u_long) * (u_short *) (KERNBASE + 0x40e)) != 0) {
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
	return;

found:
	/* calculate needed resources */
	mpfps = (mpfps_t)x;
	mptable_pass1();

	/* flag fact that we are running multiple processors */
	mp_capable = 1;
}

int
cpu_mp_probe(void)
{
	/*
	 * Record BSP in CPU map
	 * This is done here so that MBUF init code works correctly.
	 */
	all_cpus = 1;

	return (mp_capable);
}

/*
 * Initialize the SMP hardware and the APIC and start up the AP's.
 */
void
cpu_mp_start(void)
{
	POSTCODE(MP_START_POST);

	/* look for MP capable motherboard */
	if (mp_capable)
		mp_enable(boot_address);
	else
		panic("MP hardware not found!");

	cpu_setregs();
}


/*
 * Print various information about the SMP system hardware and setup.
 */
void
cpu_mp_announce(void)
{
	int     x;

	POSTCODE(MP_ANNOUNCE_POST);

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
	int	x, myid = bootAP;
	u_int	cr0;

	gdt_segs[GPRIV_SEL].ssd_base = (int) &SMP_prvspace[myid];
	gdt_segs[GPROC0_SEL].ssd_base =
		(int) &SMP_prvspace[myid].pcpu.pc_common_tss;
	SMP_prvspace[myid].pcpu.pc_prvspace =
		&SMP_prvspace[myid].pcpu;

	for (x = 0; x < NGDT; x++) {
		ssdtosd(&gdt_segs[x], &gdt[myid * NGDT + x].sd);
	}

	r_gdt.rd_limit = NGDT * sizeof(gdt[0]) - 1;
	r_gdt.rd_base = (int) &gdt[myid * NGDT];
	lgdt(&r_gdt);			/* does magic intra-segment return */

	lidt(&r_idt);

	lldt(_default_ldt);
	PCPU_SET(currentldt, _default_ldt);

	gsel_tss = GSEL(GPROC0_SEL, SEL_KPL);
	gdt[myid * NGDT + GPROC0_SEL].sd.sd_type = SDT_SYS386TSS;
	PCPU_SET(common_tss.tss_esp0, 0); /* not used until after switch */
	PCPU_SET(common_tss.tss_ss0, GSEL(GDATA_SEL, SEL_KPL));
	PCPU_SET(common_tss.tss_ioopt, (sizeof (struct i386tss)) << 16);
	PCPU_SET(tss_gdt, &gdt[myid * NGDT + GPROC0_SEL].sd);
	PCPU_SET(common_tssd, *PCPU_GET(tss_gdt));
	ltr(gsel_tss);

	/*
	 * Set to a known state:
	 * Set by mpboot.s: CR0_PG, CR0_PE
	 * Set by cpu_setregs: CR0_NE, CR0_MP, CR0_TS, CR0_WP, CR0_AM
	 */
	cr0 = rcr0();
	cr0 &= ~(CR0_CD | CR0_NW | CR0_EM);
	load_cr0(cr0);

	pmap_set_opt();
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

	POSTCODE(MP_ENABLE_POST);

	/* turn on 4MB of V == P addressing so we can get to MP table */
	*(int *)PTD = PG_V | PG_RW | ((uintptr_t)(void *)KPTphys & PG_FRAME);
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
	setup_apic_irq_mapping();

#if defined(APIC_IO)

	/* fill the LOGICAL io_apic_versions table */
	for (apic = 0; apic < mp_napics; ++apic) {
		ux = io_apic_read(apic, IOAPIC_VER);
		io_apic_versions[apic] = ux;
		io_apic_set_id(apic, IO_TO_ID(apic));
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
	setidt(XINVLPG_OFFSET, Xinvlpg,
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(XINVLRNG_OFFSET, Xinvlrng,
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

	/* install an inter-CPU IPI for forwarding hardclock() */
	setidt(XHARDCLOCK_OFFSET, Xhardclock,
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	
	/* install an inter-CPU IPI for forwarding statclock() */
	setidt(XSTATCLOCK_OFFSET, Xstatclock,
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	
	/* install an inter-CPU IPI for all-CPU rendezvous */
	setidt(XRENDEZVOUS_OFFSET, Xrendezvous,
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

	/* install an inter-CPU IPI for forcing an additional software trap */
	setidt(XCPUAST_OFFSET, Xcpuast,
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
	u_char	int_vector;
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
	{MCA, "MCA"},
	{UNKNOWN_BUSTYPE, "---"},
	{ISA, "ISA"},
	{MCA, "MCA"},
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
	{1, 0, MCA, 255, 255},
	{2, 0, ISA, 1, PCI},
	{2, 0, EISA, 1, PCI},
	{2, 0, MCA, 1, PCI}
};


/* the bus data */
static bus_datum *bus_data;

/* the IO INT data, one entry per possible APIC INTerrupt */
static io_int  *io_apic_ints;

static int nintrs;

static int processor_entry(proc_entry_ptr entry, int cpu);
static int bus_entry(bus_entry_ptr entry, int bus);
static int io_apic_entry(io_apic_entry_ptr entry, int apic);
static int int_entry(int_entry_ptr entry, int intr);
static int lookup_bus_type(char *name);


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
static void
mptable_pass1(void)
{
	int	x;
	mpcth_t	cth;
	int	totalSize;
	void*	position;
	int	count;
	int	type;
	u_int	id_mask;

	POSTCODE(MPTABLE_PASS1_POST);

	/* clear various tables */
	for (x = 0; x < NAPICID; ++x) {
		io_apic_address[x] = ~0;	/* IO APIC address table */
	}

	/* init everything to empty */
	mp_naps = 0;
	mp_nbusses = 0;
	mp_napics = 0;
	nintrs = 0;
	id_mask = 0;

	/* check for use of 'default' configuration */
	if (MPFPS_MPFB1 != 0) {
		/* use default addresses */
		cpu_apic_address = DEFAULT_APIC_BASE;
		io_apic_address[0] = DEFAULT_IO_APIC_BASE;

		/* fill in with defaults */
		mp_naps = 2;		/* includes BSP */
		mp_maxid = 1;
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
				    & PROCENTRY_FLAG_EN) {
					++mp_naps;
					mp_maxid++;
					id_mask |= 1 <<
					    ((proc_entry_ptr)position)->apic_id;
				}
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
	if (mp_naps > MAXCPU) {
		printf("Warning: only using %d of %d available CPUs!\n",
			MAXCPU, mp_naps);
		mp_naps = MAXCPU;
	}

	/* See if we need to fixup HT logical CPUs. */
	mptable_hyperthread_fixup(id_mask);
	
	/*
	 * Count the BSP.
	 * This is also used as a counter while starting the APs.
	 */
	mp_ncpus = 1;

	--mp_naps;	/* subtract the BSP */
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
	struct PROCENTRY proc;
	int     x;
	mpcth_t cth;
	int     totalSize;
	void*   position;
	int     count;
	int     type;
	int     apic, bus, cpu, intr;
	int	i, j;
	int	pgeflag;

	POSTCODE(MPTABLE_PASS2_POST);

	/* Initialize fake proc entry for use with HT fixup. */
	bzero(&proc, sizeof(proc));
	proc.type = 0;
	proc.cpu_flags = PROCENTRY_FLAG_EN;

	pgeflag = 0;		/* XXX - Not used under SMP yet.  */

	MALLOC(io_apic_versions, u_int32_t *, sizeof(u_int32_t) * mp_napics,
	    M_DEVBUF, 0);
	MALLOC(ioapic, volatile ioapic_t **, sizeof(ioapic_t *) * mp_napics,
	    M_DEVBUF, 0);
	MALLOC(io_apic_ints, io_int *, sizeof(io_int) * (nintrs + 1),
	    M_DEVBUF, 0);
	MALLOC(bus_data, bus_datum *, sizeof(bus_datum) * mp_nbusses,
	    M_DEVBUF, 0);

	bzero(ioapic, sizeof(ioapic_t *) * mp_napics);

	for (i = 0; i < mp_napics; i++) {
		for (j = 0; j < mp_napics; j++) {
			/* same page frame as a previous IO apic? */
			if (((vm_offset_t)SMPpt[NPTEPG-2-j] & PG_FRAME) ==
			    (io_apic_address[i] & PG_FRAME)) {
				ioapic[i] = (ioapic_t *)((u_int)SMP_prvspace
					+ (NPTEPG-2-j) * PAGE_SIZE
					+ (io_apic_address[i] & PAGE_MASK));
				break;
			}
			/* use this slot if available */
			if (((vm_offset_t)SMPpt[NPTEPG-2-j] & PG_FRAME) == 0) {
				SMPpt[NPTEPG-2-j] = (pt_entry_t)(PG_V | PG_RW |
				    pgeflag | (io_apic_address[i] & PG_FRAME));
				ioapic[i] = (ioapic_t *)((u_int)SMP_prvspace
					+ (NPTEPG-2-j) * PAGE_SIZE
					+ (io_apic_address[i] & PAGE_MASK));
				break;
			}
		}
	}

	/* clear various tables */
	for (x = 0; x < NAPICID; ++x) {
		ID_TO_IO(x) = -1;	/* phy APIC ID to log CPU/IO table */
		CPU_TO_ID(x) = -1;	/* logical CPU to APIC ID table */
		IO_TO_ID(x) = -1;	/* logical IO to APIC ID table */
	}

	/* clear bus data table */
	for (x = 0; x < mp_nbusses; ++x)
		bus_data[x].bus_id = 0xff;

	/* clear IO APIC INT table */
	for (x = 0; x < (nintrs + 1); ++x) {
		io_apic_ints[x].int_type = 0xff;
		io_apic_ints[x].int_vector = 0xff;
	}

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

			if (need_hyperthreading_fixup) {
				/*
				 * Create fake mptable processor entries
				 * and feed them to processor_entry() to
				 * enumerate the logical CPUs.
				 */
				proc.apic_id = ((proc_entry_ptr)position)->apic_id;
				for (i = 1; i < logical_cpus; i++) {
					proc.apic_id++;
					(void)processor_entry(&proc, cpu);
					cpu++;
				}
			}
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
 * Check if we should perform a hyperthreading "fix-up" to
 * enumerate any logical CPU's that aren't already listed
 * in the table.
 *
 * XXX: We assume that all of the physical CPUs in the
 * system have the same number of logical CPUs.
 *
 * XXX: We assume that APIC ID's are allocated such that
 * the APIC ID's for a physical processor are aligned
 * with the number of logical CPU's in the processor.
 */
static void
mptable_hyperthread_fixup(u_int id_mask)
{
	u_int i, id;

	/* Nothing to do if there is no HTT support. */
	if ((cpu_feature & CPUID_HTT) == 0)
		return;
	logical_cpus = (cpu_procinfo & CPUID_HTT_CORES) >> 16;
	if (logical_cpus <= 1)
		return;

	/*
	 * For each APIC ID of a CPU that is set in the mask,
	 * scan the other candidate APIC ID's for this
	 * physical processor.  If any of those ID's are
	 * already in the table, then kill the fixup.
	 */
	for (id = 0; id <= MAXCPU; id++) {
		if ((id_mask & 1 << id) == 0)
			continue;
		/* First, make sure we are on a logical_cpus boundary. */
		if (id % logical_cpus != 0)
			return;
		for (i = id + 1; i < id + logical_cpus; i++)
			if ((id_mask & 1 << i) != 0)
				return;
	}

	/*
	 * Ok, the ID's checked out, so enable the fixup.  We have to fixup
	 * mp_naps and mp_maxid right now.
	 */
	need_hyperthreading_fixup = 1;
	mp_maxid *= logical_cpus;
	mp_naps *= logical_cpus;
}

void
assign_apic_irq(int apic, int intpin, int irq)
{
	int x;
	
	if (int_to_apicintpin[irq].ioapic != -1)
		panic("assign_apic_irq: inconsistent table");
	
	int_to_apicintpin[irq].ioapic = apic;
	int_to_apicintpin[irq].int_pin = intpin;
	int_to_apicintpin[irq].apic_address = ioapic[apic];
	int_to_apicintpin[irq].redirindex = IOAPIC_REDTBL + 2 * intpin;
	
	for (x = 0; x < nintrs; x++) {
		if ((io_apic_ints[x].int_type == 0 || 
		     io_apic_ints[x].int_type == 3) &&
		    io_apic_ints[x].int_vector == 0xff &&
		    io_apic_ints[x].dst_apic_id == IO_TO_ID(apic) &&
		    io_apic_ints[x].dst_apic_int == intpin)
			io_apic_ints[x].int_vector = irq;
	}
}

void
revoke_apic_irq(int irq)
{
	int x;
	int oldapic;
	int oldintpin;
	
	if (int_to_apicintpin[irq].ioapic == -1)
		panic("revoke_apic_irq: inconsistent table");
	
	oldapic = int_to_apicintpin[irq].ioapic;
	oldintpin = int_to_apicintpin[irq].int_pin;

	int_to_apicintpin[irq].ioapic = -1;
	int_to_apicintpin[irq].int_pin = 0;
	int_to_apicintpin[irq].apic_address = NULL;
	int_to_apicintpin[irq].redirindex = 0;
	
	for (x = 0; x < nintrs; x++) {
		if ((io_apic_ints[x].int_type == 0 || 
		     io_apic_ints[x].int_type == 3) &&
		    io_apic_ints[x].int_vector != 0xff &&
		    io_apic_ints[x].dst_apic_id == IO_TO_ID(oldapic) &&
		    io_apic_ints[x].dst_apic_int == oldintpin)
			io_apic_ints[x].int_vector = 0xff;
	}
}


static void
allocate_apic_irq(int intr)
{
	int apic;
	int intpin;
	int irq;
	
	if (io_apic_ints[intr].int_vector != 0xff)
		return;		/* Interrupt handler already assigned */
	
	if (io_apic_ints[intr].int_type != 0 &&
	    (io_apic_ints[intr].int_type != 3 ||
	     (io_apic_ints[intr].dst_apic_id == IO_TO_ID(0) &&
	      io_apic_ints[intr].dst_apic_int == 0)))
		return;		/* Not INT or ExtInt on != (0, 0) */
	
	irq = 0;
	while (irq < APIC_INTMAPSIZE &&
	       int_to_apicintpin[irq].ioapic != -1)
		irq++;
	
	if (irq >= APIC_INTMAPSIZE)
		return;		/* No free interrupt handlers */
	
	apic = ID_TO_IO(io_apic_ints[intr].dst_apic_id);
	intpin = io_apic_ints[intr].dst_apic_int;
	
	assign_apic_irq(apic, intpin, irq);
	io_apic_setup_intpin(apic, intpin);
}


static void
swap_apic_id(int apic, int oldid, int newid)
{
	int x;
	int oapic;
	

	if (oldid == newid)
		return;			/* Nothing to do */
	
	printf("Changing APIC ID for IO APIC #%d from %d to %d in MP table\n",
	       apic, oldid, newid);
	
	/* Swap physical APIC IDs in interrupt entries */
	for (x = 0; x < nintrs; x++) {
		if (io_apic_ints[x].dst_apic_id == oldid)
			io_apic_ints[x].dst_apic_id = newid;
		else if (io_apic_ints[x].dst_apic_id == newid)
			io_apic_ints[x].dst_apic_id = oldid;
	}
	
	/* Swap physical APIC IDs in IO_TO_ID mappings */
	for (oapic = 0; oapic < mp_napics; oapic++)
		if (IO_TO_ID(oapic) == newid)
			break;
	
	if (oapic < mp_napics) {
		printf("Changing APIC ID for IO APIC #%d from "
		       "%d to %d in MP table\n",
		       oapic, newid, oldid);
		IO_TO_ID(oapic) = oldid;
	}
	IO_TO_ID(apic) = newid;
}


static void
fix_id_to_io_mapping(void)
{
	int x;

	for (x = 0; x < NAPICID; x++)
		ID_TO_IO(x) = -1;
	
	for (x = 0; x <= mp_naps; x++)
		if (CPU_TO_ID(x) < NAPICID)
			ID_TO_IO(CPU_TO_ID(x)) = x;
	
	for (x = 0; x < mp_napics; x++)
		if (IO_TO_ID(x) < NAPICID)
			ID_TO_IO(IO_TO_ID(x)) = x;
}


static int
first_free_apic_id(void)
{
	int freeid, x;
	
	for (freeid = 0; freeid < NAPICID; freeid++) {
		for (x = 0; x <= mp_naps; x++)
			if (CPU_TO_ID(x) == freeid)
				break;
		if (x <= mp_naps)
			continue;
		for (x = 0; x < mp_napics; x++)
			if (IO_TO_ID(x) == freeid)
				break;
		if (x < mp_napics)
			continue;
		return freeid;
	}
	return freeid;
}


static int
io_apic_id_acceptable(int apic, int id)
{
	int cpu;		/* Logical CPU number */
	int oapic;		/* Logical IO APIC number for other IO APIC */

	if (id >= NAPICID)
		return 0;	/* Out of range */
	
	for (cpu = 0; cpu <= mp_naps; cpu++)
		if (CPU_TO_ID(cpu) == id)
			return 0;	/* Conflict with CPU */
	
	for (oapic = 0; oapic < mp_napics && oapic < apic; oapic++)
		if (IO_TO_ID(oapic) == id)
			return 0;	/* Conflict with other APIC */
	
	return 1;		/* ID is acceptable for IO APIC */
}


/*
 * parse an Intel MP specification table
 */
static void
fix_mp_table(void)
{
	int	x;
	int	id;
	int	bus_0 = 0;	/* Stop GCC warning */
	int	bus_pci = 0;	/* Stop GCC warning */
	int	num_pci_bus;
	int	apic;		/* IO APIC unit number */
	int     freeid;		/* Free physical APIC ID */
	int	physid;		/* Current physical IO APIC ID */

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
	/* if it is number 0 all is well */
	if (num_pci_bus == 1 &&
	    bus_data[bus_pci].bus_id != 0) {
		
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

	/* Assign IO APIC IDs.
	 * 
	 * First try the existing ID. If a conflict is detected, try
	 * the ID in the MP table.  If a conflict is still detected, find
	 * a free id.
	 *
	 * We cannot use the ID_TO_IO table before all conflicts has been
	 * resolved and the table has been corrected.
	 */
	for (apic = 0; apic < mp_napics; ++apic) { /* For all IO APICs */
		
		/* First try to use the value set by the BIOS */
		physid = io_apic_get_id(apic);
		if (io_apic_id_acceptable(apic, physid)) {
			if (IO_TO_ID(apic) != physid)
				swap_apic_id(apic, IO_TO_ID(apic), physid);
			continue;
		}

		/* Then check if the value in the MP table is acceptable */
		if (io_apic_id_acceptable(apic, IO_TO_ID(apic)))
			continue;

		/* Last resort, find a free APIC ID and use it */
		freeid = first_free_apic_id();
		if (freeid >= NAPICID)
			panic("No free physical APIC IDs found");
		
		if (io_apic_id_acceptable(apic, freeid)) {
			swap_apic_id(apic, IO_TO_ID(apic), freeid);
			continue;
		}
		panic("Free physical APIC ID not usable");
	}
	fix_id_to_io_mapping();

	/* detect and fix broken Compaq MP table */
	if (apic_int_type(0, 0) == -1) {
		printf("APIC_IO: MP table broken: 8259->APIC entry missing!\n");
		io_apic_ints[nintrs].int_type = 3;	/* ExtInt */
		io_apic_ints[nintrs].int_vector = 0xff;	/* Unassigned */
		/* XXX fixme, set src bus id etc, but it doesn't seem to hurt */
		io_apic_ints[nintrs].dst_apic_id = IO_TO_ID(0);
		io_apic_ints[nintrs].dst_apic_int = 0;	/* Pin 0 */
		nintrs++;
	}
}


/* Assign low level interrupt handlers */
static void
setup_apic_irq_mapping(void)
{
	int	x;
	int	int_vector;

	/* Clear array */
	for (x = 0; x < APIC_INTMAPSIZE; x++) {
		int_to_apicintpin[x].ioapic = -1;
		int_to_apicintpin[x].int_pin = 0;
		int_to_apicintpin[x].apic_address = NULL;
		int_to_apicintpin[x].redirindex = 0;
	}

	/* First assign ISA/EISA interrupts */
	for (x = 0; x < nintrs; x++) {
		int_vector = io_apic_ints[x].src_bus_irq;
		if (int_vector < APIC_INTMAPSIZE &&
		    io_apic_ints[x].int_vector == 0xff && 
		    int_to_apicintpin[int_vector].ioapic == -1 &&
		    (apic_int_is_bus_type(x, ISA) ||
		     apic_int_is_bus_type(x, EISA)) &&
		    io_apic_ints[x].int_type == 0) {
			assign_apic_irq(ID_TO_IO(io_apic_ints[x].dst_apic_id), 
					io_apic_ints[x].dst_apic_int,
					int_vector);
		}
	}

	/* Assign ExtInt entry if no ISA/EISA interrupt 0 entry */
	for (x = 0; x < nintrs; x++) {
		if (io_apic_ints[x].dst_apic_int == 0 &&
		    io_apic_ints[x].dst_apic_id == IO_TO_ID(0) &&
		    io_apic_ints[x].int_vector == 0xff && 
		    int_to_apicintpin[0].ioapic == -1 &&
		    io_apic_ints[x].int_type == 3) {
			assign_apic_irq(0, 0, 0);
			break;
		}
	}
	/* PCI interrupt assignment is deferred */
}


static int
processor_entry(proc_entry_ptr entry, int cpu)
{
	/* check for usability */
	if (!(entry->cpu_flags & PROCENTRY_FLAG_EN))
		return 0;

	if(entry->apic_id >= NAPICID)
		panic("CPU APIC ID out of range (0..%d)", NAPICID - 1);
	/* check for BSP flag */
	if (entry->cpu_flags & PROCENTRY_FLAG_BP) {
		boot_cpu_id = entry->apic_id;
		CPU_TO_ID(0) = entry->apic_id;
		ID_TO_CPU(entry->apic_id) = 0;
		return 0;	/* its already been counted */
	}

	/* add another AP to list, if less than max number of CPUs */
	else if (cpu < MAXCPU) {
		CPU_TO_ID(cpu) = entry->apic_id;
		ID_TO_CPU(entry->apic_id) = cpu;
		return 1;
	}

	return 0;
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
	if (entry->apic_id < NAPICID)
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
	int apic;

	io_apic_ints[intr].int_type = entry->int_type;
	io_apic_ints[intr].int_flags = entry->int_flags;
	io_apic_ints[intr].src_bus_id = entry->src_bus_id;
	io_apic_ints[intr].src_bus_irq = entry->src_bus_irq;
	if (entry->dst_apic_id == 255) {
		/* This signal goes to all IO APICS.  Select an IO APIC
		   with sufficient number of interrupt pins */
		for (apic = 0; apic < mp_napics; apic++)
			if (((io_apic_read(apic, IOAPIC_VER) & 
			      IOART_VER_MAXREDIR) >> MAXREDIRSHIFT) >= 
			    entry->dst_apic_int)
				break;
		if (apic < mp_napics)
			io_apic_ints[intr].dst_apic_id = IO_TO_ID(apic);
		else
			io_apic_ints[intr].dst_apic_id = entry->dst_apic_id;
	} else
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

	apic_pin = isa_apic_irq(isa_irq);	/* look for APIC connection */
	if (apic_pin == -1)
		return 0;

	return (1 << apic_pin);			/* convert pin# to a mask */
}


/*
 * Determine which APIC pin an ISA/EISA INT is attached to.
 */
#define INTTYPE(I)	(io_apic_ints[(I)].int_type)
#define INTPIN(I)	(io_apic_ints[(I)].dst_apic_int)
#define INTIRQ(I)	(io_apic_ints[(I)].int_vector)
#define INTAPIC(I)	(ID_TO_IO(io_apic_ints[(I)].dst_apic_id))

#define SRCBUSIRQ(I)	(io_apic_ints[(I)].src_bus_irq)
int
isa_apic_irq(int isa_irq)
{
	int     intr;

	for (intr = 0; intr < nintrs; ++intr) {		/* check each record */
		if (INTTYPE(intr) == 0) {		/* standard INT */
			if (SRCBUSIRQ(intr) == isa_irq) {
				if (apic_int_is_bus_type(intr, ISA) ||
			            apic_int_is_bus_type(intr, EISA)) {
					if (INTIRQ(intr) == 0xff)
						return -1; /* unassigned */
					return INTIRQ(intr);	/* found */
				}
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
pci_apic_irq(int pciBus, int pciDevice, int pciInt)
{
	int     intr;

	--pciInt;					/* zero based */

	for (intr = 0; intr < nintrs; ++intr)		/* check each record */
		if ((INTTYPE(intr) == 0)		/* standard INT */
		    && (SRCBUSID(intr) == pciBus)
		    && (SRCBUSDEVICE(intr) == pciDevice)
		    && (SRCBUSLINE(intr) == pciInt))	/* a candidate IRQ */
			if (apic_int_is_bus_type(intr, PCI)) {
				if (INTIRQ(intr) == 0xff)
					allocate_apic_irq(intr);
				if (INTIRQ(intr) == 0xff)
					return -1;	/* unassigned */
				return INTIRQ(intr);	/* exact match */
			}

	return -1;					/* NOT found */
}

int
next_apic_irq(int irq) 
{
	int intr, ointr;
	int bus, bustype;

	bus = 0;
	bustype = 0;
	for (intr = 0; intr < nintrs; intr++) {
		if (INTIRQ(intr) != irq || INTTYPE(intr) != 0)
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
	return INTIRQ(ointr);
}
#undef SRCBUSLINE
#undef SRCBUSDEVICE
#undef SRCBUSID
#undef SRCBUSIRQ

#undef INTPIN
#undef INTIRQ
#undef INTAPIC
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
	if (bootverbose)
	    printf("Freeing redirected ISA irq %d.\n", rirq);
	/** FIXME: tickle the MB redirector chip */
	return -1;
#else
	if (bootverbose)
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
	return -1;
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

int 
apic_irq(int apic, int pin)
{
	int x;
	int res;

	for (x = 0; x < nintrs; ++x)
		if ((apic == ID_TO_IO(io_apic_ints[x].dst_apic_id)) &&
		    (pin == io_apic_ints[x].dst_apic_int)) {
			res = io_apic_ints[x].int_vector;
			if (res == 0xff)
				return -1;
			if (apic != int_to_apicintpin[res].ioapic)
				panic("apic_irq: inconsistent table");
			if (pin != int_to_apicintpin[res].int_pin)
				panic("apic_irq inconsistent table (2)");
			return res;
		}
	return -1;
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
		io_apic_set_id(0, 2);
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
	case 4:
	case 5:
	case 6:
	case 7:
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
 * start each AP in our list
 */
static int
start_all_aps(u_int boot_addr)
{
	int     x, i, pg;
	u_char  mpbiosreason;
	u_long  mpbioswarmvec;
	struct pcpu *pc;
	char *stack;
	uintptr_t kptbase;

	POSTCODE(START_ALL_APS_POST);

	mtx_init(&ap_boot_mtx, "ap boot", NULL, MTX_SPIN);

	/* initialize BSP's local APIC */
	apic_initialize();
	bsp_apic_ready = 1;

	/* install the AP 1st level boot code */
	install_ap_tramp(boot_addr);


	/* save the current value of the warm-start vector */
	mpbioswarmvec = *((u_long *) WARMBOOT_OFF);
#ifndef PC98
	outb(CMOS_REG, BIOS_RESET);
	mpbiosreason = inb(CMOS_DATA);
#endif

	/* set up temporary P==V mapping for AP boot */
	/* XXX this is a hack, we should boot the AP on its own stack/PTD */
	kptbase = (uintptr_t)(void *)KPTphys;
	for (x = 0; x < NKPT; x++)
		PTD[x] = (pd_entry_t)(PG_V | PG_RW |
		    ((kptbase + x * PAGE_SIZE) & PG_FRAME));
	invltlb();

	/* start each AP */
	for (x = 1; x <= mp_naps; ++x) {

		/* This is a bit verbose, it will go away soon.  */

		/* first page of AP's private space */
		pg = x * i386_btop(sizeof(struct privatespace));

		/* allocate a new private data page */
		pc = (struct pcpu *)kmem_alloc(kernel_map, PAGE_SIZE);

		/* wire it into the private page table page */
		SMPpt[pg] = (pt_entry_t)(PG_V | PG_RW | vtophys(pc));

		/* allocate and set up an idle stack data page */
		stack = (char *)kmem_alloc(kernel_map, KSTACK_PAGES * PAGE_SIZE); /* XXXKSE */
		for (i = 0; i < KSTACK_PAGES; i++)
			SMPpt[pg + 1 + i] = (pt_entry_t)
			    (PG_V | PG_RW | vtophys(PAGE_SIZE * i + stack));

		/* prime data page for it to use */
		pcpu_init(pc, x, sizeof(struct pcpu));

		/* setup a vector to our boot code */
		*((volatile u_short *) WARMBOOT_OFF) = WARMBOOT_TARGET;
		*((volatile u_short *) WARMBOOT_SEG) = (boot_addr >> 4);
#ifndef PC98
		outb(CMOS_REG, BIOS_RESET);
		outb(CMOS_DATA, BIOS_WARM);	/* 'warm-start' */
#endif

		bootSTK = &SMP_prvspace[x].idlekstack[KSTACK_PAGES * PAGE_SIZE];
		bootAP = x;

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
	PCPU_SET(other_cpus, all_cpus & ~PCPU_GET(cpumask));

	/* fill in our (BSP) APIC version */
	cpu_apic_versions[0] = lapic.version;

	/* restore the warmstart vector */
	*(u_long *) WARMBOOT_OFF = mpbioswarmvec;
#ifndef PC98
	outb(CMOS_REG, BIOS_RESET);
	outb(CMOS_DATA, mpbiosreason);
#endif

	/*
	 * Set up the idle context for the BSP.  Similar to above except
	 * that some was done by locore, some by pmap.c and some is implicit
	 * because the BSP is cpu#0 and the page is initially zero, and also
	 * because we can refer to variables by name on the BSP..
	 */

	/* Allocate and setup BSP idle stack */
	stack = (char *)kmem_alloc(kernel_map, KSTACK_PAGES * PAGE_SIZE);
	for (i = 0; i < KSTACK_PAGES; i++)
		SMPpt[1 + i] = (pt_entry_t)
		    (PG_V | PG_RW | vtophys(PAGE_SIZE * i + stack));

	for (x = 0; x < NKPT; x++)
		PTD[x] = 0;
	pmap_set_opt();

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

#if defined(APIC_IO)

#ifdef COUNT_XINVLTLB_HITS
u_int xhits_gbl[MAXCPU];
u_int xhits_pg[MAXCPU];
u_int xhits_rng[MAXCPU];
SYSCTL_NODE(_debug, OID_AUTO, xhits, CTLFLAG_RW, 0, "");
SYSCTL_OPAQUE(_debug_xhits, OID_AUTO, global, CTLFLAG_RW, &xhits_gbl,
    sizeof(xhits_gbl), "IU", "");
SYSCTL_OPAQUE(_debug_xhits, OID_AUTO, page, CTLFLAG_RW, &xhits_pg,
    sizeof(xhits_pg), "IU", "");
SYSCTL_OPAQUE(_debug_xhits, OID_AUTO, range, CTLFLAG_RW, &xhits_rng,
    sizeof(xhits_rng), "IU", "");

u_int ipi_global;
u_int ipi_page;
u_int ipi_range;
u_int ipi_range_size;
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_global, CTLFLAG_RW, &ipi_global, 0, "");
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_page, CTLFLAG_RW, &ipi_page, 0, "");
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_range, CTLFLAG_RW, &ipi_range, 0, "");
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_range_size, CTLFLAG_RW, &ipi_range_size,
    0, "");

u_int ipi_masked_global;
u_int ipi_masked_page;
u_int ipi_masked_range;
u_int ipi_masked_range_size;
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_masked_global, CTLFLAG_RW,
    &ipi_masked_global, 0, "");
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_masked_page, CTLFLAG_RW,
    &ipi_masked_page, 0, "");
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_masked_range, CTLFLAG_RW,
    &ipi_masked_range, 0, "");
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_masked_range_size, CTLFLAG_RW,
    &ipi_masked_range_size, 0, "");
#endif

/*
 * Flush the TLB on all other CPU's
 */
static void
smp_tlb_shootdown(u_int vector, vm_offset_t addr1, vm_offset_t addr2)
{
	u_int ncpu;
	register_t eflags;

	ncpu = mp_ncpus - 1;	/* does not shootdown self */
	if (ncpu < 1)
		return;		/* no other cpus */
	eflags = read_eflags();
	if ((eflags & PSL_I) == 0)
		panic("absolutely cannot call smp_ipi_shootdown with interrupts already disabled");
	mtx_lock_spin(&smp_tlb_mtx);
	smp_tlb_addr1 = addr1;
	smp_tlb_addr2 = addr2;
	atomic_store_rel_int(&smp_tlb_wait, 0);
	ipi_all_but_self(vector);
	while (smp_tlb_wait < ncpu)
		ia32_pause();
	mtx_unlock_spin(&smp_tlb_mtx);
}

/*
 * This is about as magic as it gets.  fortune(1) has got similar code
 * for reversing bits in a word.  Who thinks up this stuff??
 *
 * Yes, it does appear to be consistently faster than:
 * while (i = ffs(m)) {
 *	m >>= i;
 *	bits++;
 * }
 * and
 * while (lsb = (m & -m)) {	// This is magic too
 * 	m &= ~lsb;		// or: m ^= lsb
 *	bits++;
 * }
 * Both of these latter forms do some very strange things on gcc-3.1 with
 * -mcpu=pentiumpro and/or -march=pentiumpro and/or -O or -O2.
 * There is probably an SSE or MMX popcnt instruction.
 *
 * I wonder if this should be in libkern?
 *
 * XXX Stop the presses!  Another one:
 * static __inline u_int32_t
 * popcnt1(u_int32_t v)
 * {
 *	v -= ((v >> 1) & 0x55555555);
 *	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
 *	v = (v + (v >> 4)) & 0x0F0F0F0F;
 *	return (v * 0x01010101) >> 24;
 * }
 * The downside is that it has a multiply.  With a pentium3 with
 * -mcpu=pentiumpro and -march=pentiumpro then gcc-3.1 will use
 * an imull, and in that case it is faster.  In most other cases
 * it appears slightly slower.
 */
static __inline u_int32_t
popcnt(u_int32_t m)
{

	m = (m & 0x55555555) + ((m & 0xaaaaaaaa) >> 1);
	m = (m & 0x33333333) + ((m & 0xcccccccc) >> 2);
	m = (m & 0x0f0f0f0f) + ((m & 0xf0f0f0f0) >> 4);
	m = (m & 0x00ff00ff) + ((m & 0xff00ff00) >> 8);
	m = (m & 0x0000ffff) + ((m & 0xffff0000) >> 16);
	return m;
}

static void
smp_targeted_tlb_shootdown(u_int mask, u_int vector, vm_offset_t addr1, vm_offset_t addr2)
{
	int ncpu, othercpus;
	register_t eflags;

	othercpus = mp_ncpus - 1;
	if (mask == (u_int)-1) {
		ncpu = othercpus;
		if (ncpu < 1)
			return;
	} else {
		/* XXX there should be a pcpu self mask */
		mask &= ~(1 << PCPU_GET(cpuid));
		if (mask == 0)
			return;
		ncpu = popcnt(mask);
		if (ncpu > othercpus) {
			/* XXX this should be a panic offence */
			printf("SMP: tlb shootdown to %d other cpus (only have %d)\n",
			    ncpu, othercpus);
			ncpu = othercpus;
		}
		/* XXX should be a panic, implied by mask == 0 above */
		if (ncpu < 1)
			return;
	}
	eflags = read_eflags();
	if ((eflags & PSL_I) == 0)
		panic("absolutely cannot call smp_targeted_ipi_shootdown with interrupts already disabled");
	mtx_lock_spin(&smp_tlb_mtx);
	smp_tlb_addr1 = addr1;
	smp_tlb_addr2 = addr2;
	atomic_store_rel_int(&smp_tlb_wait, 0);
	if (mask == (u_int)-1)
		ipi_all_but_self(vector);
	else
		ipi_selected(mask, vector);
	while (smp_tlb_wait < ncpu)
		ia32_pause();
	mtx_unlock_spin(&smp_tlb_mtx);
}
#endif

void
smp_invltlb(void)
{
#if defined(APIC_IO)
	if (smp_started) {
		smp_tlb_shootdown(IPI_INVLTLB, 0, 0);
#ifdef COUNT_XINVLTLB_HITS
		ipi_global++;
#endif
	}
#endif  /* APIC_IO */
}

void
smp_invlpg(vm_offset_t addr)
{
#if defined(APIC_IO)
	if (smp_started) {
		smp_tlb_shootdown(IPI_INVLPG, addr, 0);
#ifdef COUNT_XINVLTLB_HITS
		ipi_page++;
#endif
	}
#endif  /* APIC_IO */
}

void
smp_invlpg_range(vm_offset_t addr1, vm_offset_t addr2)
{
#if defined(APIC_IO)
	if (smp_started) {
		smp_tlb_shootdown(IPI_INVLRNG, addr1, addr2);
#ifdef COUNT_XINVLTLB_HITS
		ipi_range++;
		ipi_range_size += (addr2 - addr1) / PAGE_SIZE;
#endif
	}
#endif  /* APIC_IO */
}

void
smp_masked_invltlb(u_int mask)
{
#if defined(APIC_IO)
	if (smp_started) {
		smp_targeted_tlb_shootdown(mask, IPI_INVLTLB, 0, 0);
#ifdef COUNT_XINVLTLB_HITS
		ipi_masked_global++;
#endif
	}
#endif  /* APIC_IO */
}

void
smp_masked_invlpg(u_int mask, vm_offset_t addr)
{
#if defined(APIC_IO)
	if (smp_started) {
		smp_targeted_tlb_shootdown(mask, IPI_INVLPG, addr, 0);
#ifdef COUNT_XINVLTLB_HITS
		ipi_masked_page++;
#endif
	}
#endif  /* APIC_IO */
}

void
smp_masked_invlpg_range(u_int mask, vm_offset_t addr1, vm_offset_t addr2)
{
#if defined(APIC_IO)
	if (smp_started) {
		smp_targeted_tlb_shootdown(mask, IPI_INVLRNG, addr1, addr2);
#ifdef COUNT_XINVLTLB_HITS
		ipi_masked_range++;
		ipi_masked_range_size += (addr2 - addr1) / PAGE_SIZE;
#endif
	}
#endif  /* APIC_IO */
}


/*
 * This is called once the rest of the system is up and running and we're
 * ready to let the AP's out of the pen.
 */
void
ap_init(void)
{
	u_int	apic_id;

	/* spin until all the AP's are ready */
	while (!aps_ready)
		ia32_pause();

	/* BSP may have changed PTD while we were waiting */
	invltlb();

#if defined(I586_CPU) && !defined(NO_F00F_HACK)
	lidt(&r_idt);
#endif

	/* set up CPU registers and state */
	cpu_setregs();

	/* set up FPU state on the AP */
	npxinit(__INITIAL_NPXCW__);

	/* set up SSE registers */
	enable_sse();

	/* A quick check from sanity claus */
	apic_id = (apic_id_to_logical[(lapic.id & 0x0f000000) >> 24]);
	if (PCPU_GET(cpuid) != apic_id) {
		printf("SMP: cpuid = %d\n", PCPU_GET(cpuid));
		printf("SMP: apic_id = %d\n", apic_id);
		printf("PTD[MPPTDI] = %p\n", (void *)PTD[MPPTDI]);
		panic("cpuid mismatch! boom!!");
	}

	/* Init local apic for irq's */
	apic_initialize();

	/* Set memory range attributes for this CPU to match the BSP */
	mem_range_AP_init();

	mtx_lock_spin(&ap_boot_mtx);

	smp_cpus++;

	CTR1(KTR_SMP, "SMP: AP CPU #%d Launched", PCPU_GET(cpuid));
	printf("SMP: AP CPU #%d Launched!\n", PCPU_GET(cpuid));

	/* Build our map of 'other' CPUs. */
	PCPU_SET(other_cpus, all_cpus & ~PCPU_GET(cpumask));

	if (bootverbose)
		apic_dump("ap_init()");

	if (smp_cpus == mp_ncpus) {
		/* enable IPI's, tlb shootdown, freezes etc */
		atomic_store_rel_int(&smp_started, 1);
		smp_active = 1;	 /* historic */
	}

	mtx_unlock_spin(&ap_boot_mtx);

	/* wait until all the AP's are up */
	while (smp_started == 0)
		ia32_pause();

	/* ok, now grab sched_lock and enter the scheduler */
	mtx_lock_spin(&sched_lock);

	binuptime(PCPU_PTR(switchtime));
	PCPU_SET(switchticks, ticks);

	cpu_throw();	/* doesn't return */

	panic("scheduler returned us to %s", __func__);
}

/*
 * For statclock, we send an IPI to all CPU's to have them call this
 * function.
 *
 * WARNING! unpend() will call statclock() directly and skip this
 * routine.
 */
void
forwarded_statclock(struct clockframe frame)
{

	if (profprocs != 0)
		profclock(&frame);
	if (pscnt == psdiv)
		statclock(&frame);
}

void
forward_statclock(void)
{
	int map;

	CTR0(KTR_SMP, "forward_statclock");

	if (!smp_started || cold || panicstr)
		return;

	map = PCPU_GET(other_cpus) & ~stopped_cpus ;
	if (map != 0)
		ipi_selected(map, IPI_STATCLOCK);
}

/*
 * For each hardclock(), we send an IPI to all other CPU's to have them
 * execute this function.  It would be nice to reduce contention on
 * sched_lock if we could simply peek at the CPU to determine the user/kernel
 * state and call hardclock_process() on the CPU receiving the clock interrupt
 * and then just use a simple IPI to handle any ast's if needed.
 *
 * WARNING! unpend() will call hardclock_process() directly and skip this
 * routine.
 */
void
forwarded_hardclock(struct clockframe frame)
{

	hardclock_process(&frame);
}

void 
forward_hardclock(void)
{
	u_int map;

	CTR0(KTR_SMP, "forward_hardclock");

	if (!smp_started || cold || panicstr)
		return;

	map = PCPU_GET(other_cpus) & ~stopped_cpus ;
	if (map != 0)
		ipi_selected(map, IPI_HARDCLOCK);
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

/*
 * send an IPI to a set of cpus.
 */
void
ipi_selected(u_int32_t cpus, u_int ipi)
{

	CTR3(KTR_SMP, "%s: cpus: %x ipi: %x", __func__, cpus, ipi);
	selected_apic_ipi(cpus, ipi, APIC_DELMODE_FIXED);
}

/*
 * send an IPI INTerrupt containing 'vector' to all CPUs, including myself
 */
void
ipi_all(u_int ipi)
{

	CTR2(KTR_SMP, "%s: ipi: %x", __func__, ipi);
	apic_ipi(APIC_DEST_ALLISELF, ipi, APIC_DELMODE_FIXED); 
}

/*
 * send an IPI to all CPUs EXCEPT myself
 */
void
ipi_all_but_self(u_int ipi)
{

	CTR2(KTR_SMP, "%s: ipi: %x", __func__, ipi);
	apic_ipi(APIC_DEST_ALLESELF, ipi, APIC_DELMODE_FIXED); 
}

/*
 * send an IPI to myself
 */
void
ipi_self(u_int ipi)
{

	CTR2(KTR_SMP, "%s: ipi: %x", __func__, ipi);
	apic_ipi(APIC_DEST_SELF, ipi, APIC_DELMODE_FIXED); 
}

static void
release_aps(void *dummy __unused)
{

	mtx_lock_spin(&sched_lock);
	atomic_store_rel_int(&aps_ready, 1);
	while (smp_started == 0)
		ia32_pause();
	mtx_unlock_spin(&sched_lock);
}

SYSINIT(start_aps, SI_SUB_SMP, SI_ORDER_FIRST, release_aps, NULL);
