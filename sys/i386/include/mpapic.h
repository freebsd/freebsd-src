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
 *	$Id: mpapic.h,v 1.10 1997/07/28 03:59:34 fsmp Exp $
 */

#ifndef _MACHINE_MPAPIC_H_
#define _MACHINE_MPAPIC_H_

#include <machine/apic.h>

#include <i386/isa/icu.h>

/* number of busses */
#if !defined(NBUS)
# define NBUS		4
#endif /* NBUS */

/* total number of APIC INTs, including SHARED INTs */
#if !defined(NINTR)
#define NINTR		48
#endif /* NINTR */


/*
 * Size of APIC ID list.
 * Also used a MAX size of various other arrays.
 */
#define NAPICID		16

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
	return apic_ipi(APIC_DEST_ALLISELF, vector, APIC_DELMODE_FIXED);
}

/*
 * send an IPI INTerrupt containing 'vector' to all CPUs EXCEPT myself
 */
static __inline int
all_but_self_ipi(int vector)
{
	return apic_ipi(APIC_DEST_ALLESELF, vector, APIC_DELMODE_FIXED);
}

/*
 * send an IPI INTerrupt containing 'vector' to myself
 */
static __inline int
self_ipi(int vector)
{
	return apic_ipi(APIC_DEST_SELF, vector, APIC_DELMODE_FIXED);
}

#endif /* _MACHINE_MPAPIC_H */
