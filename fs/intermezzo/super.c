/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 1998 Peter J. Braam <braam@clusterfs.com>
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
 *
 *  presto's super.c
 */

static char rcsid[] __attribute ((unused)) = "$Id: super.c,v 1.41 2002/10/03 03:50:49 rread Exp $";
#define INTERMEZZO_VERSION "$Revision: 1.41 $"

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
#include <linux/devfs_fs_kernel.h>
#define __NO_VERSION__
#include <linux/module.h>

#include <linux/intermezzo_fs.h>
#include <linux/intermezzo_psdev.h>

#ifdef PRESTO_DEBUG
long presto_vmemory = 0;
long presto_kmemory = 0;
#endif

/* returns an allocated string, copied out from data if opt is found */
static char *opt_read(const char *opt, char *data)
{
        char *value;
        char *retval;

        CDEBUG(D_SUPER, "option: %s, data %s\n", opt, data);
        if ( strncmp(opt, data, strlen(opt)) )
                return NULL;

        if ( (value = strchr(data, '=')) == NULL )
                return NULL;

        value++;
        PRESTO_ALLOC(retval, strlen(value) + 1);
        if ( !retval ) {
                CERROR("InterMezzo: Out of memory!\n");
                return NULL;
        }

        strcpy(retval, value);
        CDEBUG(D_SUPER, "Assigned option: %s, value %s\n", opt, retval);
        return retval;
}

static void opt_store(char **dst, char *opt)
{
        if (!dst) 
                CERROR("intermezzo: store_opt, error dst == NULL\n"); 

        if (*dst)
                PRESTO_FREE(*dst, strlen(*dst) + 1);
        *dst = opt;
}

static void opt_set_default(char **dst, char *defval)
{
        if (!dst) 
                CERROR("intermezzo: store_opt, error dst == NULL\n"); 

        if (*dst)
                PRESTO_FREE(*dst, strlen(*dst) + 1);
        if (defval) {
                char *def_alloced; 
                PRESTO_ALLOC(def_alloced, strlen(defval)+1);
                if (!def_alloced) {
                        CERROR("InterMezzo: Out of memory!\n");
                        return ;
                }
                strcpy(def_alloced, defval);
                *dst = def_alloced; 
        }
}


/* Find the options for InterMezzo in "options", saving them into the
 * passed pointers.  If the pointer is null, the option is discarded.
 * Copy out all non-InterMezzo options into cache_data (to be passed
 * to the read_super operation of the cache).  The return value will
 * be a pointer to the end of the cache_data.
 */
static char *presto_options(struct super_block *sb, 
                            char *options, char *cache_data,
                            char **cache_type, char **fileset,
                            char **channel)
{
        char *this_char;
        char *cache_data_end = cache_data;

        /* set the defaults */ 
        if (strcmp(sb->s_type->name, "intermezzo") == 0)
            opt_set_default(cache_type, "ext3"); 
        else 
            opt_set_default(cache_type, "tmpfs"); 
            
        if (!options || !cache_data)
                return cache_data_end;


        CDEBUG(D_SUPER, "parsing options\n");
        for (this_char = strtok (options, ",");
             this_char != NULL;
             this_char = strtok (NULL, ",")) {
                char *opt;
                CDEBUG(D_SUPER, "this_char %s\n", this_char);

                if ( (opt = opt_read("fileset", this_char)) ) {
                        opt_store(fileset, opt);
                        continue;
                }
                if ( (opt = opt_read("cache_type", this_char)) ) {
                        opt_store(cache_type, opt);
                        continue;
                }
                if ( (opt = opt_read("channel", this_char)) ) {
                        opt_store(channel, opt);
                        continue;
                }

                cache_data_end += 
                        sprintf(cache_data_end, "%s%s",
                                cache_data_end != cache_data ? ",":"", 
                                this_char);
        }

        return cache_data_end;
}

