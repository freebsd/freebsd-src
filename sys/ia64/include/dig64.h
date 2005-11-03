/*-
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/ia64/include/dig64.h,v 1.2 2005/01/06 22:18:23 imp Exp $
 */

#ifndef _MACHINE_DIG64_H_
#define	_MACHINE_DIG64_H_

struct dig64_gas {
	uint8_t		addr_space;
	uint8_t		bit_width;
	uint8_t		bit_offset;
	uint8_t		_reserved_;
	/*
	 * XXX using a 64-bit type for the address would cause padding and
	 * using __packed would cause unaligned accesses...
	 */
	uint32_t	addr_low;
	uint32_t	addr_high;
};

struct dig64_hcdp_entry {
	uint8_t		type;
#define	DIG64_HCDP_CONSOLE	0
#define	DIG64_HCDP_DBGPORT	1
	uint8_t		databits;
	uint8_t		parity;
	uint8_t		stopbits;
	uint8_t		pci_segment;
	uint8_t		pci_bus;
	uint8_t		pci_device:5;
	uint8_t		_reserved1_:3;
	uint8_t		pci_function:3;
	uint8_t		_reserved2_:3;
	uint8_t		interrupt:1;
	uint8_t		pci_flag:1;
	/*
	 * XXX using a 64-bit type for the baudrate would cause padding and
	 * using __packed would cause unaligned accesses...
	 */
	uint32_t	baud_low;
	uint32_t	baud_high;
	struct dig64_gas address;
	uint16_t	pci_devid;
	uint16_t	pci_vendor;
	uint32_t	irq;
	uint32_t	pclock;
	uint8_t		pci_interface;
	uint8_t		_reserved3_[7];
};

struct dig64_hcdp_table {
	char		signature[4];
#define	HCDP_SIGNATURE	"HCDP"
	uint32_t	length;
	uint8_t		revision;
	uint8_t		checksum;
	char		oem_id[6];
	char		oem_tbl_id[8];
	uint32_t	oem_rev;
	char		creator_id[4];
	uint32_t	creator_rev;
	uint32_t	entries;
	struct dig64_hcdp_entry entry[1];
};

#endif
