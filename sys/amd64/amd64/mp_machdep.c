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
 *	$Id: mp_machdep.c,v 1.1 1997/04/26 11:45:15 peter Exp $
 */

#include "opt_smp.h"
#include "opt_smp_invltlb.h"
#if defined(APIC_IO)
#if !defined(SMP_INVLTLB)
#error you must define BOTH APIC_IO and SMP_INVLTLB or NEITHER
#endif
#else				/* APIC_IO */
#if defined(SMP_INVLTLB)
#error you must define BOTH APIC_IO and SMP_INVLTLB or NEITHER
#endif
#endif				/* APIC_IO */
#define FIX_MP_TABLE_WORKS_NOT

#include "opt_serial.h"

#include <sys/param.h>		/* for KERNBASE */
#include <sys/types.h>
#include <sys/sysproto.h>
#include <sys/time.h>
#include <sys/systm.h>

#include <vm/vm.h>		/* for KERNBASE */
#include <vm/vm_param.h>	/* for KERNBASE */
#include <vm/pmap.h>		/* for KERNBASE */
#include <machine/pmap.h>	/* for KERNBASE */

#include <machine/smp.h>
#include <machine/apic.h>
#include <machine/mpapic.h>
#include <machine/cpufunc.h>
#include <machine/segments.h>
#include <machine/smptests.h>	/** TEST_UPPERPRIO, TEST_DEFAULT_CONFIG */

#include <i386/i386/cons.h>	/* cngetc() */

#if defined(SMP_INVLTLB)
#include <i386/isa/icu.h>
#endif				/* SMP_INVLTLB */

#define WARMBOOT_TARGET	0
#define WARMBOOT_OFF	(KERNBASE + 0x0467)
#define WARMBOOT_SEG	(KERNBASE + 0x0469)

#define BIOS_BASE	(0xf0000)
#define BIOS_SIZE	(0x10000)
#define BIOS_COUNT	(BIOS_SIZE/4)

#define CMOS_REG	(0x70)
#define CMOS_DATA	(0x71)
#define BIOS_RESET	(0x0f)
#define BIOS_WARM	(0x0a)

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


/** FIXME: what system files declare these??? */
extern struct region_descriptor r_gdt, r_idt;

/* global data */
struct proc *SMPcurproc[NCPU];
struct pcb *SMPcurpcb[NCPU];
struct timeval SMPruntime[NCPU];

int     mp_ncpus;		/* # of CPUs, including BSP */
int     mp_naps;		/* # of Applications processors */
int     mp_nbusses;		/* # of busses */
int     mp_napics;		/* # of IO APICs */
int     mpenabled;
int     boot_cpu_id;		/* designated BSP */
vm_offset_t cpu_apic_address;
vm_offset_t io_apic_address[NAPIC];

u_int32_t cpu_apic_versions[NCPU];
u_int32_t io_apic_versions[NAPIC];

/*
 * APIC ID logical/physical mapping structures
 */
int     cpu_num_to_apic_id[NCPU];
int     io_num_to_apic_id[NAPIC];
int     apic_id_to_logical[NAPICID];

/*
 * look for MP compliant motherboard.
 */

static u_int boot_address;
static u_int base_memory;

static int picmode;		/* 0: virtual wire mode, 1: PIC mode */
static u_int mpfps;
static int search_for_sig(u_int32_t target, int count);
static int mp_probe(u_int base_top);
static void mp_enable(u_int boot_addr);


/*
 * calculate usable address in base memory for AP trampoline code
 */
u_int
mp_bootaddress(u_int basemem)
{
	base_memory = basemem * 1024;	/* convert to bytes */

	boot_address = base_memory & ~0xfff;	/* round down to 4k boundary */
	if ((base_memory - boot_address) < bootMP_size)
		boot_address -= 4096;	/* not enough, lower by 4k */

	return boot_address;
}


/*
 * startup the SMP processors
 */
