/*-
 * Copyright (c) 2001 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#ifndef _MADT_H_
#define	_MADT_H_

#pragma pack(1)

#define	APIC_INTERRUPT_SOURCE_OVERRIDE	2
#define	APIC_NMI			3
#define	APIC_LOCAL_APIC_NMI		4
#define	APIC_LOCAL_APIC_OVERRIDE	5
#define	APIC_IO_SAPIC			6
#define	APIC_LOCAL_SAPIC		7
#define	APIC_PLATFORM_INTERRUPT		8

#define	APIC_POLARITY_CONFORM		0
#define	APIC_POLARITY_ACTIVEHI		1
#define	APIC_POLARITY_ACTIVELO		3
#define	APIC_TRIGGER_CONFORM		0
#define	APIC_TRIGGER_EDGE		1
#define	APIC_TRIGGER_LEVEL		3

typedef struct	/* Interrupt Source Override */
{
	APIC_HEADER	Header;
	UINT8		Bus;
	UINT8		Source;
	UINT32		GlobalSystemInterrupt;
	UINT16		Polarity   : 2;		/* Polarity of input signal */
	UINT16		TriggerMode: 2;		/* Trigger mode of input signal */
	UINT16		Reserved1  : 12;
} INTERRUPT_SOURCE_OVERRIDE;

typedef struct	/* NMI */
{
	APIC_HEADER	Header;
	UINT16		Polarity   : 2;		/* Polarity of input signal */
	UINT16		TriggerMode: 2;		/* Trigger mode of input signal */
	UINT16		Reserved1  : 12;
	UINT32		GlobalSystemInterrupt;
} NMI;

typedef struct	/* LOCAL APIC NMI */
{
	APIC_HEADER	Header;
	UINT8		ProcessorApicId;	/* ACPI processor id */
	UINT16		Polarity   : 2;		/* Polarity of input signal */
	UINT16		TriggerMode: 2;		/* Trigger mode of input signal */
	UINT16		Reserved1  : 12;
	UINT8		LINTPin;
} LAPIC_NMI;

typedef struct	/* LOCAL APIC OVERRIDE */
{
	APIC_HEADER	Header;
	UINT16		Reserved;
	UINT64		LocalApicAddress;
} LAPIC_OVERRIDE;

typedef struct	/* IO SAPIC */
{
	APIC_HEADER	Header;
	UINT8		IoSapicId;		/* I/O SAPIC ID */
	UINT8		Reserved;		/* reserved - must be zero */
	UINT32		Vector;			/* interrupt base */
	UINT64		IoSapicAddress;		/* SAPIC's physical address */
} IO_SAPIC;

typedef struct  /* LOCAL SAPIC */
{
	APIC_HEADER	Header;
	UINT8		ProcessorId;		/* ACPI processor id */
	UINT8		LocalSapicId;		/* Processor local SAPIC id */
	UINT8		LocalSapicEid;		/* Processor local SAPIC eid */
	UINT8		Reserved[3];
	UINT32		ProcessorEnabled: 1;
	UINT32		FlagsReserved: 31;
} LOCAL_SAPIC;

typedef struct  /* PLATFORM INTERRUPT SOURCE */
{
	APIC_HEADER	Header;
	UINT16		Polarity   : 2;		/* Polarity of input signal */
	UINT16		TriggerMode: 2;		/* Trigger mode of input signal */
	UINT16		Reserved1  : 12;
	UINT8		InterruptType;		/* 1-PMI, 2-INIT, 3-Error */
	UINT8		ProcessorId;		/* Processor ID of destination */
	UINT8		ProcessorEid;		/* Processor EID of destination */
	UINT8		IoSapicVector;		/* Value for redirection table */
	UINT32		GlobalSystemInterrupt;	/* Global System Interrupt */
	UINT32		Reserved2;
} PLATFORM_INTERRUPT_SOURCE;

#pragma pack()

#endif /* !_MADT_H_ */
