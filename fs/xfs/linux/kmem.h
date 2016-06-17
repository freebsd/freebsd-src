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
#ifndef __XFS_SUPPORT_KMEM_H__
#define __XFS_SUPPORT_KMEM_H__

#include <linux/slab.h>

/*
 * memory management routines
 */
#define KM_SLEEP	0x0001
#define KM_NOSLEEP	0x0002
#define KM_NOFS		0x0004

#define	kmem_zone	kmem_cache_s
#define kmem_zone_t	kmem_cache_t

typedef unsigned long xfs_pflags_t;

#define PFLAGS_TEST_NOIO()              (current->flags & PF_NOIO)
#define PFLAGS_TEST_FSTRANS()           (current->flags & PF_FSTRANS)

#define PFLAGS_SET_NOIO(STATEP) do {    \
	*(STATEP) = current->flags;     \
        current->flags |= PF_NOIO;      \
} while (0)

#define PFLAGS_SET_FSTRANS(STATEP) do { \
	*(STATEP) = current->flags;     \
        current->flags |= PF_FSTRANS;   \
} while (0)

#define PFLAGS_CLEAR_FSTRANS(STATEP) do { \
	*(STATEP) = current->flags;	\
	current->flags &= ~PF_FSTRANS;	\
} while (0)


#define PFLAGS_RESTORE(STATEP) do {     \
	current->flags = *(STATEP);     \
} while (0)

#define PFLAGS_DUP(OSTATEP, NSTATEP) do { \
	*(NSTATEP) = *(OSTATEP);	\
} while (0);

static __inline unsigned int kmem_flags_convert(int flags)
{
        int lflags;
        
#if DEBUG
	if (unlikely(flags & ~(KM_SLEEP|KM_NOSLEEP|KM_NOFS))) {
		printk(KERN_WARNING
		    "XFS: memory allocation with wrong flags (%x)\n", flags);
		BUG();
	}
#endif
        
        lflags = (flags & KM_NOSLEEP) ? GFP_ATOMIC : GFP_KERNEL;
        
        /* avoid recusive callbacks to filesystem during transactions */
	if (PFLAGS_TEST_FSTRANS() || (flags & KM_NOFS))
		lflags &= ~__GFP_FS;
        
        return lflags;
}

extern kmem_zone_t  *kmem_zone_init(int, char *);
extern void	    *kmem_zone_zalloc(kmem_zone_t *, int);
extern void	    *kmem_zone_alloc(kmem_zone_t *, int);
extern void         kmem_zone_free(kmem_zone_t *, void *);

extern void	    *kmem_alloc(size_t, int);
extern void	    *kmem_realloc(void *, size_t, size_t, int);
extern void	    *kmem_zalloc(size_t, int);
extern void         kmem_free(void *, size_t);

typedef void	    *kmem_shaker_t;
typedef int	    (*kmem_shake_func_t)(int, unsigned int);

extern kmem_shaker_t	kmem_shake_register(kmem_shake_func_t);
extern void	    kmem_shake_deregister(kmem_shaker_t);
static __inline int	kmem_shake_allow(unsigned int mask) { return 1; }

#endif /* __XFS_SUPPORT_KMEM_H__ */
