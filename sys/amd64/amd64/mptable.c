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
 *	$Id: mp_machdep.c,v 1.22 1997/06/25 21:01:52 fsmp Exp $
 */

#include "opt_smp.h"

#include <sys/param.h>		/* for KERNBASE */
#include <sys/types.h>
#include <sys/sysproto.h>
#include <sys/time.h>
#include <sys/systm.h>

#include <vm/vm.h>		/* for KERNBASE */
#include <vm/vm_param.h>	/* for KERNBASE */
#include <vm/pmap.h>		/* for KERNBASE */
#include <machine/pmap.h>	/* for KERNBASE */
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>

#include <machine/smp.h>
#include <machine/apic.h>
#include <machine/mpapic.h>
#include <machine/cpufunc.h>
#include <machine/segments.h>
#include <machine/smptests.h>	/** TEST_DEFAULT_CONFIG, TEST_CPUSTOP */
#include <machine/tss.h>
#include <machine/specialreg.h>

#include <i386/i386/cons.h>	/* cngetc() */

#if defined(APIC_IO)
#include <machine/md_var.h>	/* setidt() */
#include <i386/isa/icu.h>		/* Xinvltlb() */
#include <i386/isa/intr_machdep.h>	/* Xinvltlb() */
#endif	/* APIC_IO */

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
#ifndef POSTCODE
#define POSTCODE(X)
#endif

#define MP_BOOTADDRESS_POST	0x10
#define MP_PROBE_POST		0x11
#define MP_START_POST		0x12
#define MP_ANNOUNCE_POST	0x13
#define MPTABLE_PASS1_POST	0x14
#define MPTABLE_PASS2_POST	0x15
#define MP_ENABLE_POST		0x16
#define START_ALL_APS_POST	0x17
#define INSTALL_AP_TRAMP_POST	0x18
#define START_AP_POST		0x19

/** FIXME: what system files declare these??? */
extern struct region_descriptor r_gdt, r_idt;

int	mp_ncpus;		/* # of CPUs, including BSP */
int	mp_naps;		/* # of Applications processors */
int	mp_nbusses;		/* # of busses */
int	mp_napics;		/* # of IO APICs */
int	boot_cpu_id;		/* designated BSP */
vm_offset_t cpu_apic_address;
vm_offset_t io_apic_address[NAPICID];	/* NAPICID is more than enough */

u_int32_t cpu_apic_versions[NCPU];
u_int32_t io_apic_versions[NAPIC];

/*
 * APIC ID logical/physical mapping structures.
 * We oversize these to simplify boot-time config.
 */
int     cpu_num_to_apic_id[NAPICID];
int     io_num_to_apic_id[NAPICID];
int     apic_id_to_logical[NAPICID];

/* Bitmap of all available CPUs */
u_int	all_cpus;

/* Boot of AP uses this PTD */
u_int *bootPTD;

/* Hotwire a 0->4MB V==P mapping */
extern pt_entry_t KPTphys;

/* virtual address of per-cpu common_tss */
extern struct i386tss common_tss;

/*
 * look for MP compliant motherboard.
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
static int	start_all_aps(u_int boot_addr);
static void	install_ap_tramp(u_int boot_addr);
static int	start_ap(int logicalCpu, u_int boot_addr);


/*
 * calculate usable address in base memory for AP trampoline code
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

found:				/* please forgive the 'goto'! */
	/* calculate needed resources */
	mpfps = (mpfps_t)x;
	if (mptable_pass1())
		panic("you must reconfigure your kernel");

	/* flag fact that we are running multiple processors */
	mp_capable = 1;
	return 1;
}


/*
 * startup the SMP processors
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
 * print various information about the SMP system hardware and setup
 */
void
mp_announce(void)
{
	int     x;

	POSTCODE(MP_ANNOUNCE_POST);

	printf("FreeBSD/SMP: Multiprocessor motherboard\n");
	printf(" cpu0 (BSP): apic id: %d", CPU_TO_ID(0));
	printf(", version: 0x%08x", cpu_apic_versions[0]);
	printf(", at 0x%08x\n", cpu_apic_address);
	for (x = 1; x <= mp_naps; ++x) {
		printf(" cpu%d (AP):  apic id: %d", x, CPU_TO_ID(x));
		printf(", version: 0x%08x", cpu_apic_versions[x]);
		printf(", at 0x%08x\n", cpu_apic_address);
	}

#if defined(APIC_IO)
	for (x = 0; x < mp_napics; ++x) {
		printf(" io%d (APIC): apic id: %d", x, IO_TO_ID(x));
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
	int     gsel_tss, slot;

	r_gdt.rd_limit = sizeof(gdt[0]) * (NGDT + NCPU) - 1;
	r_gdt.rd_base = (int) gdt;
	lgdt(&r_gdt);		/* does magic intra-segment return */
	lidt(&r_idt);
	lldt(_default_ldt);

	slot = NGDT + cpuid;
	gsel_tss = GSEL(slot, SEL_KPL);
	gdt[slot].sd.sd_type = SDT_SYS386TSS;
	common_tss.tss_esp0 = 0;	/* not used until after switch */
	common_tss.tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);
	common_tss.tss_ioopt = (sizeof common_tss) << 16;
	ltr(gsel_tss);

	load_cr0(0x8005003b);	/* XXX! */

	PTD[0] = 0;
	invltlb();
}


