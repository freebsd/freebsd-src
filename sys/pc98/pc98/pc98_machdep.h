/*
 * Copyright (c) KATO Takenori, 1996.  All rights reserved.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * $FreeBSD$
 */

#ifndef __PC98_PC98_PC98_MACHDEP_H__
#define __PC98_PC98_PC98_MACHDEP_H__

void	pc98_init_dmac(void);
unsigned int	pc98_getmemsize(unsigned *, unsigned *);

struct	ccb_calc_geometry;
int	scsi_da_bios_params(struct ccb_calc_geometry *);

#define	PC98_VECTOR_SIZE			(0x400)
#define	PC98_SYSTEM_PARAMETER_SIZE		(0x230)

#define	PC98_SAVE_AREA(highreso_flag)	(0xa1000)
#define	PC98_SAVE_AREA_ADDRESS		(0x10)

#if defined(_KERNEL) && !defined(LOCORE)
/* BIOS parameter block */
extern unsigned char	pc98_system_parameter[]; /* in locore.c */
#define	OFS_BOOT_boothowto		0x210
#define	OFS_BOOT_bootdev		0x214
#define	OFS_BOOT_cyloffset		0x218
#define	OFS_WD_BIOS_SECSIZE(i)	(0x200+(i)*6)
#define	OFS_WD_BIOS_NCYL(i)		(0x202+(i)*6)
#define	OFS_WD_BIOS_HEAD(i)		(0x205+(i)*6)
#define	OFS_WD_BIOS_SEC(i)		(0x204+(i)*6)
#define	OFS_pc98_machine_type	0x220
#define	OFS_epson_machine_id	0x224
#define	OFS_epson_bios_id		0x225
#define	OFS_epson_system_type	0x226

#define	PC98_SYSTEM_PARAMETER(x) pc98_system_parameter[(x)-0x400]
#define	BOOT_boothowto (*(unsigned long*)(&pc98_system_parameter[OFS_BOOT_boothowto]))
#define	BOOT_bootdev   (*(unsigned long*)(&pc98_system_parameter[OFS_BOOT_bootdev]))
#define	BOOT_cyloffset (*(unsigned long*)(&pc98_system_parameter[OFS_BOOT_cyloffset]))
#define	WD_BIOS_SECSIZE(i) (*(unsigned short*)(&pc98_system_parameter[OFS_WD_BIOS_SECSIZE(i)]))
#define	WD_BIOS_NCYL(i) (*(unsigned short*)(&pc98_system_parameter[OFS_WD_BIOS_NCYL(i)]))
#define	WD_BIOS_HEAD(i) (pc98_system_parameter[OFS_WD_BIOS_HEAD(i)])
#define	WD_BIOS_SEC(i) (pc98_system_parameter[OFS_WD_BIOS_SEC(i)])
#define	pc98_machine_type (*(unsigned long*)&pc98_system_parameter[OFS_pc98_machine_type])
#define	epson_machine_id	(pc98_system_parameter[OFS_epson_machine_id])
#define	epson_bios_id		(pc98_system_parameter[OFS_epson_bios_id])
#define	epson_system_type	(pc98_system_parameter[OFS_epson_system_type])

# define PC98_TYPE_CHECK(x)	((pc98_machine_type & (x)) == (x))

#endif /* _KERNEL */

#endif /* __PC98_PC98_PC98_MACHDEP_H__ */
