/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2001 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@cambridge.redhat.com>
 *
 * The original JFFS, from which the design for JFFS2 was derived,
 * was designed and implemented by Axis Communications AB.
 *
 * The contents of this file are subject to the Red Hat eCos Public
 * License Version 1.1 (the "Licence"); you may not use this file
 * except in compliance with the Licence.  You may obtain a copy of
 * the Licence at http://www.redhat.com/
 *
 * Software distributed under the Licence is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing rights and
 * limitations under the Licence.
 *
 * The Original Code is JFFS2 - Journalling Flash File System, version 2
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License version 2 (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of the
 * above.  If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the RHEPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the RHEPL or the GPL.
 *
 * $Id: malloc.c,v 1.16 2001/03/15 15:38:24 dwmw2 Exp $
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/jffs2.h>
#include "nodelist.h"

#if 0
#define JFFS2_SLAB_POISON SLAB_POISON
#else
#define JFFS2_SLAB_POISON 0
#endif

/* These are initialised to NULL in the kernel startup code.
   If you're porting to other operating systems, beware */
static kmem_cache_t *full_dnode_slab;
static kmem_cache_t *raw_dirent_slab;
static kmem_cache_t *raw_inode_slab;
static kmem_cache_t *tmp_dnode_info_slab;
static kmem_cache_t *raw_node_ref_slab;
static kmem_cache_t *node_frag_slab;
static kmem_cache_t *inode_cache_slab;

void jffs2_free_tmp_dnode_info_list(struct jffs2_tmp_dnode_info *tn)
{
	struct jffs2_tmp_dnode_info *next;

	while (tn) {
		next = tn;
		tn = tn->next;
		jffs2_free_full_dnode(next->fn);
		jffs2_free_tmp_dnode_info(next);
	}
}

void jffs2_free_full_dirent_list(struct jffs2_full_dirent *fd)
{
	struct jffs2_full_dirent *next;

	while (fd) {
		next = fd->next;
		jffs2_free_full_dirent(fd);
		fd = next;
	}
}

int __init jffs2_create_slab_caches(void)
{
	full_dnode_slab = kmem_cache_create("jffs2_full_dnode", sizeof(struct jffs2_full_dnode), 0, JFFS2_SLAB_POISON, NULL, NULL);
	if (!full_dnode_slab)
		goto err;

	raw_dirent_slab = kmem_cache_create("jffs2_raw_dirent", sizeof(struct jffs2_raw_dirent), 0, JFFS2_SLAB_POISON, NULL, NULL);
	if (!raw_dirent_slab)
		goto err;

	raw_inode_slab = kmem_cache_create("jffs2_raw_inode", sizeof(struct jffs2_raw_inode), 0, JFFS2_SLAB_POISON, NULL, NULL);
	if (!raw_inode_slab)
		goto err;

	tmp_dnode_info_slab = kmem_cache_create("jffs2_tmp_dnode", sizeof(struct jffs2_tmp_dnode_info), 0, JFFS2_SLAB_POISON, NULL, NULL);
	if (!tmp_dnode_info_slab)
		goto err;

	raw_node_ref_slab = kmem_cache_create("jffs2_raw_node_ref", sizeof(struct jffs2_raw_node_ref), 0, JFFS2_SLAB_POISON, NULL, NULL);
	if (!raw_node_ref_slab)
		goto err;

	node_frag_slab = kmem_cache_create("jffs2_node_frag", sizeof(struct jffs2_node_frag), 0, JFFS2_SLAB_POISON, NULL, NULL);
	if (!node_frag_slab)
		goto err;

	inode_cache_slab = kmem_cache_create("jffs2_inode_cache", sizeof(struct jffs2_inode_cache), 0, JFFS2_SLAB_POISON, NULL, NULL);

	if (inode_cache_slab)
		return 0;
 err:
	jffs2_destroy_slab_caches();
	return -ENOMEM;
}

void jffs2_destroy_slab_caches(void)
{
	if(full_dnode_slab)
		kmem_cache_destroy(full_dnode_slab);
	if(raw_dirent_slab)
		kmem_cache_destroy(raw_dirent_slab);
	if(raw_inode_slab)
		kmem_cache_destroy(raw_inode_slab);
	if(tmp_dnode_info_slab)
		kmem_cache_destroy(tmp_dnode_info_slab);
	if(raw_node_ref_slab)
		kmem_cache_destroy(raw_node_ref_slab);
	if(node_frag_slab)
		kmem_cache_destroy(node_frag_slab);
	if(inode_cache_slab)
		kmem_cache_destroy(inode_cache_slab);

}

struct jffs2_full_dirent *jffs2_alloc_full_dirent(int namesize)
{
	return kmalloc(sizeof(struct jffs2_full_dirent) + namesize, GFP_KERNEL);
}

void jffs2_free_full_dirent(struct jffs2_full_dirent *x)
{
	kfree(x);
}

struct jffs2_full_dnode *jffs2_alloc_full_dnode(void)
{
	void *ret = kmem_cache_alloc(full_dnode_slab, GFP_KERNEL);
	return ret;
}

void jffs2_free_full_dnode(struct jffs2_full_dnode *x)
{
	kmem_cache_free(full_dnode_slab, x);
}

struct jffs2_raw_dirent *jffs2_alloc_raw_dirent(void)
{
	return kmem_cache_alloc(raw_dirent_slab, GFP_KERNEL);
}

void jffs2_free_raw_dirent(struct jffs2_raw_dirent *x)
{
	kmem_cache_free(raw_dirent_slab, x);
}

struct jffs2_raw_inode *jffs2_alloc_raw_inode(void)
{
	return kmem_cache_alloc(raw_inode_slab, GFP_KERNEL);
}

void jffs2_free_raw_inode(struct jffs2_raw_inode *x)
{
	kmem_cache_free(raw_inode_slab, x);
}

struct jffs2_tmp_dnode_info *jffs2_alloc_tmp_dnode_info(void)
{
	return kmem_cache_alloc(tmp_dnode_info_slab, GFP_KERNEL);
}

void jffs2_free_tmp_dnode_info(struct jffs2_tmp_dnode_info *x)
{
	kmem_cache_free(tmp_dnode_info_slab, x);
}

struct jffs2_raw_node_ref *jffs2_alloc_raw_node_ref(void)
{
	return kmem_cache_alloc(raw_node_ref_slab, GFP_KERNEL);
}

void jffs2_free_raw_node_ref(struct jffs2_raw_node_ref *x)
{
	kmem_cache_free(raw_node_ref_slab, x);
}

struct jffs2_node_frag *jffs2_alloc_node_frag(void)
{
	return kmem_cache_alloc(node_frag_slab, GFP_KERNEL);
}

void jffs2_free_node_frag(struct jffs2_node_frag *x)
{
	kmem_cache_free(node_frag_slab, x);
}

struct jffs2_inode_cache *jffs2_alloc_inode_cache(void)
{
	struct jffs2_inode_cache *ret = kmem_cache_alloc(inode_cache_slab, GFP_KERNEL);
	D1(printk(KERN_DEBUG "Allocated inocache at %p\n", ret));
	return ret;
}

void jffs2_free_inode_cache(struct jffs2_inode_cache *x)
{
	D1(printk(KERN_DEBUG "Freeing inocache at %p\n", x));
	kmem_cache_free(inode_cache_slab, x);
}

