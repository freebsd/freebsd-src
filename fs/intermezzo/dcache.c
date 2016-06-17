/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Original version: Copyright (C) 1996 P. Braam and M. Callahan
 *  Rewritten for Linux 2.1. Copyright (C) 1997 Carnegie Mellon University
 *  d_fsdata and NFS compatiblity fixes Copyright (C) 2001 Tacit Networks, Inc.
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
 *
 * Directory operations for InterMezzo filesystem
 */

/* inode dentry alias list walking code adapted from linux/fs/dcache.c
 *
 * fs/dcache.c
 *
 * (C) 1997 Thomas Schoebel-Theuer,
 * with heavy changes by Linus Torvalds
 */

#define __NO_VERSION__
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <linux/slab.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/smp_lock.h>
#include <linux/vmalloc.h>

#include <linux/intermezzo_fs.h>

kmem_cache_t * presto_dentry_slab;

/* called when a cache lookup succeeds */
static int presto_d_revalidate(struct dentry *de, int flag)
{
        struct inode *inode = de->d_inode;
        struct presto_file_set * root_fset;

        ENTRY;
        if (!inode) {
                EXIT;
                return 0;
        }

        if (is_bad_inode(inode)) {
                EXIT;
                return 0;
        }

        if (!presto_d2d(de)) {
                presto_set_dd(de);
        }

        if (!presto_d2d(de)) {
                EXIT;
                return 0;
        }

        root_fset = presto_d2d(de->d_inode->i_sb->s_root)->dd_fset;
        if (root_fset->fset_flags & FSET_FLAT_BRANCH && 
            (presto_d2d(de)->dd_fset != root_fset )) {
                presto_d2d(de)->dd_fset = root_fset;
        }

        EXIT;
        return 1;

#if 0
        /* The following is needed for metadata on demand. */
        if ( S_ISDIR(inode->i_mode) ) {
                EXIT;
                return (presto_chk(de, PRESTO_DATA) &&
                        (presto_chk(de, PRESTO_ATTR)));
        } else {
                EXIT;
                return presto_chk(de, PRESTO_ATTR);
        }
#endif
}

static void presto_d_release(struct dentry *dentry)
{
        if (!presto_d2d(dentry)) {
                /* This can happen for dentries from NFSd */
                return;
        }
        presto_d2d(dentry)->dd_count--;

        if (!presto_d2d(dentry)->dd_count) {
                kmem_cache_free(presto_dentry_slab, presto_d2d(dentry));
                dentry->d_fsdata = NULL;
        }
}

struct dentry_operations presto_dentry_ops = 
{
        .d_revalidate =  presto_d_revalidate,
        .d_release = presto_d_release
};

static inline int presto_is_dentry_ROOT (struct dentry *dentry)
{
        return(dentry_name_cmp(dentry,"ROOT") &&
               !dentry_name_cmp(dentry->d_parent,".intermezzo"));
}

static struct presto_file_set* presto_try_find_fset(struct dentry* dentry,
                int *is_under_d_intermezzo)
{
        struct dentry* temp_dentry;
        struct presto_dentry_data *d_data;
        int found_root=0;

        ENTRY;
        CDEBUG(D_FSDATA, "finding fileset for %p:%s\n", dentry, 
                        dentry->d_name.name);

        *is_under_d_intermezzo = 0;

        /* walk up through the branch to get the fileset */
        /* The dentry we are passed presumably does not have the correct
         * fset information. However, we still want to start walking up
         * the branch from this dentry to get our found_root and 
         * is_under_d_intermezzo decisions correct
         */
        for (temp_dentry = dentry ; ; temp_dentry = temp_dentry->d_parent) {
                CDEBUG(D_FSDATA, "--->dentry %p:%*s\n", temp_dentry, 
                        temp_dentry->d_name.len,temp_dentry->d_name.name);
                if (presto_is_dentry_ROOT(temp_dentry))
                        found_root = 1;
                if (!found_root &&
                    dentry_name_cmp(temp_dentry, ".intermezzo")) {
                        *is_under_d_intermezzo = 1;
                }
                d_data = presto_d2d(temp_dentry);
                if (d_data) {
                        /* If we found a "ROOT" dentry while walking up the
                         * branch, we will journal regardless of whether
                         * we are under .intermezzo or not.
                         * If we are already under d_intermezzo don't reverse
                         * the decision here...even if we found a "ROOT"
                         * dentry above .intermezzo (if we were ever to
                         * modify the directory structure).
                         */
                        if (!*is_under_d_intermezzo)  
                                *is_under_d_intermezzo = !found_root &&
                                  (d_data->dd_flags & PRESTO_DONT_JOURNAL);
                        EXIT;
                        return d_data->dd_fset;
                }
                if (temp_dentry->d_parent == temp_dentry) {
                        break;
                }
        }
        EXIT;
        return NULL;
}

/* Only call this function on positive dentries */
static struct presto_dentry_data* presto_try_find_alias_with_dd (
                  struct dentry* dentry)
{
        struct inode *inode=dentry->d_inode;
        struct list_head *head, *next, *tmp;
        struct dentry *tmp_dentry;