#if defined(APIC_IO)
void
configure_local_apic(void)
{
	u_char  byte;
	u_int32_t temp;

	if (picmode) {
		outb(0x22, 0x70);	/* select IMCR */
		byte = inb(0x23);	/* current contents */
		byte |= 0x01;	/* mask external INTR */
		outb(0x23, byte);	/* disconnect 8259s/NMI */
	}

	/* mask lint0 (the 8259 'virtual wire' connection) */
	temp = lapic.lvt_lint0;
	temp |= APIC_LVT_M;
	lapic.lvt_lint0 = temp;

        /* setup lint1 to handle NMI */
#if 1
        /** XXX FIXME: 
         *      should we arrange for ALL CPUs to catch NMI???
         *      it would probably crash, so for now only the BSP
         *      will catch it
         */
        if (cpuid != 0)
                return;
#endif /* 0/1 */

        temp = lapic.lvt_lint1;

        /* clear fields of interest, preserve undefined fields */
        temp &= ~(0x1f000 | APIC_LVT_DM | APIC_LVT_VECTOR);

        /* setup for NMI, edge trigger, active hi */
        temp |= (APIC_LVT_DM_NMI | APIC_LVT_IIPP_INTAHI);

        lapic.lvt_lint1 = temp;
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

	/* Turn on 4MB of V == P addressing so we can get to MP table */
	*(int *)PTD = PG_V | PG_RW | ((u_long)KPTphys & PG_FRAME);
	invltlb();

	/* examine the MP table for needed info, uses physical addresses */
	x = mptable_pass2();

	*(int *)PTD = 0;
	invltlb();

	/* can't process default configs till the CPU APIC is pmapped */
	if (x)
		default_mp_table(x);

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

	/* install an inter-CPU IPI for TLB invalidation */
	setidt(XINVLTLB_OFFSET, Xinvltlb,
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

#if defined(TEST_CPUSTOP)
	/* install an inter-CPU IPI for CPU stop/restart */
	setidt(XCPUSTOP_OFFSET, Xcpustop,
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#endif  /* TEST_CPUSTOP */
#endif	/* APIC_IO */

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
bus_datum bus_data[NBUS];

/* the IO INT data, one entry per possible APIC INTerrupt */
io_int  io_apic_ints[NINTR];

static int nintrs;

static void fix_mp_table	__P((void));
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
	if (mpfps->mpfb1 != 0) {
		/* use default addresses */
		cpu_apic_address = DEFAULT_APIC_BASE;
		io_apic_address[0] = DEFAULT_IO_APIC_BASE;

		/* fill in with defaults */
		mp_naps = 2;		/* includes BSP */
		mp_nbusses = default_data[mpfps->mpfb1 - 1][0];
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
		printf("Warning: only using %d of %d available CPUs!\n",
			NCPU, mp_naps);
#if 0
		/** XXX we consider this legal now (but should we?) */
		mustpanic = 1;
#endif
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
#if defined(TEST_DEFAULT_CONFIG)
	return TEST_DEFAULT_CONFIG;
#else
	if (mpfps->mpfb1 != 0)
		return mpfps->mpfb1;	/* return default configuration type */
#endif	/* TEST_DEFAULT_CONFIG */

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

	/* post scan cleanup */
	fix_mp_table();

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

#if defined(SMP_TIMER_NC)
	if (isa_irq == 0)
		return -1;
#endif	/* SMP_TIMER_NC */

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
#undef SRCBUSIRQ


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
#undef SRCBUSLINE
#undef SRCBUSDEVICE
#undef SRCBUSID

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
		io_apic_ints[pin].int_flags = 0x05;	/* edge-triggered/active-hi */
		io_apic_ints[pin].src_bus_id = 0;
		io_apic_ints[pin].src_bus_irq = pin;	/* IRQ2 is caught below */
		io_apic_ints[pin].dst_apic_id = io_apic_id;
		io_apic_ints[pin].dst_apic_int = pin;	/* 1-to-1 correspondence */
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
	int     x, i;
	u_char  mpbiosreason;
	u_long  mpbioswarmvec;
	pd_entry_t newptd;
	pt_entry_t newpt;
	int *newpp;

	POSTCODE(START_ALL_APS_POST);

	/**
         * NOTE: this needs further thought:
         *        where does it get released?
         *        should it be set to empy?
         *
         * get the initial mp_lock with a count of 1 for the BSP
         */
	mp_lock = 1;	/* this uses a LOGICAL cpu ID, ie BSP == 0 */

	/* initialize BSP's local APIC */
	apic_initialize(1);

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

		/* HACK HACK HACK !!! */

		/* alloc new page table directory */
		newptd = (pd_entry_t)(kmem_alloc(kernel_map, PAGE_SIZE));

		/* clone currently active one (ie: IdlePTD) */
		bcopy(PTD, newptd, PAGE_SIZE);	/* inc prv page pde */

		/* set up 0 -> 4MB P==V mapping for AP boot */
		newptd[0] = PG_V | PG_RW | ((u_long)KPTphys & PG_FRAME);

		/* store PTD for this AP */
		bootPTD = (pd_entry_t)vtophys(newptd);

		/* alloc new page table page */
		newpt = (pt_entry_t)(kmem_alloc(kernel_map, PAGE_SIZE));

		/* set the new PTD's private page to point there */
		newptd[MPPTDI] = PG_V | PG_RW | vtophys(newpt);

		/* install self referential entry */
		newptd[PTDPTDI] = PG_V | PG_RW | vtophys(newptd);

		/* get a new private data page */
		newpp = (int *)kmem_alloc(kernel_map, PAGE_SIZE);

		/* wire it into the private page table page */
		newpt[0] = PG_V | PG_RW | vtophys(newpp);

		/* wire the ptp into itself for access */
		newpt[1] = PG_V | PG_RW | vtophys(newpt);

		/* and the local apic */
		newpt[2] = SMP_prvpt[2];

		/* and the IO apic mapping[s] */
		for (i = 16; i < 32; i++)
			newpt[i] = SMP_prvpt[i];

		/* prime data page for it to use */
		newpp[0] = x;		/* cpuid */
		newpp[1] = 0;		/* curproc */
		newpp[2] = 0;		/* curpcb */
		newpp[3] = 0;		/* npxproc */
		newpp[4] = 0;		/* runtime.tv_sec */
		newpp[5] = 0;		/* runtime.tv_usec */
		newpp[6] = x << 24;	/* cpu_lockid */

		/* XXX NOTE: ABANDON bootPTD for now!!!! */

		/* END REVOLTING HACKERY */

		/* setup a vector to our boot code */
		*((volatile u_short *) WARMBOOT_OFF) = WARMBOOT_TARGET;
		*((volatile u_short *) WARMBOOT_SEG) = (boot_addr >> 4);
		outb(CMOS_REG, BIOS_RESET);
		outb(CMOS_DATA, BIOS_WARM);	/* 'warm-start' */

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
	if (smp_active && invltlb_ok)
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


#if defined(TEST_CPUSTOP)

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
stop_cpus( u_int map )
{
	if (!smp_active)
		return 0;

	stopped_cpus = 0;

	/* send IPI to all CPUs in map */
#if defined(DEBUG_CPUSTOP)
	db_printf("\nCPU%d stopping CPUs: 0x%08x\n", cpuid, map);
#endif /* DEBUG_CPUSTOP */
#if 0
	selected_apic_ipi(map, XCPUSTOP_OFFSET, APIC_DELMODE_FIXED);
#else
	all_but_self_ipi(XCPUSTOP_OFFSET);
#endif

#if defined(DEBUG_CPUSTOP)
	db_printf(" stopped_cpus: 0x%08x, map: 0x%08x, spin\n",
	    	  stopped_cpus, map);
#endif /* DEBUG_CPUSTOP */

	while (stopped_cpus != map) {
#if 0
		/* spin */ ;
#else
		POSTCODE(stopped_cpus & 0xff);
#endif
	}

#if defined(DEBUG_CPUSTOP)
	db_printf("  spun\nstopped\n");
#endif /* DEBUG_CPUSTOP */

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
restart_cpus( u_int map )
{
	if (!smp_active)
		return 0;

	started_cpus = map;		/* signal other cpus to restart */

#if defined(DEBUG_CPUSTOP)
	db_printf("\nCPU%d restarting CPUs: 0x%08x (0x%08x)\n",
	       cpuid, started_cpus, stopped_cpus);
#endif /* DEBUG_CPUSTOP */

	while (started_cpus)		/* wait for each to clear its bit */
		/* spin */ ;

#if defined(DEBUG_CPUSTOP)
	db_printf(" restarted\n");
#endif /* DEBUG_CPUSTOP */

	return 1;
}

#endif  /* TEST_CPUSTOP */
