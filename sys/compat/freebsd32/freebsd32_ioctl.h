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
 * $FreeBSD: src/sys/compat/freebsd32/freebsd32_ioctl.h,v 1.2.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _COMPAT_FREEBSD32_IOCTL_H_
#define	_COMPAT_FREEBSD32_IOCTL_H_

typedef __uint32_t caddr_t32;

struct ioc_toc_header32 {
	u_short	len;
	u_char	starting_track;
	u_char	ending_track;
};

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

#define	CDIOREADTOCENTRYS_32 _IOWR('c', 5, struct ioc_read_toc_entry32)
#define	CDIOREADTOCHEADER_32 _IOR('c', 4, struct ioc_toc_header32)
#define	MDIOCATTACH_32	_IOC(IOC_INOUT, 'm', 0, sizeof(struct md_ioctl32) + 4)
#define	MDIOCDETACH_32	_IOC(IOC_INOUT, 'm', 1, sizeof(struct md_ioctl32) + 4)
#define	MDIOCQUERY_32	_IOC(IOC_INOUT, 'm', 2, sizeof(struct md_ioctl32) + 4)
#define	MDIOCLIST_32	_IOC(IOC_INOUT, 'm', 3, sizeof(struct md_ioctl32) + 4)

#endif	/* _COMPAT_FREEBSD32_IOCTL_H_ */