static int presto_set_channel(struct presto_cache *cache, char *channel)
{
        int minor; 

        ENTRY;
        if (!channel) {
                minor = izo_psdev_get_free_channel();
        } else {
                minor = simple_strtoul(channel, NULL, 0); 
        }
        if (minor < 0 || minor >= MAX_CHANNEL) { 
                CERROR("all channels in use or channel too large %d\n", 
                       minor);
                return -EINVAL;
        }
        
        cache->cache_psdev = &(izo_channels[minor]);
        list_add(&cache->cache_channel_list, 
                 &cache->cache_psdev->uc_cache_list); 

        EXIT;
        return minor;
}

/* We always need to remove the presto options before passing 
   mount options to cache FS */
struct super_block * presto_read_super(struct super_block * sb,
                                       void * data, int silent)
{
        struct file_system_type *fstype;
        struct presto_cache *cache = NULL;
        char *cache_data = NULL;
        char *cache_data_end;
        char *cache_type = NULL;
        char *fileset = NULL;
        char *channel = NULL;
        int err; 
        unsigned int minor;

        ENTRY;

        /* reserve space for the cache's data */
        PRESTO_ALLOC(cache_data, PAGE_SIZE);
        if ( !cache_data ) {
                CERROR("presto_read_super: Cannot allocate data page.\n");
                EXIT;
                goto out_err;
        }

        /* read and validate options */
        cache_data_end = presto_options(sb, data, cache_data, &cache_type, 
                                        &fileset, &channel);

        /* was there anything for the cache filesystem in the data? */
        if (cache_data_end == cache_data) {
                PRESTO_FREE(cache_data, PAGE_SIZE);
                cache_data = NULL;
        } else {
                CDEBUG(D_SUPER, "cache_data at %p is: %s\n", cache_data,
                       cache_data);
        }

        /* set up the cache */
        cache = presto_cache_init();
        if ( !cache ) {
                CERROR("presto_read_super: failure allocating cache.\n");
                EXIT;
                goto out_err;
        }
        cache->cache_type = cache_type;

        /* link cache to channel */ 
        minor = presto_set_channel(cache, channel);
        if (minor < 0) { 
                EXIT;
                goto out_err;
        }

        CDEBUG(D_SUPER, "Presto: type=%s, fset=%s, dev= %d, flags %x\n",
               cache_type, fileset?fileset:"NULL", minor, cache->cache_flags);

        MOD_INC_USE_COUNT;

        /* get the filter for the cache */
        fstype = get_fs_type(cache_type);
        cache->cache_filter = filter_get_filter_fs((const char *)cache_type); 
        if ( !fstype || !cache->cache_filter) {
                CERROR("Presto: unrecognized fs type or cache type\n");
                MOD_DEC_USE_COUNT;
                EXIT;
                goto out_err;
        }

        /* can we in fact mount the cache */ 
        if ((fstype->fs_flags & FS_REQUIRES_DEV) && !sb->s_bdev) {
                CERROR("filesystem \"%s\" requires a valid block device\n",
                                cache_type);
                MOD_DEC_USE_COUNT;
                EXIT;
                goto out_err;
        }

        sb = fstype->read_super(sb, cache_data, silent);

        /* this might have been freed above */
        if (cache_data) {
                PRESTO_FREE(cache_data, PAGE_SIZE);
                cache_data = NULL;
        }

        if ( !sb ) {
                CERROR("InterMezzo: cache mount failure.\n");
                MOD_DEC_USE_COUNT;
                EXIT;
                goto out_err;
        }

        cache->cache_sb = sb;
        cache->cache_root = dget(sb->s_root);

        /* we now know the dev of the cache: hash the cache */
        presto_cache_add(cache, sb->s_dev);
        err = izo_prepare_fileset(sb->s_root, fileset); 

        filter_setup_journal_ops(cache->cache_filter, cache->cache_type); 

        /* make sure we have our own super operations: sb
           still contains the cache operations */
        filter_setup_super_ops(cache->cache_filter, sb->s_op, 
                               &presto_super_ops);
        sb->s_op = filter_c2usops(cache->cache_filter);

        /* get izo directory operations: sb->s_root->d_inode exists now */
        filter_setup_dir_ops(cache->cache_filter, sb->s_root->d_inode,
                             &presto_dir_iops, &presto_dir_fops);
        filter_setup_dentry_ops(cache->cache_filter, sb->s_root->d_op, 
                                &presto_dentry_ops);
        sb->s_root->d_inode->i_op = filter_c2udiops(cache->cache_filter);
        sb->s_root->d_inode->i_fop = filter_c2udfops(cache->cache_filter);
        sb->s_root->d_op = filter_c2udops(cache->cache_filter);

        EXIT;
        return sb;

 out_err:
        CDEBUG(D_SUPER, "out_err called\n");
        if (cache)
                PRESTO_FREE(cache, sizeof(struct presto_cache));
        if (cache_data)
                PRESTO_FREE(cache_data, PAGE_SIZE);
        if (fileset)
                PRESTO_FREE(fileset, strlen(fileset) + 1);
        if (channel)
                PRESTO_FREE(channel, strlen(channel) + 1);
        if (cache_type)
                PRESTO_FREE(cache_type, strlen(cache_type) + 1);

        CDEBUG(D_MALLOC, "mount error exit: kmem %ld, vmem %ld\n",
               presto_kmemory, presto_vmemory);
        return NULL;
}



