/*-
 * Copyright (C) 1994 by Rodney W. Grimes, Milwaukie, Oregon  97222
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Rodney W. Grimes.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY RODNEY W. GRIMES ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL RODNEY W. GRIMES BE LIABLE
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

#ifndef	_MACHINE_BOOTINFO_H_
#define	_MACHINE_BOOTINFO_H_ 

/* Only change the version number if you break compatibility. */
#define	BOOTINFO_VERSION	1

#define	N_BIOS_GEOM		8

/*
 * A zero bootinfo field often means that there is no info available.
 * Flags are used to indicate the validity of fields where zero is a
 * normal value.
 */
struct bootinfo {
	unsigned long		bi_version;
	unsigned char		*bi_kernelname;
	void				*bi_nfs_diskless;
				/* End of fields that are always present. */
#define	bi_endcommon		bi_n_bios_used
	unsigned long		bi_n_bios_used;
	unsigned long		bi_bios_geom[N_BIOS_GEOM];
	unsigned long		bi_size;
	unsigned char		bi_memsizes_valid;
	unsigned char		bi_pad[3];
	unsigned long		bi_basemem;
	unsigned long		bi_extmem;
	unsigned long		bi_symtab;
	unsigned long		bi_esymtab;
};

#ifdef KERNEL
extern struct bootinfo	bootinfo;
#endif

#endif	/* _MACHINE_BOOTINFO_H_ */
