/*-
 * Copyright (c) 1997 Michael Smith
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
 *      $Id: bios.h,v 1.1 1997/08/01 06:04:59 msmith Exp $
 */

/* 
 * Signature structure for the BIOS32 Service Directory header 
 */
struct bios32_SDheader 
{
    u_int8_t	sig[4];
    u_int32_t	entry;
    u_int8_t	revision;
    u_int8_t	len;
    u_int8_t	cksum;
    u_int8_t	pad[5];
};

/* 
 * BIOS32 Service Directory entry.  Caller supplies name, bios32_SDlookup
 * fills in the rest of the details.
 */
struct bios32_SDentry 
{
    union 
    {
	u_int8_t	name[4];	/* service identifier */
	u_int32_t	id;		/* as a 32-bit value */
    } ident;
    u_int32_t	base;			/* base of service */
    u_int32_t	len;			/* service length */
    u_int32_t	entry;			/* entrypoint offset from base */
};

extern int		bios32_SDlookup(struct bios32_SDentry *ent);
extern u_int32_t	bios_sigsearch(u_int32_t start, u_char *sig, int siglen, 
					 int paralen, int sigofs);

/*
 * Call a 32-bit BIOS function
 */
struct bios32_args {
    u_long eax;
    u_long ebx;
    u_long ecx;
    u_long edx;
};
extern void		bios32(caddr_t func_addr, struct bios32_args *args);

#define BIOS_PADDRTOVADDR(x)	(((x) - ISA_HOLE_START) + atdevbase)
#define BIOS_VADDRTOPADDR(x)	(((x) - atdevbase) + ISA_HOLE_START)


/*
 * System Management BIOS / Desktop Management Interface tables
 */

struct DMI_table 
{
    u_int8_t	sig[5];			/* "_DMI_" */
    u_int8_t	cksum;			/* checksum */
    u_int16_t	st_size;		/* total length of SMBIOS table (bytes)*/
    u_int32_t	st_base;		/* base address of the SMBIOS table (physical) */
    u_int16_t	st_entries;		/* total number of structures present in the table */
    u_int8_t	bcd_revision;		/* interface revision number */
};

struct SMBIOS_table 
{
    u_int8_t	sig[4];			/* "_SM_" */
    u_int8_t	cksum;			/* checksum */
    u_int8_t	len;			/* structure length */
    u_int8_t	major, minor;		/* major/minor revision numbers */
    u_int16_t	st_maxsize;		/* largest structure size (bytes) */
    u_int8_t	revision;		/* entrypoint revision */
    u_int8_t	pad[5];
    struct DMI_table dmi;		/* follows immediately */
};


/* 
 * Exported lookup results 
 */
extern struct bios32_SDentry	PCIbios;
extern struct SMBIOS_table	*SMBIOS_table;
extern struct DMI_table		*DMI_table;


    
