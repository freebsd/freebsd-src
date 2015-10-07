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

#ifndef _COMPAT_CHERIABI_IOCTL_H_
#define	_COMPAT_CHERIABI_IOCTL_H_

#include <cam/scsi/scsi_sg.h>

struct ioc_read_toc_entry_c {
	u_char	address_format;
	u_char	starting_track;
	u_short	data_len;
	struct chericap data;		/* struct cd_toc_entry* */
};

struct md_ioctl_c {
	unsigned	md_version;	/* Structure layout version */
	unsigned	md_unit;	/* unit number */
	enum md_types	md_type;	/* type of disk */
	struct chericap	md_file;	/* pathname of file to mount */
	off_t		md_mediasize;	/* size of disk in bytes */
	unsigned	md_sectorsize;	/* sectorsize */
	unsigned	md_options;	/* options */
	u_int64_t	md_base;	/* base address */
	int		md_fwheads;	/* firmware heads */
	int		md_fwsectors;	/* firmware sectors */
	int		md_pad[MDNPAD];	/* padding for future ideas */
};

struct fiodgname_arg_c {
	int		len;
	struct chericap	buf;
};

struct mem_range_op_c {
	struct chericap	mo_desc;
	int		mo_arg[2];
};

struct pci_conf_io_c {
	u_int32_t		pat_buf_len;	/* pattern buffer length */
	u_int32_t		num_patterns;	/* number of patterns */
	struct chericap		patterns;	/* struct pci_match_conf ptr */
	u_int32_t		match_buf_len;	/* match buffer length */
	u_int32_t		num_matches;	/* number of matches returned */
	struct chericap		matches;	/* struct pci_conf ptr */
	u_int32_t		offset;		/* offset into device list */
	u_int32_t		generation;	/* device list generation */
	u_int32_t		status;		/* request status */
};

struct sg_io_hdr_c {
	int		interface_id;
	int		dxfer_direction;
	u_char		cmd_len;
	u_char		mx_sb_len;
	u_short		iovec_count;
	u_int		dxfer_len;
	struct chericap	dxferp;
	struct chericap	cmdp;
	struct chericap	sbp;
	u_int		timeout;
	u_int		flags;
	int		pack_id;
	struct chericap	usr_ptr;
	u_char		status;
	u_char		masked_status;
	u_char		msg_status;
	u_char		sb_len_wr;
	u_short		host_status;
	u_short		driver_status;
	int		resid;
	u_int		duration;
	u_int		info;
};


#define	CDIOREADTOCENTRYS_C _IOWR('c', 5, struct ioc_read_toc_entry_c)
#define	MDIOCATTACH_C	_IOC(IOC_INOUT, 'm', 0, sizeof(struct md_ioctl_c) + 4)
#define	MDIOCDETACH_C	_IOC(IOC_INOUT, 'm', 1, sizeof(struct md_ioctl_c) + 4)
#define	MDIOCQUERY_C	_IOC(IOC_INOUT, 'm', 2, sizeof(struct md_ioctl_c) + 4)
#define	MDIOCLIST_C	_IOC(IOC_INOUT, 'm', 3, sizeof(struct md_ioctl_c) + 4)
#define	FIODGNAME_C	_IOW('f', 120, struct fiodgname_arg_c)
#define	MEMRANGE_GET_C	_IOWR('m', 50, struct mem_range_op_c)
#define	MEMRANGE_SET_C	_IOW('m', 51, struct mem_range_op_c)
#define	PCIOCGETCONF_C	_IOWR('p', 5, struct pci_conf_io_c)
#define	SG_IO_C		_IOWR(SGIOC, 0x85, struct sg_io_hdr_c)

#endif	/* _COMPAT_CHERIABI_IOCTL_H_ */
