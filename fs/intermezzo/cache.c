/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 2000 Stelias Computing, Inc.
 *  Copyright (C) 2000 Red Hat, Inc.
 *
 *   This file is part of InterMezzo, http://www.inter-mezzo.org.
 *
 *   InterMezzo is free software; you can redistribute it and/or
 *   modify it under the terms of version 2 of the GNU General Public
 *   License as published by the Free Software Foundation.
 *
 *   InterMezzo is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with InterMezzo; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define __NO_VERSION__
#include <linux/module.h>
#include <stdarg.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/system.h>

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/locks.h>
#include <linux/blkdev.h>
#include <linux/init.h>

#include <linux/intermezzo_fs.h>
#include <linux/intermezzo_psdev.h>

/*
   This file contains the routines associated with managing a
   cache of files for InterMezzo.  These caches have two reqs:
   - need to be found fast so they are hashed by the device, 
     with an attempt to have collision chains of length 1.
   The methods for the cache are set up in methods.
*/

extern kmem_cache_t * presto_dentry_slab;

/* the intent of this hash is to have collision chains of length 1 */
#define CACHES_BITS 8
#define CACHES_SIZE (1 << CACHES_BITS)
#define CACHES_MASK CACHES_SIZE - 1
static struct list_head presto_caches[CACHES_SIZE];

static inline int presto_cache_hash(kdev_t dev)
{
        return (CACHES_MASK) & ((0x000F & (dev)) + ((0x0F00 & (dev)) >>8));
}

inline void presto_cache_add(struct presto_cache *cache, kdev_t dev)
{
        list_add(&cache->cache_chain,
                 &presto_caches[presto_cache_hash(dev)]);
        cache->cache_dev = dev;
}

inline void presto_cache_init_hash(void)
{
        int i;
        for ( i = 0; i < CACHES_SIZE; i++ ) {
                INIT_LIST_HEAD(&presto_caches[i]);
        }
}

/* map a device to a cache */
struct presto_cache *presto_cache_find(kdev_t dev)
{
        struct presto_cache *cache;
        struct list_head *lh, *tmp;

        lh = tmp = &(presto_caches[presto_cache_hash(dev)]);
        while ( (tmp = lh->next) != lh ) {
                cache = list_entry(tmp, struct presto_cache, cache_chain);
                if ( cache->cache_dev == dev ) {
                        return cache;
                }
        }
        return NULL;
}


/* map an inode to a cache */
struct presto_cache *presto_get_cache(struct inode *inode)
{
        struct presto_cache *cache;
        ENTRY;
        /* find the correct presto_cache here, based on the device */
        cache = presto_cache_find(inode->i_dev);
        if ( !cache ) {
                CERROR("WARNING: no presto cache for dev %x, ino %ld\n",
                       inode->i_dev, inode->i_ino);
                EXIT;
                return NULL;
        }
        EXIT;
        return cache;
}

/* another debugging routine: check fs is InterMezzo fs */
int presto_ispresto(struct inode *inode)
{
        struct presto_cache *cache;

        if ( !inode )
                return 0;
        cache = presto_get_cache(inode);
        if ( !cache )
                return 0;
        return (inode->i_dev == cache->cache_dev);
}

/* setup a cache structure when we need one */
struct presto_cache *presto_cache_init(void)
{
        struct presto_cache *cache;

        PRESTO_ALLOC(cache, sizeof(struct presto_cache));
        if ( cache ) {
                memset(cache, 0, sizeof(struct presto_cache));
                INIT_LIST_HEAD(&cache->cache_chain);
                INIT_LIST_HEAD(&cache->cache_fset_list);
                cache->cache_lock = SPIN_LOCK_UNLOCKED;
                cache->cache_reserved = 0; 
        }
        return cache;
}

/* free a cache structure and all of the memory it is pointing to */
inline void presto_free_cache(struct presto_cache *cache)
{
        if (!cache)
                return;

        list_del(&cache->cache_chain);
        if (cache->cache_sb && cache->cache_sb->s_root &&
                        presto_d2d(cache->cache_sb->s_root)) {
                kmem_cache_free(presto_dentry_slab, 
                                presto_d2d(cache->cache_sb->s_root));
                cache->cache_sb->s_root->d_fsdata = NULL;
        }

        PRESTO_FREE(cache, sizeof(struct presto_cache));
}

int presto_reserve_space(struct presto_cache *cache, loff_t req)
{
        struct filter_fs *filter; 
        loff_t avail; 
        struct super_block *sb = cache->cache_sb;
        filter = cache->cache_filter;
        if (!filter ) {
                EXIT;
                return 0; 
        }
        if (!filter->o_trops ) {
                EXIT;
                return 0; 
        }
        if (!filter->o_trops->tr_avail ) {
                EXIT;
                return 0; 
        }

        spin_lock(&cache->cache_lock);
        avail = filter->o_trops->tr_avail(cache, sb); 
        CDEBUG(D_SUPER, "ESC::%ld +++> %ld \n", (long) cache->cache_reserved,
                 (long) (cache->cache_reserved + req)); 
        CDEBUG(D_SUPER, "ESC::Avail::%ld \n", (long) avail);
        if (req + cache->cache_reserved > avail) {
                spin_unlock(&cache->cache_lock);
                EXIT;
                return -ENOSPC;
        }
        cache->cache_reserved += req; 
        spin_unlock(&cache->cache_lock);

        EXIT;
        return 0;
}

void presto_release_space(struct presto_cache *cache, loff_t req)
{
        CDEBUG(D_SUPER, "ESC::%ld ---> %ld \n", (long) cache->cache_reserved,
                 (long) (cache->cache_reserved - req)); 
        spin_lock(&cache->cache_lock);
        cache->cache_reserved -= req; 
        spin_unlock(&cache->cache_lock);
}
