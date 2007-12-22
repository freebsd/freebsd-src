/*-
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
#define	PC98_SYSTEM_PARAMETER_SIZE		(0x240)
#define	PC98_SAVE_AREA				(0xa1000)

#if defined(_KERNEL) && !defined(LOCORE)
/* BIOS parameter block */
extern unsigned char	pc98_system_parameter[]; /* in locore.c */

#define	OFS_pc98_machine_type		0x220
#define	OFS_epson_machine_id		0x224
#define	OFS_epson_bios_id		0x225
#define	OFS_epson_system_type		0x226

#define	PC98_SYSTEM_PARAMETER(x) pc98_system_parameter[(x)-0x400]
#define	pc98_machine_type (*(unsigned long*)&pc98_system_parameter[OFS_pc98_machine_type])
#define	epson_machine_id	(pc98_system_parameter[OFS_epson_machine_id])
#define	epson_bios_id		(pc98_system_parameter[OFS_epson_bios_id])
#define	epson_system_type	(pc98_system_parameter[OFS_epson_system_type])

# define PC98_TYPE_CHECK(x)	((pc98_machine_type & (x)) == (x))

/*
 * PC98 machine type
 */
#define	M_NEC_PC98	0x0001
#define	M_EPSON_PC98	0x0002
#define	M_NOT_H98	0x0010
#define	M_H98		0x0020
#define	M_NOTE		0x0040
#define	M_NORMAL	0x1000
#define	M_8M		0x8000

/*
 * EPSON machine list
 */
#define EPSON_PC386_NOTE_A	0x20
#define EPSON_PC386_NOTE_W	0x22
#define EPSON_PC386_NOTE_AE	0x27
#define EPSON_PC386_NOTE_WR	0x2a
#define EPSON_PC486_GR		0x2b
#define EPSON_PC486_P		0x30
#define EPSON_PC486_GR_SUPER	0x31
#define EPSON_PC486_GR_PLUS	0x32
#define EPSON_PC486_HX		0x34
#define EPSON_PC486_HG		0x35
#define EPSON_PC486_SE		0x37
#define EPSON_PC486_SR		0x38
#define EPSON_PC486_HA		0x3b

#endif /* _KERNEL */

#endif /* __PC98_PC98_PC98_MACHDEP_H__ */