void
mp_start(void)
{
	/* look for MP capable motherboard */
	if (mp_probe(base_memory))
		mp_enable(boot_address);
	else {
		printf("MP FPS NOT FOUND, suggest use of 'mptable' program\n");
		panic("can't continue!\n");
	}

	/* finish pmap initialization - turn off V==P mapping at zero */
	pmap_bootstrap2();
}


/*
 * print various information about the SMP system hardware and setup
 */
void
mp_announce(void)
{
	int     x;

	printf("FreeBSD/SMP: Multiprocessor motherboard\n");
	printf(" cpu0 (BSP): apic id: %d", CPU_TO_ID(0));
	printf(", version: 0x%08x\n", cpu_apic_versions[0]);
	for (x = 1; x <= mp_naps; ++x) {
		printf(" cpu%d (AP):  apic id: %d", x, CPU_TO_ID(x));
		printf(", version: 0x%08x\n", cpu_apic_versions[x]);
	}

#if defined(APIC_IO)
	for (x = 0; x < mp_napics; ++x) {
		printf(" io%d (APIC): apic id: %d", x, IO_TO_ID(x));
		printf(", version: 0x%08x\n", io_apic_versions[x]);
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

	slot = NGDT + cpunumber();
	gsel_tss = GSEL(slot, SEL_KPL);
	gdt[slot].sd.sd_type = SDT_SYS386TSS;
	ltr(gsel_tss);

	load_cr0(0x8005003b);	/* XXX! */
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
	/* mask the LVT1 */
	temp = apic_base[APIC_LVT1];
	temp |= APIC_LVT_M;
	apic_base[APIC_LVT1] = temp;
}
#endif	/* APIC_IO */


/*******************************************************************
 * local functions and data
 */

static int
mp_probe(u_int base_top)
{
	int     x;
	u_long  segment;
	u_int32_t target;

	/* see if EBDA exists */
	if (segment = (u_long) * (u_short *) (KERNBASE + 0x40e)) {
		/* search first 1K of EBDA */
		target = (u_int32_t) (segment << 4);
		if ((x = search_for_sig(target, 1024 / 4)) >= 0)
			goto found;
	} else {
		/*last 1K of base memory, effective 'top of base' is passed in*/
		target = (u_int32_t) (base_top - 0x400);
		if ((x = search_for_sig(target, 1024 / 4)) >= 0)
			goto found;
	}

	/* search the BIOS */
	target = (u_int32_t) BIOS_BASE;
	if ((x = search_for_sig(target, BIOS_COUNT)) >= 0)
		goto found;

	/* nothing found */
	mpfps = mpenabled = 0;
	return 0;

found:				/* please forgive the 'goto'! */
	/* flag fact that we are running multiple processors */
	mpfps = x;
	mpenabled = 1;
	return 1;
}


/*
 * start the SMP system
 */
static int parse_mp_table(void);
static void default_mp_table(int type);
static int start_all_aps(u_int boot_addr);

#if defined(APIC_IO)
#include <i386/include/md_var.h>	/* setidt() */
#include <i386/isa/isa_device.h>	/* Xinvltlb() */
#endif	/* APIC_IO */

static void
mp_enable(u_int boot_addr)
{
	int     x;
#if defined(APIC_IO)
	int     apic;
	u_int   ux;
#if defined(TEST_UPPERPRIO)
	u_char  select;		/* the select register is 8 bits */
	u_int32_t flags;	/* the window register is 32 bits */
#endif	/* TEST_UPPERPRIO */
#endif	/* APIC_IO */

	/* examine the MP table for needed info */
	x = parse_mp_table();

	/* create pages for (address common) cpu APIC and each IO APIC */
	pmap_bootstrap_apics();

	/* can't process default configs till the CPU APIC is pmapped */
	if (x)
		default_mp_table(x);

#if defined(APIC_IO)
	/* fill the LOGICAL io_apic_versions table */
	for (apic = 0; apic < mp_napics; ++apic) {
		ux = io_apic_read(apic, IOAPIC_VER);
		io_apic_versions[apic] = ux;
	}

	/*
	 */
	for (apic = 0; apic < mp_napics; ++apic)
		if (io_apic_setup(apic) < 0)
			panic("IO APIC setup failure\n");

	/* install an inter-CPU IPI for TLB invalidation */
	setidt(ICU_OFFSET + XINVLTLB_OFFSET, Xinvltlb,
	    SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

#if defined(TEST_UPPERPRIO)

#if 1
	printf("special IRQ10\n");
	select = IOAPIC_REDTBL10;	/** HARD_VECTORXXX:  */
	flags = io_apic_read(0, select);
	flags &= ~0xff;		/** clear vector */
	flags |= 64;
	io_apic_write(0, select, flags);
#else
	printf("special IRQ10\n");
	cngetc();
	select = IOAPIC_REDTBL10;	/** HARD_VECTORXXX:  */
	flags = io_apic_read(0, select);
	flags &= ~IOART_DELMOD;	/* FIXED mode */
	io_apic_write(0, select, flags);
	io_apic_write(0, select + 1, boot_cpu_id << 24);
#endif	/** 0/1 */

#endif	/* TEST_UPPERPRIO */

#endif	/* APIC_IO */

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

#if defined(FIX_MP_TABLE_WORKS)
static void fix_mp_table __P((void));
#endif /* FIX_MP_TABLE_WORKS */

static void processor_entry __P((proc_entry_ptr entry, int *cpu));
static void io_apic_entry __P((io_apic_entry_ptr entry, int *apic));
static void bus_entry __P((bus_entry_ptr entry, int *bus));
static void int_entry __P((int_entry_ptr entry, int *intr));
static int lookup_bus_type __P((char *name));


/*
 * parse an Intel MP specification table
 */
static int
parse_mp_table(void)
{
	int     x;
	mpfps_t fps;
	mpcth_t cth;
	int     totalSize;
	void   *position;
	int     count;
	int     type;
	int     apic, bus, cpu, intr;

	/* clear physical APIC ID to logical CPU/IO table */
	for (x = 0; x < NAPICID; ++x)
		ID_TO_IO(x) = -1;

	/* clear logical CPU to APIC ID table */
	for (x = 0; x < NCPU; ++x)
		CPU_TO_ID(x) = -1;

	/* clear logical IO to APIC ID table */
	for (x = 0; x < NAPIC; ++x)
		IO_TO_ID(x) = -1;

	/* clear IO APIC address table */
	for (x = 0; x < NAPIC; ++x)
		io_apic_address[x] = ~0;

	/* clear bus data table */
	for (x = 0; x < NBUS; ++x)
		bus_data[x].bus_id = 0xff;

	/* clear IO APIC INT table */
	for (x = 0; x < NINTR; ++x)
		io_apic_ints[x].int_type = 0xff;
	nintrs = 0;

	/* count the BSP */
	mp_ncpus = 1;

	/* setup the cpu/apic mapping arrays */
	boot_cpu_id = -1;

	/* local pointer */
	fps = (mpfps_t) mpfps;

	/* record whether PIC or virtual-wire mode */
	picmode = (fps->mpfb2 & 0x80) ? 1 : 0;

	/* check for use of 'default' configuration */
#if defined(TEST_DEFAULT_CONFIG)
	/* use default addresses */
	cpu_apic_address = DEFAULT_APIC_BASE;
	io_apic_address[0] = DEFAULT_IO_APIC_BASE;

	/* return default configuration type */
	return TEST_DEFAULT_CONFIG;
#else
	if (fps->mpfb1 != 0) {
		/* use default addresses */
		cpu_apic_address = DEFAULT_APIC_BASE;
		io_apic_address[0] = DEFAULT_IO_APIC_BASE;

		/* return default configuration type */
		return fps->mpfb1;
	}
#endif	/* TEST_DEFAULT_CONFIG */

	if ((cth = fps->pap) == 0)
		panic("MP Configuration Table Header MISSING!\n");

	cpu_apic_address = (vm_offset_t) cth->apic_address;

	totalSize = cth->base_table_length - sizeof(struct MPCTH);
	position = (u_char *) cth + sizeof(struct MPCTH);
	count = cth->entry_count;

	apic = 0;		/* logical apic# start @ 0 */
	bus = 0;		/* logical bus# start @ 0 */
	cpu = 1;		/* logical cpu# start @ 0, BUT reserve 0 for */
				/* BSP */
	intr = 0;		/* unknown */

	/* walk the table, recording info of interest */
	while (count--) {
		switch (type = *(u_char *) position) {
		case 0:
			processor_entry(position, &cpu);
			break;
		case 1:
			bus_entry(position, &bus);
			break;
		case 2:
			io_apic_entry(position, &apic);
			break;
		case 3:
			int_entry(position, &intr);
			break;
		case 4:
			/* int_entry(position); */
			break;
		default:
			panic("mpfps Base Table HOSED!\n");
			/* NOTREACHED */
		}

		totalSize -= basetable_entry_types[type].length;
		(u_char *) position += basetable_entry_types[type].length;
	}

	if (boot_cpu_id == -1)
		panic("NO BSP found!\n");

	/* record # of APs found */
	mp_naps = (cpu - 1);

	/* record # of busses found */
	mp_nbusses = bus;

	/* record # of IO APICs found */
	mp_napics = apic;

	/* record # of IO APICs found */
	nintrs = intr;

#if defined(FIX_MP_TABLE_WORKS)
	/* post scan cleanup */
	fix_mp_table();
#endif /* FIX_MP_TABLE_WORKS */

	/* report fact that its NOT a default configuration */
	return 0;
}


/*
 * parse an Intel MP specification table
 */
#if defined(FIX_MP_TABLE_WORKS)
static void
fix_mp_table(void)
{
	int     x;
	int     y;
	int     num_pci_bus;
	bus_datum bus_record;

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

	/* count the number of PCI busses */
	for (num_pci_bus = 0, x = 0; x < mp_nbusses; ++x) {
		if (bus_data[x].bus_type == PCI)
			++num_pci_bus;
	}

	/* check the 1 PCI bus case for sanity */
	if (num_pci_bus == 1) {

		/* if its in the first slot all is well */
		if (bus_data[0].bus_type == PCI)
			return;

		/* mis-numbered, swap with whichever bus uses slot 0 */

		/* locate the entry holding the PCI bus */
		for (x = 1; x < mp_nbusses; ++x) {
			if (bus_data[x].bus_type == PCI)
				break;
		}

		/* swap the bus entry records */
		bus_record = bus_data[0];
		bus_data[0] = bus_data[x];
		bus_data[x] = bus_record;

		/* swap each relavant INTerrupt entry */
		for (y = 0; y < nintrs; ++y) {
			if (io_apic_ints[y].src_bus_id == x)
				io_apic_ints[y].src_bus_id = 0;
			else
				if (io_apic_ints[y].src_bus_id == 0)
					io_apic_ints[y].src_bus_id = x;
		}
	}
	/* sanity check if more than 1 PCI bus */
	else
		if (num_pci_bus > 1) {
			for (x = 0; x < num_pci_bus; ++x) {
				if (bus_data[x].bus_type != PCI) {
					printf("bad PCI bus numbering\n");
					panic("\n");
				}
			}
		}
}
#endif /* FIX_MP_TABLE_WORKS */


static void
processor_entry(proc_entry_ptr entry, int *cpu)
{
	int     x = *cpu;

	/* check for usability */
	if (!(entry->cpu_flags & PROCENTRY_FLAG_EN))
		return;

	/* check for BSP flag */
	if (entry->cpu_flags & PROCENTRY_FLAG_BP) {
		/* always give boot CPU the logical value of 0 */
		x = 0;
		boot_cpu_id = entry->apic_id;
	} else {
		/* add another AP to list, if less than max number of CPUs */
		if (x == NCPU) {
			printf("Warning: only using %d of the available CPUs!\n", x);
			return;
		}
		++(*cpu);
	}

	CPU_TO_ID(x) = entry->apic_id;
	ID_TO_CPU(entry->apic_id) = x;
}


static void
bus_entry(bus_entry_ptr entry, int *bus)
{
	int     x, y;
	char    name[8];
	char    c;

	if ((x = (*bus)++) == NBUS)
		panic("too many busses, increase 'NBUS'\n");

	/* encode the name into an index */
	for (y = 0; y < 6; ++y) {
		if ((c = entry->bus_type[y]) == ' ')
			break;
		name[y] = c;
	}
	name[y] = '\0';

	if ((y = lookup_bus_type(name)) == UNKNOWN_BUSTYPE)
		panic("unknown bus type: '%s'\n", name);

	bus_data[x].bus_id = entry->bus_id;
	bus_data[x].bus_type = y;
}


static void
io_apic_entry(io_apic_entry_ptr entry, int *apic)
{
	int     x;

	if (!(entry->apic_flags & IOAPICENTRY_FLAG_EN))
		return;

	if ((x = (*apic)++) == NAPIC)
		panic("too many APICs, increase 'NAPIC'\n");

	IO_TO_ID(x) = entry->apic_id;
	ID_TO_IO(entry->apic_id) = x;

	io_apic_address[x] = (vm_offset_t) entry->apic_address;
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


static void
int_entry(int_entry_ptr entry, int *intr)
{
	int     x;

	if ((x = (*intr)++) == NINTR)
		panic("too many INTs, increase 'NINTR'\n");

	io_apic_ints[x].int_type = entry->int_type;
	io_apic_ints[x].int_flags = entry->int_flags;
	io_apic_ints[x].src_bus_id = entry->src_bus_id;
	io_apic_ints[x].src_bus_irq = entry->src_bus_irq;
	io_apic_ints[x].dst_apic_id = entry->dst_apic_id;
	io_apic_ints[x].dst_apic_int = entry->dst_apic_int;
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
 * determine which APIC pin an ISA INT is attached to.
 */
#define INTTYPE(I)	(io_apic_ints[(I)].int_type)
#define INTPIN(I)	(io_apic_ints[(I)].dst_apic_int)

#define SRCBUSIRQ(I)	(io_apic_ints[(I)].src_bus_irq)
int
get_isa_apic_irq(int isaIRQ)
{
	int     intr;

#if defined(SMP_TIMER_NC)
	if (isaIRQ == 0)
		return -1;
#endif				/* SMP_TIMER_NC */

	for (intr = 0; intr < nintrs; ++intr)	/* search each INT record */
		if ((INTTYPE(intr) == 0)
		    && (SRCBUSIRQ(intr) == isaIRQ))	/* a candidate IRQ */
			if (apic_int_is_bus_type(intr, ISA))	/* check bus match */
				return INTPIN(intr);	/* exact match */

	return -1;		/* NOT found */
}
#undef SRCBUSIRQ


/*
 * determine which APIC pin an EISA INT is attached to.
 */
#define SRCBUSIRQ(I)	(io_apic_ints[(I)].src_bus_irq)
int
get_eisa_apic_irq(int eisaIRQ)
{
	int     intr;

#if defined(SMP_TIMER_NC)
	if (eisaIRQ == 0)
		return -1;
#endif				/* SMP_TIMER_NC */

	for (intr = 0; intr < nintrs; ++intr)	/* search each INT record */
		if ((INTTYPE(intr) == 0)
		    && (SRCBUSIRQ(intr) == eisaIRQ))	/* a candidate IRQ */
			if (apic_int_is_bus_type(intr, EISA))	/* check bus match */
				return INTPIN(intr);	/* exact match */

	return -1;		/* NOT found */
}
#undef SRCBUSIRQ


/*
 * determine which APIC pin a PCI INT is attached to.
 */
#define SRCBUSID(I)	(io_apic_ints[(I)].src_bus_id)
#define SRCBUSDEVICE(I)	((io_apic_ints[(I)].src_bus_irq >> 2) & 0x1f)
#define SRCBUSLINE(I)	(io_apic_ints[(I)].src_bus_irq & 0x03)
int
get_pci_apic_irq(int pciBus, int pciDevice, int pciInt)
{
	int     intr;

	--pciInt;		/* zero based */

	for (intr = 0; intr < nintrs; ++intr)	/* search each record */
		if ((INTTYPE(intr) == 0)
#if defined(FIX_MP_TABLE_WORKS)
		    && (SRCBUSID(intr) == pciBus)
#endif /* FIX_MP_TABLE_WORKS */
		    && (SRCBUSDEVICE(intr) == pciDevice)
		    && (SRCBUSLINE(intr) == pciInt))	/* a candidate IRQ */
			if (apic_int_is_bus_type(intr, PCI))	/* check bus match */
				return INTPIN(intr);	/* exact match */

	return -1;		/* NOT found */
}
#undef SRCBUSLINE
#undef SRCBUSDEVICE
#undef SRCBUSID

#undef INTPIN
#undef INTTYPE


int
undirect_pci_irq(int rirq)
{
#if defined(READY)
	printf("Freeing irq %d for ISA cards.\n", rirq);
	/** FIXME: tickle the MB redirector chip */
	return ???;
#else
	printf("Freeing (NOT implimented) irq %d for ISA cards.\n", rirq);
	return 0;
#endif				/* READY */
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

	boot_cpu_id = (apic_base[APIC_ID] & APIC_ID_MASK) >> 24;
	ap_cpu_id = (boot_cpu_id == 0) ? 1 : 0;

	/* BSP */
	CPU_TO_ID(0) = boot_cpu_id;
	ID_TO_CPU(boot_cpu_id) = 0;

	/* one and only AP */
	CPU_TO_ID(1) = ap_cpu_id;
	ID_TO_CPU(ap_cpu_id) = 1;
	mp_naps = 1;

	/* one and only IO APIC */
#if defined(APIC_IO)
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
			panic("Problem: can't control IO APIC ID, reg: 0x%08x\n", ux);
		io_apic_id = 2;
	}
	IO_TO_ID(0) = io_apic_id;
	ID_TO_IO(io_apic_id) = 0;
	mp_napics = 1;
#else
	mp_napics = 0;
#endif	/* APIC_IO */

	/* fill out bus entries */
	switch (type) {
	case 1:
	case 2:
	case 3:
	case 5:
	case 6:
		mp_nbusses = default_data[type - 1][0];
		bus_data[0].bus_id = default_data[type - 1][1];
		bus_data[0].bus_type = default_data[type - 1][2];
		bus_data[1].bus_id = default_data[type - 1][3];
		bus_data[1].bus_type = default_data[type - 1][4];
		break;

	/* case 4: case 7:		   MCA NOT supported */
	default:		/* illegal/reserved */
		panic("BAD default MP config: %d\n", type);
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
		panic("sorry, can't support type 2 default yet\n");
#endif	/* APIC_MIXED_MODE */
	} else
		io_apic_ints[2].src_bus_irq = 0;	/* ISA IRQ0 is on APIC INT 2 */

	if (type == 7)
		io_apic_ints[0].int_type = 0xff;	/* N/C */
	else
		io_apic_ints[0].int_type = 3;	/* vectored 8259 */

	nintrs = 16;
#endif	/* APIC_IO */
}


static void install_ap_tramp(u_int boot_addr);
static int start_ap(int logicalCpu, u_int boot_addr);

/*
 * start each AP in our list
 */
static int
start_all_aps(u_int boot_addr)
{
	int     x;
	u_char  mpbiosreason;
	u_long  mpbioswarmvec;

	/**
         * NOTE: this needs further thought:
         *        where does it get released?
         *        should it be set to empy?
         *
         * get the initial mp_lock with a count of 1 for the BSP
         */
	mp_lock = (apic_base[APIC_ID] & APIC_ID_MASK) + 1;

	/* initialize BSP's local APIC */
	apic_initialize(1);

	/* install the AP 1st level boot code */
	install_ap_tramp(boot_addr);

	/* save the current value of the warm-start vector */
	mpbioswarmvec = *((u_long *) WARMBOOT_OFF);
	outb(CMOS_REG, BIOS_RESET);
	mpbiosreason = inb(CMOS_DATA);

	/* start each AP */
	for (x = 1; x <= mp_naps; ++x) {

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
			/*
			 * better panic as the AP may be running loose
			 * somewhere
			 */
			printf("panic y/n? [n] ");
			if (cngetc() != 'n')
				panic("bye-bye\n");
		}
		CHECK_PRINT("trace");	/* show checkpoints */

		/* record its version info */
		cpu_apic_versions[x] = cpu_apic_versions[0];
	}

	/* fill in our (BSP) APIC version */
	cpu_apic_versions[0] = apic_base[APIC_VER];

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
	icr_hi = apic_base[APIC_ICR_HI] & ~APIC_ID_MASK;
	icr_hi |= (physical_cpu << 24);
	apic_base[APIC_ICR_HI] = icr_hi;

	/* do an INIT IPI: assert RESET */
	icr_lo = apic_base[APIC_ICR_LOW] & 0xfff00000;
	apic_base[APIC_ICR_LOW] = icr_lo | 0x0000c500;

	/* wait for pending status end */
	while (apic_base[APIC_ICR_LOW] & APIC_DELSTAT_MASK)
		 /* spin */ ;

	/* do an INIT IPI: deassert RESET */
	apic_base[APIC_ICR_LOW] = icr_lo | 0x00008500;

	/* wait for pending status end */
	u_sleep(10000);		/* wait ~10mS */
	while (apic_base[APIC_ICR_LOW] & APIC_DELSTAT_MASK)
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
	apic_base[APIC_ICR_LOW] = icr_lo | 0x00000600 | vector;
	while (apic_base[APIC_ICR_LOW] & APIC_DELSTAT_MASK)
		 /* spin */ ;
	u_sleep(200);		/* wait ~200uS */

	/*
	 * finally we do a 2nd STARTUP IPI: this 2nd STARTUP IPI should run IF
	 * the previous STARTUP IPI was cancelled by a latched INIT IPI. OR
	 * this STARTUP IPI will be ignored, as only ONE STARTUP IPI is
	 * recognized after hardware RESET or INIT IPI.
	 */

	apic_base[APIC_ICR_LOW] = icr_lo | 0x00000600 | vector;
	while (apic_base[APIC_ICR_LOW] & APIC_DELSTAT_MASK)
		 /* spin */ ;
	u_sleep(200);		/* wait ~200uS */

	/* wait for it to start */
	set_apic_timer(5000000);/* == 5 seconds */
	while (read_apic_timer())
		if (mp_ncpus > cpus)
			return 1;	/* return SUCCESS */

	return 0;		/* return FAILURE */
}


#ifdef SMP_INVLTLB
/*
 * Flush the TLB on all other CPU's
 *
 * XXX: Needs to handshake and wait for completion before proceding.
 */

void
smp_invltlb()
{
	if (smp_active) {
		if (invldebug & 2)
			all_but_self_ipi(ICU_OFFSET + 32);
	}
}

void
invlpg(u_int addr)
{
	__asm   __volatile("invlpg (%0)"::"r"(addr):"memory");
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
	smp_invltlb();
}
/*
 * Handles recieving an "IRQ 27", the invalidate tlb IPI..
 */
void
ipi_invltlb(void)
{
	u_long  temp;

	if (invldebug & 4) {
		/*
		 * This should be implemented as load_cr3(rcr3()) when
		 * load_cr3() is inlined.
		 */
		__asm   __volatile("movl %%cr3, %0; movl %0, %%cr3":"=r"(temp)
		    ::      "memory");

	}
}
#endif	/* SMP_INVLTLB */
