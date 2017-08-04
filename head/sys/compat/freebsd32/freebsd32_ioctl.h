/*-
 * Copyright (c) 2008 David E. O'Brien
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
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#ifndef _COMPAT_FREEBSD32_IOCTL_H_
#define	_COMPAT_FREEBSD32_IOCTL_H_

#include <cam/scsi/scsi_sg.h>

typedef __uint32_t caddr_t32;

struct ioc_read_toc_entry32 {
	u_char	address_format;
	u_char	starting_track;
	u_short	data_len;
	uint32_t data;		/* struct cd_toc_entry* */
};

#define	MDNPAD32	MDNPAD - 1
struct md_ioctl32 {
	unsigned	md_version;	/* Structure layout version */
	unsigned	md_unit;	/* unit number */
	enum md_types	md_type;	/* type of disk */
	caddr_t32	md_file;	/* pathname of file to mount */
	off_t		md_mediasize;	/* size of disk in bytes */
	unsigned	md_sectorsize;	/* sectorsize */
	unsigned	md_options;	/* options */
	u_int64_t	md_base;	/* base address */
	int		md_fwheads;	/* firmware heads */
	int		md_fwsectors;	/* firmware sectors */
	int		md_pad[MDNPAD32]; /* padding for future ideas */
};

struct fiodgname_arg32 {
	int		len;
	caddr_t32	buf;
};

struct mem_range_op32
{
	caddr_t32	mo_desc;
	int		mo_arg[2];
};

struct pci_conf32 {
	struct pcisel	pc_sel;		/* domain+bus+slot+function */
	u_int8_t	pc_hdr;		/* PCI header type */
	u_int16_t	pc_subvendor;	/* card vendor ID */
	u_int16_t	pc_subdevice;	/* card device ID, assigned by
					   card vendor */
	u_int16_t	pc_vendor;	/* chip vendor ID */
	u_int16_t	pc_device;	/* chip device ID, assigned by
					   chip vendor */
	u_int8_t	pc_class;	/* chip PCI class */
	u_int8_t	pc_subclass;	/* chip PCI subclass */
	u_int8_t	pc_progif;	/* chip PCI programming interface */
	u_int8_t	pc_revid;	/* chip revision ID */
	char		pd_name[PCI_MAXNAMELEN + 1];  /* device name */
	u_int32_t	pd_unit;	/* device unit number */
};

struct pci_match_conf32 {
	struct pcisel		pc_sel;		/* domain+bus+slot+function */
	char			pd_name[PCI_MAXNAMELEN + 1];  /* device name */
	u_int32_t		pd_unit;	/* Unit number */
	u_int16_t		pc_vendor;	/* PCI Vendor ID */
	u_int16_t		pc_device;	/* PCI Device ID */
	u_int8_t		pc_class;	/* PCI class */
	u_int32_t		flags;		/* Matching expression */
};

struct pci_conf_io32 {
	u_int32_t		pat_buf_len;	/* pattern buffer length */
	u_int32_t		num_patterns;	/* number of patterns */
	caddr_t32		patterns;	/* struct pci_match_conf ptr */
	u_int32_t		match_buf_len;	/* match buffer length */
	u_int32_t		num_matches;	/* number of matches returned */
	caddr_t32		matches;	/* struct pci_conf ptr */
	u_int32_t		offset;		/* offset into device list */
	u_int32_t		generation;	/* device list generation */
	u_int32_t		status;		/* request status */
};

#define	CDIOREADTOCENTRYS_32 _IOWR('c', 5, struct ioc_read_toc_entry32)
#define	MDIOCATTACH_32	_IOC(IOC_INOUT, 'm', 0, sizeof(struct md_ioctl32) + 4)
#define	MDIOCDETACH_32	_IOC(IOC_INOUT, 'm', 1, sizeof(struct md_ioctl32) + 4)
#define	MDIOCQUERY_32	_IOC(IOC_INOUT, 'm', 2, sizeof(struct md_ioctl32) + 4)
#define	MDIOCLIST_32	_IOC(IOC_INOUT, 'm', 3, sizeof(struct md_ioctl32) + 4)
#define	FIODGNAME_32	_IOW('f', 120, struct fiodgname_arg32)
#define	MEMRANGE_GET32	_IOWR('m', 50, struct mem_range_op32)
#define	MEMRANGE_SET32	_IOW('m', 51, struct mem_range_op32)
#define	PCIOCGETCONF_32	_IOWR('p', 5, struct pci_conf_io32)
#define	SG_IO_32	_IOWR(SGIOC, 0x85, struct sg_io_hdr32)

#endif	/* _COMPAT_FREEBSD32_IOCTL_H_ */
