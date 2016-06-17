/*
 *  linux/fs/hfsplus/bfind.c
 *
 * Copyright (C) 2001
 * Brad Boyer (flar@allandria.com)
 * (C) 2003 Ardis Technologies <roman@ardistech.com>
 *
 * Search routines for btrees
 */

#include <linux/slab.h>
#include "hfsplus_fs.h"

/* Find the record in bnode that best matches key (not greater than...)*/
void hfsplus_find_rec(hfsplus_bnode *bnode, struct hfsplus_find_data *fd)
{
	int cmpval;
	u16 off, len, keylen;
	int rec;
	int b, e;

	b = 0;
	e = bnode->num_recs - 1;
	do {
		rec = (e + b) / 2;
		len = hfsplus_brec_lenoff(bnode, rec, &off);
		keylen = hfsplus_brec_keylen(bnode, rec);
		hfsplus_bnode_readbytes(bnode, fd->key, off, keylen);
		cmpval = bnode->tree->keycmp(fd->key, fd->search_key);
		if (!cmpval) {
			fd->exact = 1;
			e = rec;
			break;
		}
		if (cmpval < 0)
			b = rec + 1;
		else
			e = rec - 1;
	} while (b <= e);
	//printk("%d: %d,%d,%d\n", bnode->this, b, e, rec);
	if (rec != e && e >= 0) {
		len = hfsplus_brec_lenoff(bnode, e, &off);
		keylen = hfsplus_brec_keylen(bnode, e);
		hfsplus_bnode_readbytes(bnode, fd->key, off, keylen);
	}
	fd->record = e;
	fd->keyoffset = off;
	fd->keylength = keylen;
	fd->entryoffset = off + keylen;
	fd->entrylength = len - keylen;
}

/* Traverse a B*Tree from the root to a leaf finding best fit to key */
/* Return allocated copy of node found, set recnum to best record */
int hfsplus_btree_find(struct hfsplus_find_data *fd)
{
	hfsplus_btree *tree;
	hfsplus_bnode *bnode;
	u32 data, nidx, parent;
	int height, err;

	tree = fd->tree;
	if (fd->bnode)
		hfsplus_put_bnode(fd->bnode);
	fd->bnode = NULL;
	fd->exact = 0;
	nidx = tree->root;
	if (!nidx)
		return -ENOENT;
	height = tree->depth;
	err = 0;
	parent = 0;
	for (;;) {
		bnode = hfsplus_find_bnode(tree, nidx);
		if (!bnode) {
			err = -EIO;
			break;
		}
		if (bnode->height != height)
			goto invalid;
		if (bnode->kind != (--height ? HFSPLUS_NODE_NDX : HFSPLUS_NODE_LEAF))
			goto invalid;
		bnode->parent = parent;

		hfsplus_find_rec(bnode, fd);
		if (fd->record < 0) {
			err = -ENOENT;
			goto release;
		}
		if (!height) {
			if (!fd->exact)
				err = -ENOENT;
			break;
		}

		parent = nidx;
		hfsplus_bnode_readbytes(bnode, &data, fd->entryoffset, 4);
		nidx = be32_to_cpu(data);
		hfsplus_put_bnode(bnode);
	}
	fd->bnode = bnode;
	return err;

invalid:
	printk("HFS+-fs: inconsistency in B*Tree\n");
	err = -EIO;
release:
	hfsplus_put_bnode(bnode);
	return err;
}

int hfsplus_btree_find_entry(struct hfsplus_find_data *fd,
			     void *entry, int entry_len)
{
	int res;

	res = hfsplus_btree_find(fd);
	if (res)
		return res;
	if (fd->entrylength > entry_len)
		return -EINVAL;
	hfsplus_bnode_readbytes(fd->bnode, entry, fd->entryoffset, fd->entrylength);
	return 0;
}

int hfsplus_btree_move(struct hfsplus_find_data *fd, int cnt)
{
	struct hfsplus_btree *tree;
	hfsplus_bnode *bnode;
	int idx, res = 0;
	u16 off, len, keylen;

	bnode = fd->bnode;
	tree = bnode->tree;

	if (cnt < -0xFFFF || cnt > 0xFFFF)
		return -EINVAL;

	if (cnt < 0) {
		cnt = -cnt;
		while (cnt > fd->record) {
			cnt -= fd->record + 1;
			fd->record = bnode->num_recs - 1;
			idx = bnode->prev;
			if (!idx) {
				res = -ENOENT;
				goto out;
			}
			hfsplus_put_bnode(bnode);
			bnode = hfsplus_find_bnode(tree, idx);
			if (!bnode) {
				res = -EIO;
				goto out;
			}
		}
		fd->record -= cnt;
	} else {
		while (cnt >= bnode->num_recs - fd->record) {
			cnt -= bnode->num_recs - fd->record;
			fd->record = 0;
			idx = bnode->next;
			if (!idx) {
				res = -ENOENT;
				goto out;
			}
			hfsplus_put_bnode(bnode);
			bnode = hfsplus_find_bnode(tree, idx);
			if (!bnode) {
				res = -EIO;
				goto out;
			}
		}
		fd->record += cnt;
	}

	len = hfsplus_brec_lenoff(bnode, fd->record, &off);
	keylen = hfsplus_brec_keylen(bnode, fd->record);
	fd->keyoffset = off;
	fd->keylength = keylen;
	fd->entryoffset = off + keylen;
	fd->entrylength = len - keylen;
	hfsplus_bnode_readbytes(bnode, fd->key, off, keylen);
out:
	fd->bnode = bnode;
	return res;
}

int hfsplus_find_init(hfsplus_btree *tree, struct hfsplus_find_data *fd)
{
	fd->tree = tree;
	fd->bnode = NULL;
	fd->search_key = kmalloc(tree->max_key_len + 2, GFP_KERNEL);
	if (!fd->search_key)
		return -ENOMEM;
	fd->key = kmalloc(tree->max_key_len + 2, GFP_KERNEL);
	if (!fd->key) {
		kfree(fd->search_key);
		return -ENOMEM;
	}
	dprint(DBG_BNODE_REFS, "find_init: %d (%p)\n", tree->cnid, __builtin_return_address(0));
	down(&tree->tree_lock);
	return 0;
}

void hfsplus_find_exit(struct hfsplus_find_data *fd)
{
	hfsplus_put_bnode(fd->bnode);
	kfree(fd->search_key);
	kfree(fd->key);
	dprint(DBG_BNODE_REFS, "find_exit: %d (%p)\n", fd->tree->cnid, __builtin_return_address(0));
	up(&fd->tree->tree_lock);
	fd->tree = NULL;
}