        /* Search through the alias list for dentries with d_fsdata */
        spin_lock(&dcache_lock);
        head = &inode->i_dentry;
        next = inode->i_dentry.next;
        while (next != head) {
                tmp = next;
                next = tmp->next;
                tmp_dentry = list_entry(tmp, struct dentry, d_alias);
                if (!presto_d2d(tmp_dentry)) {
                        spin_unlock(&dcache_lock);
                        return presto_d2d(tmp_dentry);
                }
        }
        spin_unlock(&dcache_lock);
        return NULL;
}

/* Only call this function on positive dentries */
static void presto_set_alias_dd (struct dentry *dentry, 
                struct presto_dentry_data* dd)
{
        struct inode *inode=dentry->d_inode;
        struct list_head *head, *next, *tmp;
        struct dentry *tmp_dentry;

        /* Set d_fsdata for this dentry */
        dd->dd_count++;
        dentry->d_fsdata = dd;

        /* Now set d_fsdata for all dentries in the alias list. */
        spin_lock(&dcache_lock);
        head = &inode->i_dentry;
        next = inode->i_dentry.next;
        while (next != head) {
                tmp = next;
                next = tmp->next;
                tmp_dentry = list_entry(tmp, struct dentry, d_alias);
                if (!presto_d2d(tmp_dentry)) {
                        dd->dd_count++;
                        tmp_dentry->d_fsdata = dd;
                }
        }
        spin_unlock(&dcache_lock);
        return;
}

inline struct presto_dentry_data *izo_alloc_ddata(void)
{
        struct presto_dentry_data *dd;

        dd = kmem_cache_alloc(presto_dentry_slab, SLAB_KERNEL);
        if (dd == NULL) {
                CERROR("IZO: out of memory trying to allocate presto_dentry_data\n");
                return NULL;
        }
        memset(dd, 0, sizeof(*dd));
        dd->dd_count = 1;

        return dd;
}

/* This uses the BKL! */
int presto_set_dd(struct dentry * dentry)
{
        struct presto_file_set *fset;
        struct presto_dentry_data *dd;
        int is_under_d_izo;
        int error=0;

        ENTRY;

        if (!dentry)
                BUG();

        lock_kernel();

        /* Did we lose a race? */
        if (dentry->d_fsdata) {
                CERROR("dentry %p already has d_fsdata set\n", dentry);
                if (dentry->d_inode)
                        CERROR("    inode: %ld\n", dentry->d_inode->i_ino);
                EXIT;
                goto out_unlock;
        }

        if (dentry->d_inode != NULL) {
                /* NFSd runs find_fh_dentry which instantiates disconnected
                 * dentries which are then connected without a lookup(). 
                 * So it is possible to have connected dentries that do not 
                 * have d_fsdata set. So we walk the list trying to find 
                 * an alias which has its d_fsdata set and then use that 
                 * for all the other dentries  as well. 
                 * - SHP,Vinny. 
                 */

                /* If there is an alias with d_fsdata use it. */
                if ((dd = presto_try_find_alias_with_dd (dentry))) {
                        presto_set_alias_dd (dentry, dd);
                        EXIT;
                        goto out_unlock;
                }
        } else {
                /* Negative dentry */
                CDEBUG(D_FSDATA,"negative dentry %p: %*s\n", dentry, 
                                dentry->d_name.len, dentry->d_name.name);
        }

        /* No pre-existing d_fsdata, we need to construct one.
         * First, we must walk up the tree to find the fileset 
         * If a fileset can't be found, we leave a null fsdata
         * and return EROFS to indicate that we can't journal
         * updates. 
         */
        fset = presto_try_find_fset (dentry, &is_under_d_izo);
        if (!fset) { 
#ifdef PRESTO_NO_NFS
                CERROR("No fileset for dentry %p: %*s\n", dentry,
                                dentry->d_name.len, dentry->d_name.name);
#endif
                error = -EROFS;
                EXIT;
                goto out_unlock;
        }

        dentry->d_fsdata = izo_alloc_ddata();
        if (!presto_d2d(dentry)) {
                CERROR ("InterMezzo: out of memory allocating d_fsdata\n");
                error = -ENOMEM;
                goto out_unlock;
        }
        presto_d2d(dentry)->dd_fset = fset;
        if (is_under_d_izo)
                presto_d2d(dentry)->dd_flags |= PRESTO_DONT_JOURNAL;
        EXIT;

out_unlock:    
        CDEBUG(D_FSDATA,"presto_set_dd dentry %p: %*s, d_fsdata %p\n", 
                        dentry, dentry->d_name.len, dentry->d_name.name, 
                        dentry->d_fsdata);
        unlock_kernel();
        return error; 
}

int presto_init_ddata_cache(void)
{
        ENTRY;
        presto_dentry_slab =
                kmem_cache_create("presto_cache",
                                  sizeof(struct presto_dentry_data), 0,
                                  SLAB_HWCACHE_ALIGN, NULL,
                                  NULL);
        EXIT;
        return (presto_dentry_slab != NULL);
}

void presto_cleanup_ddata_cache(void)
{
        kmem_cache_destroy(presto_dentry_slab);
}
