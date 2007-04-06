/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_ARC_H
#define	_SYS_ARC_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/zfs_context.h>

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/zio.h>

typedef struct arc_buf_hdr arc_buf_hdr_t;
typedef struct arc_buf arc_buf_t;
typedef void arc_done_func_t(zio_t *zio, arc_buf_t *buf, void *private);
typedef void arc_byteswap_func_t(void *buf, size_t size);
typedef int arc_evict_func_t(void *private);

/* generic arc_done_func_t's which you can use */
arc_done_func_t arc_bcopy_func;
arc_done_func_t arc_getbuf_func;

struct arc_buf {
	arc_buf_hdr_t		*b_hdr;
	arc_buf_t		*b_next;
	void			*b_data;
	arc_evict_func_t	*b_efunc;
	void			*b_private;
};

typedef enum arc_buf_contents {
	ARC_BUFC_UNDEF,				/* buffer contents undefined */
	ARC_BUFC_DATA,				/* buffer contains data */
	ARC_BUFC_METADATA			/* buffer contains metadata */
} arc_buf_contents_t;
/*
 * These are the flags we pass into calls to the arc
 */
#define	ARC_WAIT	(1 << 1)	/* perform I/O synchronously */
#define	ARC_NOWAIT	(1 << 2)	/* perform I/O asynchronously */
#define	ARC_PREFETCH	(1 << 3)	/* I/O is a prefetch */
#define	ARC_CACHED	(1 << 4)	/* I/O was already in cache */

arc_buf_t *arc_buf_alloc(spa_t *spa, int size, void *tag,
    arc_buf_contents_t type);
void arc_buf_add_ref(arc_buf_t *buf, void *tag);
int arc_buf_remove_ref(arc_buf_t *buf, void *tag);
int arc_buf_size(arc_buf_t *buf);
void arc_release(arc_buf_t *buf, void *tag);
int arc_released(arc_buf_t *buf);
int arc_has_callback(arc_buf_t *buf);
void arc_buf_freeze(arc_buf_t *buf);
void arc_buf_thaw(arc_buf_t *buf);
#ifdef ZFS_DEBUG
int arc_referenced(arc_buf_t *buf);
#endif

int arc_read(zio_t *pio, spa_t *spa, blkptr_t *bp, arc_byteswap_func_t *swap,
    arc_done_func_t *done, void *private, int priority, int flags,
    uint32_t *arc_flags, zbookmark_t *zb);
zio_t *arc_write(zio_t *pio, spa_t *spa, int checksum, int compress,
    int ncopies, uint64_t txg, blkptr_t *bp, arc_buf_t *buf,
    arc_done_func_t *ready, arc_done_func_t *done, void *private, int priority,
    int flags, zbookmark_t *zb);
int arc_free(zio_t *pio, spa_t *spa, uint64_t txg, blkptr_t *bp,
    zio_done_func_t *done, void *private, uint32_t arc_flags);
int arc_tryread(spa_t *spa, blkptr_t *bp, void *data);

void arc_set_callback(arc_buf_t *buf, arc_evict_func_t *func, void *private);
int arc_buf_evict(arc_buf_t *buf);

void arc_flush(void);
void arc_tempreserve_clear(uint64_t tempreserve);
int arc_tempreserve_space(uint64_t tempreserve);

void arc_init(void);
void arc_fini(void);

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_ARC_H */
