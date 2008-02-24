/*-
 * Copyright (c) 2003 Poul-Henning Kamp
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * $FreeBSD: src/sys/geom/geom_disk.h,v 1.7 2007/05/05 17:12:15 pjd Exp $
 */

#ifndef _GEOM_GEOM_DISK_H_
#define _GEOM_GEOM_DISK_H_

#ifdef _KERNEL 

#include <sys/queue.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>
#include <sys/disk.h>

struct disk;

typedef	int	disk_open_t(struct disk *);
typedef	int	disk_close_t(struct disk *);
typedef	void	disk_strategy_t(struct bio *bp);
typedef	int	disk_ioctl_t(struct disk *, u_long cmd, void *data,
			int fflag, struct thread *td);
		/* NB: disk_ioctl_t SHALL be cast'able to d_ioctl_t */

struct g_geom;
struct devstat;

struct disk {
	/* Fields which are private to geom_disk */
	struct g_geom		*d_geom;
	struct devstat		*d_devstat;
	int			d_destroyed;

	/* Shared fields */
	u_int			d_flags;
	const char		*d_name;
	u_int			d_unit;
	struct bio_queue_head	*d_queue;
	struct mtx		*d_lock;

	/* Disk methods  */
	disk_open_t		*d_open;
	disk_close_t		*d_close;
	disk_strategy_t		*d_strategy;
	disk_ioctl_t		*d_ioctl;
	dumper_t		*d_dump;

	/* Info fields from driver to geom_disk.c. Valid when open */
	u_int			d_sectorsize;
	off_t			d_mediasize;
	u_int			d_fwsectors;
	u_int			d_fwheads;
	u_int			d_maxsize;
	u_int			d_stripeoffset;
	u_int			d_stripesize;
	char			d_ident[DISK_IDENT_SIZE];

	/* Fields private to the driver */
	void			*d_drv1;
};

#define DISKFLAG_NEEDSGIANT	0x1
#define DISKFLAG_OPEN		0x2
#define DISKFLAG_CANDELETE	0x4
#define DISKFLAG_CANFLUSHCACHE	0x8

struct disk *disk_alloc(void);
void disk_create(struct disk *disk, int version);
void disk_destroy(struct disk *disk);
void disk_gone(struct disk *disk);

#define DISK_VERSION_00		0x58561059
#define DISK_VERSION_01		0x5856105a
#define DISK_VERSION		DISK_VERSION_01

#endif /* _KERNEL */
#endif /* _GEOM_GEOM_DISK_H_ */
