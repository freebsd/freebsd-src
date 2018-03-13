/*	$NetBSD: ibcs2_stat.h,v 1.2 1994/10/26 02:53:03 cgd Exp $	*/
/* $FreeBSD$ */

/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 1994 Scott Bartram
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Scott Bartram.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

#ifndef	_IBCS2_STAT_H
#define	_IBCS2_STAT_H

#include <i386/ibcs2/ibcs2_types.h>

struct ibcs2_stat {
	ibcs2_dev_t	st_dev;
	ibcs2_ino_t	st_ino;
	ibcs2_mode_t	st_mode;
	ibcs2_nlink_t	st_nlink;
	ibcs2_uid_t	st_uid;
	ibcs2_gid_t	st_gid;
	ibcs2_dev_t	st_rdev;
	ibcs2_off_t	st_size;
	ibcs2_time_t	st_atim;
	ibcs2_time_t	st_mtim;
	ibcs2_time_t	st_ctim;
};

#define ibcs2_stat_len	(sizeof(struct ibcs2_stat))

#define IBCS2_S_IFMT		0xf000
#define IBCS2_S_IFIFO		0x1000
#define IBCS2_S_IFCHR		0x2000
#define IBCS2_S_IFDIR		0x4000
#define IBCS2_S_IFBLK		0x6000
#define IBCS2_S_IFREG		0x8000
#define IBCS2_S_IFSOCK		0xc000

#define IBCS2_S_IFNAM		0x5000
#define IBCS2_S_IFLNK		0xa000

#define IBCS2_S_ISUID		0x0800
#define IBCS2_S_ISGID		0x0400
#define IBCS2_S_ISVTX		0x0200

#define IBCS2_S_IRWXU		0x01c0
#define IBCS2_S_IRUSR		0x0100
#define IBCS2_S_IWUSR		0x0080
#define IBCS2_S_IXUSR		0x0040
#define IBCS2_S_IRWXG		0x0038
#define IBCS2_S_IRGRP		0x0020
#define IBCS2_S_IWGRP		0x000f
#define IBCS2_S_IXGRP		0x0008
#define IBCS2_S_IRWXO		0x0007
#define IBCS2_S_IROTH		0x0004
#define IBCS2_S_IWOTH		0x0002
#define IBCS2_S_IXOTH		0x0001

#define IBCS2_S_ISFIFO(mode)	(((mode) & IBCS2_S_IFMT) == IBCS2_S_IFIFO)
#define IBCS2_S_ISCHR(mode)	(((mode) & IBCS2_S_IFMT) == IBCS2_S_IFCHR)
#define IBCS2_S_ISDIR(mode)	(((mode) & IBCS2_S_IFMT) == IBCS2_S_IFDIR)
#define IBCS2_S_ISBLK(mode)	(((mode) & IBCS2_S_IFMT) == IBCS2_S_IFBLK)
#define IBCS2_S_ISREG(mode)	(((mode) & IBCS2_S_IFMT) == IBCS2_S_IFREG)
#define IBCS2_S_ISSOCK(mode)	(((mode) & IBCS2_S_IFMT) == IBCS2_S_IFSOCK)

#endif /* _IBCS2_STAT_H */