#ifdef PRESTO_DEVEL
static DECLARE_FSTYPE(presto_fs_type, "izo", presto_read_super, FS_REQUIRES_DEV);
static DECLARE_FSTYPE(vpresto_fs_type, "vintermezzo", presto_read_super, FS_LITTER);
#else 
static DECLARE_FSTYPE(vpresto_fs_type, "vintermezzo", presto_read_super, FS_LITTER);
static DECLARE_FSTYPE(presto_fs_type, "intermezzo", presto_read_super, FS_REQUIRES_DEV);
#endif



int __init init_intermezzo_fs(void)
{
        int status;

        printk(KERN_INFO "InterMezzo Kernel/Intersync communications " INTERMEZZO_VERSION
               " info@clusterfs.com\n");

        status = presto_psdev_init();
        if ( status ) {
                CERROR("Problem (%d) in init_intermezzo_psdev\n", status);
                return status;
        }

        status = init_intermezzo_sysctl();
        if (status) {
                CERROR("presto: failed in init_intermezzo_sysctl!\n");
        }

        presto_cache_init_hash();

        if (!presto_init_ddata_cache()) {
                CERROR("presto out of memory!\n");
                return -ENOMEM;
        }

        status = register_filesystem(&presto_fs_type);
        if (status) {
                CERROR("presto: failed in register_filesystem!\n");
        }
        status = register_filesystem(&vpresto_fs_type);
        if (status) {
                CERROR("vpresto: failed in register_filesystem!\n");
        }
        return status;
}

void __exit exit_intermezzo_fs(void)
{
        int err;

        ENTRY;

        if ( (err = unregister_filesystem(&presto_fs_type)) != 0 ) {
                CERROR("presto: failed to unregister filesystem\n");
        }
        if ( (err = unregister_filesystem(&vpresto_fs_type)) != 0 ) {
                CERROR("vpresto: failed to unregister filesystem\n");
        }

        presto_psdev_cleanup();
        cleanup_intermezzo_sysctl();
        presto_cleanup_ddata_cache();
        CERROR("after cleanup: kmem %ld, vmem %ld\n",
               presto_kmemory, presto_vmemory);
}


MODULE_AUTHOR("Cluster Filesystems Inc. <info@clusterfs.com>");
MODULE_DESCRIPTION("InterMezzo Kernel/Intersync communications " INTERMEZZO_VERSION);
MODULE_LICENSE("GPL");

module_init(init_intermezzo_fs)
module_exit(exit_intermezzo_fs)
