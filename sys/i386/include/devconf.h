/*
 * Copyright (c) 1994, Garrett A. Wollman.  All rights reserved.
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
 *	$Id: devconf.h,v 1.12 1995/11/20 12:41:31 phk Exp $
 */
/*
 * devconf.h - machine-dependent device configuration table
 *
 * Garrett A. Wollman, October 1994.
 */

#ifndef _MACHINE_DEVCONF_H_
#define _MACHINE_DEVCONF_H_ 1

#define PARENTNAMELEN	32

#ifdef PC98
enum machdep_devtype { MDDT_CPU, MDDT_PC98, MDDT_PCI, MDDT_SCSI,
	       MDDT_DISK, MDDT_BUS, NDEVTYPES };
#else
enum machdep_devtype { MDDT_CPU, MDDT_ISA, MDDT_EISA, MDDT_PCI, MDDT_SCSI,
	       MDDT_DISK, MDDT_BUS, NDEVTYPES };
#endif

#ifdef PC98
#define DEVTYPENAMES { \
			       "cpu", \
			       "pc98", \
			       "pci", \
			       "scsi", \
			       "disk", \
			       "bus", \
			       0 \
	       }
#else
#define DEVTYPENAMES { \
			       "cpu", \
			       "isa", \
			       "eisa", \
			       "pci", \
			       "scsi", \
			       "disk", \
			       "bus", \
			       0 \
	       }
#endif

struct machdep_devconf {
	enum machdep_devtype mddc_devtype;
	int mddc_flags;
	char mddc_imask[4];
};

#define MDDC_SCSI { MDDT_SCSI, 0 }
#define MDDC_SCBUS { MDDT_BUS, 0 }

#define machdep_kdevconf machdep_devconf
#define MACHDEP_COPYDEV(dc, kdc) ((dc)->dc_md = (kdc)->kdc_md)

#define dc_devtype dc_md.mddc_devtype
#define dc_flags dc_md.mddc_flags
#ifdef PC98
#define kdc_pc98 kdc_parentdata
#else
#define kdc_isa kdc_parentdata
#define kdc_eisa kdc_parentdata
#endif
#define kdc_scsi kdc_parentdata

#define CPU_EXTERNALLEN (0)
#define DISK_EXTERNALLEN (sizeof(int))
#define BUS_EXTERNALLEN (0)

#ifdef KERNEL			/* XXX move this */
struct sysctl_req;
extern int disk_externalize(int, struct sysctl_req *);
#endif

#endif /* _MACHINE_DEVCONF_H_ */
