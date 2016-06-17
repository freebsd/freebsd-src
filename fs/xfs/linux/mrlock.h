/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */
#ifndef __XFS_SUPPORT_MRLOCK_H__
#define __XFS_SUPPORT_MRLOCK_H__

#include <linux/time.h>
#include <linux/wait.h>
#include <asm/atomic.h>
#include <asm/semaphore.h>

/*
 * Implement mrlocks on Linux that work for XFS.
 *
 * These are sleep locks and not spinlocks. If one wants read/write spinlocks,
 * use read_lock, write_lock, ... see spinlock.h.
 */

typedef struct mrlock_s {
	int			mr_count;
	unsigned short		mr_reads_waiting;
	unsigned short		mr_writes_waiting;
	wait_queue_head_t	mr_readerq;
	wait_queue_head_t	mr_writerq;
	spinlock_t		mr_lock;
} mrlock_t;

#define MR_ACCESS	1
#define MR_UPDATE	2

#define MRLOCK_BARRIER		0x1
#define MRLOCK_ALLOW_EQUAL_PRI	0x8

/*
 * mraccessf/mrupdatef take flags to be passed in while sleeping;
 * only PLTWAIT is currently supported.
 */

extern void	mraccessf(mrlock_t *, int);
extern void	mrupdatef(mrlock_t *, int);
extern void     mrlock(mrlock_t *, int, int);
extern void     mrunlock(mrlock_t *);
extern void     mraccunlock(mrlock_t *);
extern int      mrtryupdate(mrlock_t *);
extern int      mrtryaccess(mrlock_t *);
extern int	mrtrypromote(mrlock_t *);
extern void     mrdemote(mrlock_t *);

extern int	ismrlocked(mrlock_t *, int);
extern void     mrlock_init(mrlock_t *, int type, char *name, long sequence);
extern void     mrfree(mrlock_t *);

#define mrinit(mrp, name)	mrlock_init(mrp, MRLOCK_BARRIER, name, -1)
#define mraccess(mrp)		mraccessf(mrp, 0) /* grab for READ/ACCESS */
#define mrupdate(mrp)		mrupdatef(mrp, 0) /* grab for WRITE/UPDATE */
#define mrislocked_access(mrp)	((mrp)->mr_count > 0)
#define mrislocked_update(mrp)	((mrp)->mr_count < 0)

#endif /* __XFS_SUPPORT_MRLOCK_H__ */
