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
 *	$Id: mpapic.h,v 1.14 1997/04/26 08:11:50 peter Exp $
 */

#ifndef _MACHINE_MPAPIC_H_
#define _MACHINE_MPAPIC_H_

#include <i386/isa/icu.h>

/* number of busses */
#if !defined(NBUS)
# define NBUS		4
#endif /* NBUS */

/* total number of APIC INTs, including SHARED INTs */
#if !defined(NINTR)
#define NINTR		24
#endif /* NINTR */

/* size of APIC ID list */
#define NAPICID		16

/* number of IO APICs */
# if !defined(NAPIC)
# define NAPIC		1
# endif /* NAPIC */

/* use inline xxxIPI functions */
#define FAST_IPI_NOT
#define APICIPI_BANDAID


/* these don't really belong in here... */
enum busTypes {
    CBUS = 1,
    CBUSII = 2,
    EISA = 3,
    ISA = 6,
    PCI = 13,
    XPRESS = 18,
    MAX_BUSTYPE = 18,
    UNKNOWN_BUSTYPE = 0xff
};


/*
 * the physical/logical APIC ID management macors
 */
#define CPU_TO_ID(CPU)	(cpu_num_to_apic_id[CPU])
#define ID_TO_CPU(ID)	(apic_id_to_logical[ID])
#define IO_TO_ID(IO)	(io_num_to_apic_id[IO])
#define ID_TO_IO(ID)	(apic_id_to_logical[ID])


/*
 * inline functions to read/write the IO APIC
 * NOTES:
 *  unlike the local APIC, the IO APIC is accessed indirectly thru 2 registers.
 *  the select register is loaded with an index to the desired 'window' reg.
 *  the 'window' is accessed as a 32 bit unsigned.
 */

/*
 * read 'reg' from 'apic'
 */
#if defined(MULTIPLE_IOAPICS)
#error MULTIPLE_IOAPICSXXX
#else
static __inline u_int32_t
io_apic_read(int apic __attribute__ ((unused)), int reg)

{
	(*io_apic_base) = reg;
	return (*(io_apic_base + (IOAPIC_WINDOW / sizeof(u_int))));
}
#endif /* MULTIPLE_IOAPICS */


/*
 * write 'value' to 'reg' of 'apic'
 */
#if defined(MULTIPLE_IOAPICS)
#error MULTIPLE_IOAPICSXXX
#else
static __inline void
io_apic_write(int apic __attribute__ ((unused)), int reg, u_int32_t value)
{
	(*io_apic_base) = reg;
	(*(io_apic_base + (IOAPIC_WINDOW / sizeof(u_int)))) = value;
}
#endif /* MULTIPLE_IOAPICS */


#if defined(READY)
/*
 * set the IO APIC mask for INT# 'i'
 */
#if defined(MULTIPLE_IOAPICS)
#error MULTIPLE_IOAPICSXXX
#else
static __inline void
set_io_apic_mask(int apic, u_int32_t i)
{
	int		select;		/* the select register is 8 bits */
	u_int32_t	low_reg;	/* the window register is 32 bits */

	imen |= (1<<i);			/* set mask variable */

	select = IOAPIC_REDTBL + (i * 2); /* calculate addr */
	low_reg = io_apic_read(select);	/* read contents */

	low_reg |= IOART_INTMASK;	/* set mask */
	io_apic_write(select, low_reg);	/* new value */
}
#endif /* MULTIPLE_IOAPICS */
#endif /* READY */


#if defined(READY)
/*
 * clear the IO APIC mask for INT# 'i'
 */
#if defined(MULTIPLE_IOAPICS)
#error MULTIPLE_IOAPICSXXX
#else
static __inline void
clr_io_apic_mask(int apic, u_int32_t i)
{
	int		select;		/* the select register is 8 bits */
	u_int32_t	low_reg;	/* the window register is 32 bits */

	imen &= ~(1<<i);		/* clear mask variable */

	select = IOAPIC_REDTBL + (i * 2); /* calculate addr */
	low_reg = io_apic_read(select);	/* read contents */

	low_reg &= ~IOART_INTMASK;	/* clear mask */
	io_apic_write(select, low_reg);	/* new value */
}
#endif /* MULTIPLE_IOAPICS */
#endif /* READY */


/*
 * read current IRQ0 -IRQ23 masks
 */
#if defined(MULTIPLE_IOAPICS)
#error MULTIPLE_IOAPICSXXX
#else
static __inline u_int32_t
read_io_apic_mask24(int apic __attribute__ ((unused)))
{
	return (imen & 0x00ffffff);	/* return our global copy */
}
#endif /* MULTIPLE_IOAPICS */


/*
 * send an EndOfInterrupt to the local APIC
 */
static __inline void
apic_eoi(void)
{
	apic_base[APIC_EOI] = 0;
}


