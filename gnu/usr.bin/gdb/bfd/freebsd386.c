/* BFD back-end for i386 a.out binaries under BSD.
   Copyright (C) 1990, 1991, 1992 Free Software Foundation, Inc.

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* This data should be correct for the format used under all the various
   BSD ports for 386 machines.  */

#define	BYTES_IN_WORD	4
#define	ARCH	32

/* ZMAGIC files never have the header in the text.  */
#define	N_HEADER_IN_TEXT(x)	0

/* ZMAGIC files start at address 0.  This does not apply to QMAGIC.  */
#define TEXT_START_ADDR 0

#define	PAGE_SIZE	4096
#define	SEGMENT_SIZE	PAGE_SIZE

#define	DEFAULT_ARCH	bfd_arch_i386
#define MACHTYPE_OK(mtype) ((mtype) == M_386 || (mtype) == M_386_NETBSD || (mtype) == M_UNKNOWN)

#define MY(OP) CAT(freebsd386_,OP)
#define TARGETNAME "a.out-freebsd-386"

#define N_MAGIC(ex) \
    ((ex).a_info & 0xffff)
#define N_MACHTYPE(ex) \
	( (N_GETMAGIC_NET(ex) == ZMAGIC) ? N_GETMID_NET(ex) : \
	((ex).a_info >> 16) & 0x03ff )
#define N_FLAGS(ex) \
	( (N_GETMAGIC_NET(ex) == ZMAGIC) ? N_GETFLAG_NET(ex) : \
	((ex).a_info >> 26) & 0x3f )
#define N_SET_INFO(ex,mag,mid,flag) \
	( (ex).a_info = (((flag) & 0x3f) <<26) | (((mid) & 0x03ff) << 16) | \
	((mag) & 0xffff) )

#define N_GETMAGIC_NET(ex) \
	(ntohl((ex).a_info) & 0xffff)
#define N_GETMID_NET(ex) \
	((ntohl((ex).a_info) >> 16) & 0x03ff)
#define N_GETFLAG_NET(ex) \
	((ntohl((ex).a_info) >> 26) & 0x3f)
#define N_SETMAGIC_NET(ex,mag,mid,flag) \
	( (ex).a_info = htonl( (((flag)&0x3f)<<26) | (((mid)&0x03ff)<<16) | \
	(((mag)&0xffff)) ) )

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "libaout.h"

#define N_ALIGN(ex,x) \
	(N_MAGIC(ex) == ZMAGIC || N_MAGIC(ex) == QMAGIC || \
	 N_GETMAGIC_NET(ex) == ZMAGIC || N_GETMAGIC_NET(ex) == QMAGIC ? \
	 ((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1) : (x))

/* Valid magic number check. */
#define	N_BADMAG(ex) \
	(N_MAGIC(ex) != OMAGIC && N_MAGIC(ex) != NMAGIC && \
	 N_MAGIC(ex) != ZMAGIC && N_MAGIC(ex) != QMAGIC && \
	 N_GETMAGIC_NET(ex) != OMAGIC && N_GETMAGIC_NET(ex) != NMAGIC && \
	 N_GETMAGIC_NET(ex) != ZMAGIC && N_GETMAGIC_NET(ex) != QMAGIC)

/* Address of the bottom of the text segment. */
#define N_TXTADDR(ex) \
	((N_MAGIC(ex) == OMAGIC || N_MAGIC(ex) == NMAGIC || \
	N_MAGIC(ex) == ZMAGIC) ? 0 : PAGE_SIZE)

/* Address of the bottom of the data segment. */
#define N_DATADDR(ex) \
	N_ALIGN(ex, N_TXTADDR(ex) + (ex).a_text)

/* Text segment offset. */
#define	N_TXTOFF(ex) \
	(N_MAGIC(ex) == ZMAGIC ? PAGE_SIZE : (N_MAGIC(ex) == QMAGIC || \
	N_GETMAGIC_NET(ex) == ZMAGIC) ? 0 : EXEC_BYTES_SIZE) 

/* Data segment offset. */
#define	N_DATOFF(ex) \
	N_ALIGN(ex, N_TXTOFF(ex) + (ex).a_text)

/* Relocation table offset. */
#define N_RELOFF(ex) \
	N_ALIGN(ex, N_DATOFF(ex) + (ex).a_data)

/* Symbol table offset. */
#define N_SYMOFF(ex) \
	(N_RELOFF(ex) + (ex).a_trsize + (ex).a_drsize)

/* String table offset. */
#define	N_STROFF(ex) 	(N_SYMOFF(ex) + (ex).a_syms)

#define NO_SWAP_MAGIC	/* magic number already in correct endian format */

#include "aout-target.h"