#if defined(FAST_IPI)

/*
 * send APIC IPI 'vector' to 'destType' via 'deliveryMode'
 *
 *  destType is 1 of: APIC_DEST_SELF, APIC_DEST_ALLISELF, APIC_DEST_ALLESELF
 *  vector is any valid SYSTEM INT vector
 *  deliveryMode is 1 of: APIC_DELMODE_FIXED, APIC_DELMODE_LOWPRIO
 */
static __inline int
apic_ipi(int destType, int vector, int deliveryMode)
{
	u_long	icr_lo;

	/* build IRC_LOW */
	icr_lo = (apic_base[APIC_ICR_LOW] & APIC_RESV2_MASK) |
	    destType | deliveryMode | vector;

	/* write APIC ICR */
	apic_base[APIC_ICR_LOW] = icr_lo;

	/* wait for pending status end */
	while (apic_base[APIC_ICR_LOW] & APIC_DELSTAT_MASK)
		/* spin */ ;

	/** FIXME: return result */
	return 0;
}


/*
 * send an IPI INTerrupt containing 'vector' to CPU 'target'
 *   NOTE: target is a LOGICAL APIC ID
 */
static __inline int
selected_proc_ipi(int target, int vector)
{
	u_long	icr_lo;
	u_long	icr_hi;

	/* write the destination field for the target AP */
	icr_hi = (apic_base[APIC_ICR_HI] & ~APIC_ID_MASK) |
	    (cpu_num_to_apic_id[target] << 24);
	apic_base[APIC_ICR_HI] = icr_hi;

	/* write command */
	icr_lo = (apic_base[APIC_ICR_LOW] & APIC_RESV2_MASK) |
	    APIC_DEST_DESTFLD | APIC_DELMODE_FIXED | vector;
	apic_base[APIC_ICR_LOW] = icr_lo;

	/* wait for pending status end */
	while (apic_base[APIC_ICR_LOW] & APIC_DELSTAT_MASK)
		/* spin */ ;

	return 0;	/** FIXME: return result */
}


/*
 * send an IPI INTerrupt containing 'vector' to CPUs in 'targetMap'
 * 'targetMap' is a bitfiled of length 14,
 *   APIC #0 == bit 0, ..., APIC #14 == bit 14
 *   NOTE: these are LOGICAL APIC IDs
 */
static __inline int
selected_procs_ipi(int targetMap, int vector)
{
	return selected_apic_ipi(targetMap, vector, APIC_DELMODE_FIXED);
}

/*
 * send an IPI INTerrupt containing 'vector' to all CPUs, including myself
 */
static __inline int
all_procs_ipi(int vector)
{
	u_int32_t	icr_lo;

	/* build command */
	icr_lo = (apic_base[APIC_ICR_LOW] & APIC_RESV2_MASK) |
	    APIC_DEST_ALLISELF | APIC_DELMODE_FIXED | vector;

	/* write command */
	apic_base[APIC_ICR_LOW] = icr_lo;

	/* wait for pending status end */
	while (apic_base[APIC_ICR_LOW] & APIC_DELSTAT_MASK)
		/* spin */ ;

	return 0;	/** FIXME: return result */
}


/*
 * send an IPI INTerrupt containing 'vector' to all CPUs EXCEPT myself
 */
static __inline int
all_but_self_ipi(int vector)
{
	u_int32_t	icr_lo;

	/* build command */
	icr_lo = (apic_base[APIC_ICR_LOW] & APIC_RESV2_MASK) |
	    APIC_DEST_ALLESELF | APIC_DELMODE_FIXED | vector;

	/* write command */
	apic_base[APIC_ICR_LOW] = icr_lo;

	/* wait for pending status end */
	while (apic_base[APIC_ICR_LOW] & APIC_DELSTAT_MASK)
		/* spin */ ;

	return 0;	/** FIXME: return result */
}

/*
 * send an IPI INTerrupt containing 'vector' to myself
 */
static __inline int
self_ipi(int vector)
{
	u_int32_t	icr_lo;

	/* build command */
	icr_lo = (apic_base[APIC_ICR_LOW] & APIC_RESV2_MASK) |
	    APIC_DEST_SELF | APIC_DELMODE_FIXED | vector;

	/* write command */
	apic_base[APIC_ICR_LOW] = icr_lo;

	/* wait for pending status end */
	while (apic_base[APIC_ICR_LOW] & APIC_DELSTAT_MASK)
		/* spin */ ;

	return 0;	/** FIXME: return result */
}

#  else /* !FAST_IPI */

int	apic_ipi		__P((int, int, int));
int	selected_procs_ipi	__P((int, int));
int	all_procs_ipi		__P((int));
int	all_but_self_ipi	__P((int));
int	self_ipi		__P((int));

#endif /* FAST_IPI */

#endif /* _MACHINE_MPAPIC_H */
