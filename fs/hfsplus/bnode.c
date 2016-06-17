/*
 *  linux/fs/hfsplus/bnode.c
 *
 * Copyright (C) 2001
 * Brad Boyer (flar@allandria.com)
 * (C) 2003 Ardis Technologies <roman@ardistech.com>
 *
 * Handle basic btree node operations
 */

#include <linux/string.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/fs.h>
#include <linux/swap.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#include <linux/buffer_head.h>
#endif

#include "hfsplus_fs.h"
#include "hfsplus_raw.h"

#define REF_PAGES	0

int hfsplus_update_idx_rec(struct hfsplus_find_data *fd);

/* Get the given block associated with the tree owning node */
struct buffer_head *hfsplus_getblk(struct inode *inode, unsigned long n)
{
	struct super_block *sb;
	struct buffer_head tmp_bh;

	sb = inode->i_sb;
	if (hfsplus_get_block(inode, n, &tmp_bh, 1)) {
		printk("HFS+-fs: Failed to find block for B*Tree data\n");
		return NULL;
	}
	return sb_bread(sb, tmp_bh.b_blocknr);
}

/* Copy a specified range of bytes from the raw data of a node */
void hfsplus_bnode_readbytes(hfsplus_bnode *node, void *buf,
			     unsigned long off, unsigned long len)
{
	unsigned long l;
	struct page **pagep;

	off += node->page_offset;
	pagep = node->page + (off >> PAGE_CACHE_SHIFT);
	off &= ~PAGE_CACHE_MASK;

	l = min(len, PAGE_CACHE_SIZE - off);
	memcpy(buf, hfsplus_kmap(*pagep) + off, l);
	hfsplus_kunmap(*pagep++);

	while ((len -= l)) {
		buf += l;
		l = min(len, PAGE_CACHE_SIZE);
		memcpy(buf, hfsplus_kmap(*pagep), l);
		hfsplus_kunmap(*pagep++);
	}
}

u16 hfsplus_bnode_read_u16(hfsplus_bnode *node, unsigned long off)
{
	u16 data;
	// optimize later...
	hfsplus_bnode_readbytes(node, &data, off, 2);
	return be16_to_cpu(data);
}

void hfsplus_bnode_read_key(hfsplus_bnode *node, void *key, unsigned long off)
{
	hfsplus_btree *tree;
	unsigned long key_len;

	tree = node->tree;
	if (node->kind == HFSPLUS_NODE_LEAF ||
	    tree->attributes & HFSPLUS_TREE_VAR_NDXKEY_SIZE)
		key_len = hfsplus_bnode_read_u16(node, off) + 2;
	else
		key_len = tree->max_key_len + 2;
	
	hfsplus_bnode_readbytes(node, key, off, key_len);	
}

void hfsplus_bnode_writebytes(hfsplus_bnode *node, void *buf,
			      unsigned long off, unsigned long len)
{
	unsigned long l;
	struct page **pagep;

	off += node->page_offset;
	pagep = node->page + (off >> PAGE_CACHE_SHIFT);
	off &= ~PAGE_CACHE_MASK;

	l = min(len, PAGE_CACHE_SIZE - off);
	memcpy(hfsplus_kmap(*pagep) + off, buf, l);
	set_page_dirty(*pagep);
	hfsplus_kunmap(*pagep++);

	while ((len -= l)) {
		buf += l;
		l = min(len, PAGE_CACHE_SIZE);
		memcpy(hfsplus_kmap(*pagep), buf, l);
		set_page_dirty(*pagep);
		hfsplus_kunmap(*pagep++);
	}
}

void hfsplus_bnode_write_u16(hfsplus_bnode *node, unsigned long off, u16 data)
{
	data = cpu_to_be16(data);
	// optimize later...
	hfsplus_bnode_writebytes(node, &data, off, 2);
}

void hfsplus_bnode_copybytes(hfsplus_bnode *dst_node, unsigned long dst,
			     hfsplus_bnode *src_node, unsigned long src, unsigned long len)
{
	struct hfsplus_btree *tree;
	struct page **src_page, **dst_page;
	unsigned long l;

	dprint(DBG_BNODE_MOD, "copybytes: %lu,%lu,%lu\n", dst, src, len);
	if (!len)
		return;
	tree = src_node->tree;
	src += src_node->page_offset;
	dst += dst_node->page_offset;
	src_page = src_node->page + (src >> PAGE_CACHE_SHIFT);
	src &= ~PAGE_CACHE_MASK;
	dst_page = dst_node->page + (dst >> PAGE_CACHE_SHIFT);
	dst &= ~PAGE_CACHE_MASK;

	if (src == dst) {
		l = min(len, PAGE_CACHE_SIZE - src);
		memcpy(hfsplus_kmap(*dst_page) + src, hfsplus_kmap(*src_page) + src, l);
		hfsplus_kunmap(*src_page++);
		set_page_dirty(*dst_page);
		hfsplus_kunmap(*dst_page++);

		while ((len -= l)) {
			l = min(len, PAGE_CACHE_SIZE);
			memcpy(hfsplus_kmap(*dst_page), hfsplus_kmap(*src_page), l);
			hfsplus_kunmap(*src_page++);
			set_page_dirty(*dst_page);
			hfsplus_kunmap(*dst_page++);
		}
	} else {
		void *src_ptr, *dst_ptr;

		do {
			src_ptr = hfsplus_kmap(*src_page) + src;
			dst_ptr = hfsplus_kmap(*dst_page) + dst;
			if (PAGE_CACHE_SIZE - src < PAGE_CACHE_SIZE - dst) {
				l = PAGE_CACHE_SIZE - src;
				src = 0;
				dst += l;
			} else {
				l = PAGE_CACHE_SIZE - dst;
				src += l;
				dst = 0;
			}
			l = min(len, l);
			memcpy(dst_ptr, src_ptr, l);
			hfsplus_kunmap(*src_page);
			set_page_dirty(*dst_page);
			hfsplus_kunmap(*dst_page);
			if (!dst)
				dst_page++;
			else
				src_page++;
		} while ((len -= l));
	}
}

void hfsplus_bnode_movebytes(hfsplus_bnode *node, unsigned long dst,
			     unsigned long src, unsigned long len)
{
	struct page **src_page, **dst_page;
	unsigned long l;

	dprint(DBG_BNODE_MOD, "movebytes: %lu,%lu,%lu\n", dst, src, len);
	if (!len)
		return;
	src += node->page_offset;
	dst += node->page_offset;
	if (dst > src) {
		src += len - 1;
		src_page = node->page + (src >> PAGE_CACHE_SHIFT);
		src = (src & ~PAGE_CACHE_MASK) + 1;
		dst += len - 1;
		dst_page = node->page + (dst >> PAGE_CACHE_SHIFT);
		dst = (dst & ~PAGE_CACHE_MASK) + 1;

		if (src == dst) {
			while (src < len) {
				memmove(hfsplus_kmap(*dst_page), hfsplus_kmap(*src_page), src);
				hfsplus_kunmap(*src_page--);
				set_page_dirty(*dst_page);
				hfsplus_kunmap(*dst_page--);
				len -= src;
				src = PAGE_CACHE_SIZE;
			}
			src -= len;
			memmove(hfsplus_kmap(*dst_page) + src, hfsplus_kmap(*src_page) + src, len);
			hfsplus_kunmap(*src_page);
			set_page_dirty(*dst_page);
			hfsplus_kunmap(*dst_page);
		} else {
			void *src_ptr, *dst_ptr;

			do {
				src_ptr = hfsplus_kmap(*src_page) + src;
				dst_ptr = hfsplus_kmap(*dst_page) + dst;
				if (src < dst) {
					l = src;
					src = PAGE_CACHE_SIZE;
					dst -= l;
				} else {
					l = dst;
					src -= l;
					dst = PAGE_CACHE_SIZE;
				}
				l = min(len, l);
				memmove(dst_ptr - l, src_ptr - l, l);
				hfsplus_kunmap(*src_page);
				set_page_dirty(*dst_page);
				hfsplus_kunmap(*dst_page);
				if (dst == PAGE_CACHE_SIZE)
					dst_page--;
				else
					src_page--;
			} while ((len -= l));
		}
	} else {
		src_page = node->page + (src >> PAGE_CACHE_SHIFT);
		src &= ~PAGE_CACHE_MASK;
		dst_page = node->page + (dst >> PAGE_CACHE_SHIFT);
		dst &= ~PAGE_CACHE_MASK;

		if (src == dst) {
			l = min(len, PAGE_CACHE_SIZE - src);
			memmove(hfsplus_kmap(*dst_page) + src, hfsplus_kmap(*src_page) + src, l);
			hfsplus_kunmap(*src_page++);
			set_page_dirty(*dst_page);
			hfsplus_kunmap(*dst_page++);

			while ((len -= l)) {
				l = min(len, PAGE_CACHE_SIZE);
				memmove(hfsplus_kmap(*dst_page), hfsplus_kmap(*src_page), l);
				hfsplus_kunmap(*src_page++);
				set_page_dirty(*dst_page);
				hfsplus_kunmap(*dst_page++);
			}
		} else {
			void *src_ptr, *dst_ptr;

			do {
				src_ptr = hfsplus_kmap(*src_page) + src;
				dst_ptr = hfsplus_kmap(*dst_page) + dst;
				if (PAGE_CACHE_SIZE - src < PAGE_CACHE_SIZE - dst) {
					l = PAGE_CACHE_SIZE - src;
					src = 0;
					dst += l;
				} else {
					l = PAGE_CACHE_SIZE - dst;
					src += l;
					dst = 0;
				}
				l = min(len, l);
				memmove(dst_ptr, src_ptr, l);
				hfsplus_kunmap(*src_page);
				set_page_dirty(*dst_page);
				hfsplus_kunmap(*dst_page);
				if (!dst)
					dst_page++;
				else
					src_page++;
			} while ((len -= l));
		}
	}
}

void hfsplus_bnode_dump(hfsplus_bnode *node)
{
	hfsplus_btree_node_desc desc;
	u32 cnid;
	int i, off, key_off;

	dprint(DBG_BNODE_MOD, "bnode: %d\n", node->this);
	hfsplus_bnode_readbytes(node, &desc, 0, sizeof(desc));
	dprint(DBG_BNODE_MOD, "%d, %d, %d, %d, %d\n",
		be32_to_cpu(desc.next), be32_to_cpu(desc.prev),
		desc.kind, desc.height, be16_to_cpu(desc.num_rec));

	off = node->tree->node_size - 2;
	for (i = be16_to_cpu(desc.num_rec); i >= 0; off -= 2, i--) {
		key_off = hfsplus_bnode_read_u16(node, off);
		dprint(DBG_BNODE_MOD, " %d", key_off);
		if (i && node->kind == HFSPLUS_NODE_NDX) {
			int tmp;

			tmp = hfsplus_bnode_read_u16(node, key_off);
			dprint(DBG_BNODE_MOD, " (%d", tmp);
			hfsplus_bnode_readbytes(node, &cnid, key_off + 2 + tmp, 4);
			dprint(DBG_BNODE_MOD, ",%d)", be32_to_cpu(cnid));
		}
	}
	dprint(DBG_BNODE_MOD, "\n");
}

int hfsplus_btree_add_level(hfsplus_btree *tree)
{
	hfsplus_bnode *node, *new_node;
	hfsplus_btree_node_desc node_desc;
	int key_size, rec;
	u32 cnid;

	node = NULL;
	if (tree->root)
		node = hfsplus_find_bnode(tree, tree->root);
	new_node = hfsplus_btree_alloc_node(tree);
	if (IS_ERR(new_node))
		return PTR_ERR(new_node);

	tree->root = new_node->this;
	if (!tree->depth) {
		tree->leaf_head = tree->leaf_tail = new_node->this;
		new_node->kind = HFSPLUS_NODE_LEAF;
		new_node->num_recs = 0;
	} else {
		new_node->kind = HFSPLUS_NODE_NDX;
		new_node->num_recs = 1;
	}
	new_node->parent = 0;
	new_node->next = 0;
	new_node->prev = 0;
	new_node->height = ++tree->depth;

	node_desc.next = cpu_to_be32(new_node->next);
	node_desc.prev = cpu_to_be32(new_node->prev);
	node_desc.kind = new_node->kind;
	node_desc.height = new_node->height;
	node_desc.num_rec = cpu_to_be16(new_node->num_recs);
	node_desc.reserved = 0;
	hfsplus_bnode_writebytes(new_node, &node_desc, 0, sizeof(node_desc));

	rec = tree->node_size - 2;
	hfsplus_bnode_write_u16(new_node, rec, 14);

	if (node) {
		/* insert old root idx into new root */
		node->parent = tree->root;
		key_size = hfsplus_bnode_read_u16(node, 14) + 2;
		// key_size if index node
		hfsplus_bnode_copybytes(new_node, 14, node, 14, key_size);
		cnid = cpu_to_be32(node->this);
		hfsplus_bnode_writebytes(new_node, &cnid, 14 + key_size, 4);

		rec -= 2;
		hfsplus_bnode_write_u16(new_node, rec, 14 + key_size + 4);

		hfsplus_put_bnode(node);
	}
	hfsplus_put_bnode(new_node);
	mark_inode_dirty(tree->inode);

	return 0;
}

hfsplus_bnode *hfsplus_bnode_split(struct hfsplus_find_data *fd)
{
	hfsplus_btree *tree;
	hfsplus_bnode *node, *new_node;
	hfsplus_btree_node_desc node_desc;
	int num_recs, new_rec_off, new_off, old_rec_off;
	int data_start, data_end, size;

	tree = fd->tree;
	node = fd->bnode;
	new_node = hfsplus_btree_alloc_node(tree);
	if (IS_ERR(new_node))
		return new_node;
	hfsplus_get_bnode(node);
	dprint(DBG_BNODE_MOD, "split_nodes: %d - %d - %d\n",
		node->this, new_node->this, node->next);
	new_node->next = node->next;
	new_node->prev = node->this;
	new_node->parent = node->parent;
	new_node->kind = node->kind;
	new_node->height = node->height;

	size = tree->node_size / 2;
	old_rec_off = tree->node_size - 4;
	num_recs = 1;
	for (;;) {
		data_start = hfsplus_bnode_read_u16(node, old_rec_off);
		if (data_start > size)
			break;
		old_rec_off -= 2;
		num_recs++;
		if (num_recs < node->num_recs)
			continue;
		/* panic? */
		hfsplus_put_bnode(node);
		hfsplus_put_bnode(new_node);
		return NULL;
	}

	if (fd->record + 1 < num_recs) {
		/* new record is in the lower half,
		 * so leave some more space there
		 */
		old_rec_off += 2;
		num_recs--;
		data_start = hfsplus_bnode_read_u16(node, old_rec_off);
	} else {
		hfsplus_put_bnode(node);
		hfsplus_get_bnode(new_node);
		fd->bnode = new_node;
		fd->record -= num_recs;
		fd->keyoffset -= data_start;
		fd->entryoffset -= data_start;
	}
	new_node->num_recs = node->num_recs - num_recs;
	node->num_recs = num_recs;

	new_rec_off = tree->node_size - 2;
	new_off = 14;
	size = data_start - new_off;
	num_recs = new_node->num_recs;
	data_end = data_start;
	while (num_recs) {
		hfsplus_bnode_write_u16(new_node, new_rec_off, new_off);
		old_rec_off -= 2;
		new_rec_off -= 2;
		data_end = hfsplus_bnode_read_u16(node, old_rec_off);
		new_off = data_end - size;
		num_recs--;
	}
	hfsplus_bnode_write_u16(new_node, new_rec_off, new_off);
	hfsplus_bnode_copybytes(new_node, 14, node, data_start, data_end - data_start);

	/* update new bnode header */
	node_desc.next = cpu_to_be32(new_node->next);
	node_desc.prev = cpu_to_be32(new_node->prev);
	node_desc.kind = new_node->kind;
	node_desc.height = new_node->height;
	node_desc.num_rec = cpu_to_be16(new_node->num_recs);
	node_desc.reserved = 0;
	hfsplus_bnode_writebytes(new_node, &node_desc, 0, sizeof(node_desc));

	/* update previous bnode header */
	node->next = new_node->this;
	hfsplus_bnode_readbytes(node, &node_desc, 0, sizeof(node_desc));
	node_desc.next = cpu_to_be32(node->next);
	node_desc.num_rec = cpu_to_be16(node->num_recs);
	hfsplus_bnode_writebytes(node, &node_desc, 0, sizeof(node_desc));

	/* update next bnode header */
	if (new_node->next) {
		hfsplus_bnode *next_node = hfsplus_find_bnode(tree, new_node->next);
		next_node->prev = new_node->this;
		hfsplus_bnode_readbytes(next_node, &node_desc, 0, sizeof(node_desc));
		node_desc.prev = cpu_to_be32(next_node->prev);
		hfsplus_bnode_writebytes(next_node, &node_desc, 0, sizeof(node_desc));
		hfsplus_put_bnode(next_node);
	} else if (node->this == tree->leaf_tail) {
		/* if there is no next node, this might be the new tail */
		tree->leaf_tail = new_node->this;
		mark_inode_dirty(tree->inode);
	}

	hfsplus_bnode_dump(node);
	hfsplus_bnode_dump(new_node);
	hfsplus_put_bnode(node);

	return new_node;
}

int hfsplus_bnode_insert_rec(struct hfsplus_find_data *fd, void *entry, int entry_len)
{
	hfsplus_btree *tree;
	hfsplus_bnode *node, *new_node;
	int size, key_len, rec;
	int data_off, end_off;
	int idx_rec_off, data_rec_off, end_rec_off;
	u32 cnid;

	tree = fd->tree;
	if (!fd->bnode) {
		if (!tree->root)
			hfsplus_btree_add_level(tree);
		fd->bnode = hfsplus_find_bnode(tree, tree->leaf_head);
		fd->record = -1;
	}
	new_node = NULL;
again:
	/* new record idx and complete record size */
	rec = fd->record + 1;
	key_len = be16_to_cpu(fd->search_key->key_len) + 2;
	size = key_len + entry_len;

	node = fd->bnode;
	hfsplus_bnode_dump(node);
	/* get last offset */
	end_rec_off = tree->node_size - (node->num_recs + 1) * 2;
	end_off = hfsplus_bnode_read_u16(node, end_rec_off);
	end_rec_off -= 2;
	dprint(DBG_BNODE_MOD, "insert_rec: %d, %d, %d, %d\n", rec, size, end_off, end_rec_off);
	if (size > end_rec_off - end_off) {
		if (new_node)
			panic("not enough room!\n");
		new_node = hfsplus_bnode_split(fd);
		if (IS_ERR(new_node))
			return PTR_ERR(new_node);
		goto again;
	}
	if (node->kind == HFSPLUS_NODE_LEAF) {
		tree->leaf_count++;
		mark_inode_dirty(tree->inode);
	}
	node->num_recs++;
	/* write new last offset */
	hfsplus_bnode_write_u16(node, offsetof(hfsplus_btree_node_desc, num_rec), node->num_recs);
	hfsplus_bnode_write_u16(node, end_rec_off, end_off + size);
	data_off = end_off;
	data_rec_off = end_rec_off + 2;
	idx_rec_off = tree->node_size - (rec + 1) * 2;
	if (idx_rec_off == data_rec_off)
		goto skip;
	/* move all following entries */
	do {
		data_off = hfsplus_bnode_read_u16(node, data_rec_off + 2);
		hfsplus_bnode_write_u16(node, data_rec_off, data_off + size);
		data_rec_off += 2;
	} while (data_rec_off < idx_rec_off);

	/* move data away */
	hfsplus_bnode_movebytes(node, data_off + size, data_off,
				end_off - data_off);

skip:
	hfsplus_bnode_writebytes(node, fd->search_key, data_off, key_len);
	hfsplus_bnode_writebytes(node, entry, data_off + key_len, entry_len);
	hfsplus_bnode_dump(node);

	if (new_node) {
		if (!rec && new_node != node)
			hfsplus_update_idx_rec(fd);

		hfsplus_put_bnode(fd->bnode);
		if (!new_node->parent) {
			hfsplus_btree_add_level(tree);
			new_node->parent = tree->root;
		}
		fd->bnode = hfsplus_find_bnode(tree, new_node->parent);

		/* create index data entry */
		cnid = cpu_to_be32(new_node->this);
		entry = &cnid;
		entry_len = sizeof(cnid);

		/* get index key */
		hfsplus_bnode_read_key(new_node, fd->search_key, 14);
		hfsplus_find_rec(fd->bnode, fd);

		hfsplus_put_bnode(new_node);
		new_node = NULL;
		goto again;
	}

	if (!rec)
		hfsplus_update_idx_rec(fd);

	return 0;
}

int hfsplus_update_idx_rec(struct hfsplus_find_data *fd)
{
	hfsplus_btree *tree;
	hfsplus_bnode *node, *new_node, *parent;
	int newkeylen, diff;
	int rec, rec_off, end_rec_off;
	int start_off, end_off;

	tree = fd->tree;
	node = fd->bnode;
	new_node = NULL;
	if (!node->parent)
		return 0;

again:
	parent = hfsplus_find_bnode(tree, node->parent);
	hfsplus_find_rec(parent, fd);
	hfsplus_bnode_dump(parent);
	rec = fd->record;

	/* size difference between old and new key */
	newkeylen = hfsplus_bnode_read_u16(node, 14) + 2;
	dprint(DBG_BNODE_MOD, "update_rec: %d, %d, %d\n", rec, fd->keylength, newkeylen);

	rec_off = tree->node_size - (rec + 2) * 2;
	end_rec_off = tree->node_size - (parent->num_recs + 1) * 2;
	diff = newkeylen - fd->keylength;
	if (!diff)
		goto skip;
	if (diff > 0) {
		end_off = hfsplus_bnode_read_u16(parent, end_rec_off);
		if (end_rec_off - end_off < diff) {

			printk("splitting index node...\n");
			fd->bnode = parent;
			new_node = hfsplus_bnode_split(fd);
			if (IS_ERR(new_node))
				return PTR_ERR(new_node);
			parent = fd->bnode;
			rec = fd->record;
			rec_off = tree->node_size - (rec + 2) * 2;
			end_rec_off = tree->node_size - (parent->num_recs + 1) * 2;
		}
	}

	end_off = start_off = hfsplus_bnode_read_u16(parent, rec_off);
	hfsplus_bnode_write_u16(parent, rec_off, start_off + diff);
	start_off -= 4;	/* move previous cnid too */

	while (rec_off > end_rec_off) {
		rec_off -= 2;
		end_off = hfsplus_bnode_read_u16(parent, rec_off);
		hfsplus_bnode_write_u16(parent, rec_off, end_off + diff);
	}
	hfsplus_bnode_movebytes(parent, start_off + diff, start_off,
				end_off - start_off);
skip:
	hfsplus_bnode_copybytes(parent, fd->keyoffset, node, 14, newkeylen);
	hfsplus_bnode_dump(parent);

	hfsplus_put_bnode(node);
	node = parent;

	if (new_node) {
		u32 cnid;

		fd->bnode = hfsplus_find_bnode(tree, new_node->parent);
		/* create index key and entry */
		hfsplus_bnode_read_key(new_node, fd->search_key, 14);
		cnid = cpu_to_be32(new_node->this);

		hfsplus_find_rec(fd->bnode, fd);
		hfsplus_bnode_insert_rec(fd, &cnid, sizeof(cnid));
		hfsplus_put_bnode(fd->bnode);
		hfsplus_put_bnode(new_node);

		if (!rec) {
			if (new_node == node)
				goto out;
			/* restore search_key */
			hfsplus_bnode_read_key(node, fd->search_key, 14);
		}
	}

	if (!rec && node->parent)
		goto again;
out:
	fd->bnode = node;
	return 0;
}

int hfsplus_bnode_remove_rec(struct hfsplus_find_data *fd)
{
	hfsplus_btree *tree;
	hfsplus_bnode *node, *parent;
	int end_off, rec_off, data_off, size;

	tree = fd->tree;
	node = fd->bnode;
again:
	rec_off = tree->node_size - (fd->record + 2) * 2;
	end_off = tree->node_size - (node->num_recs + 1) * 2;

	if (node->kind == HFSPLUS_NODE_LEAF) {
		tree->leaf_count--;
		mark_inode_dirty(tree->inode);
	}
	hfsplus_bnode_dump(node);
	dprint(DBG_BNODE_MOD, "remove_rec: %d, %d\n", fd->record, fd->keylength + fd->entrylength);
	if (!--node->num_recs) {
		hfsplus_btree_remove_node(node);
		if (!node->parent)
			return 0;
		parent = hfsplus_find_bnode(tree, node->parent);
		if (!parent)
			return -EIO;
		hfsplus_put_bnode(node);
		node = fd->bnode = parent;

		hfsplus_find_rec(node, fd);
		goto again;
	}
	hfsplus_bnode_write_u16(node, offsetof(hfsplus_btree_node_desc, num_rec), node->num_recs);

	if (rec_off == end_off)
		goto skip;
	size = fd->keylength + fd->entrylength;

	do {
		data_off = hfsplus_bnode_read_u16(node, rec_off);
		hfsplus_bnode_write_u16(node, rec_off + 2, data_off - size);
		rec_off -= 2;
	} while (rec_off >= end_off);

	/* fill hole */
	hfsplus_bnode_movebytes(node, fd->keyoffset, fd->keyoffset + size,
				data_off - fd->keyoffset - size);
skip:
	hfsplus_bnode_dump(node);
	if (!fd->record)
		hfsplus_update_idx_rec(fd);
	return 0;
}

/* Check for valid kind/height pairs , return 0 for bad pairings */
static int hfsplus_check_kh(hfsplus_btree *tree, u8 kind, u8 height)
{
	if ((kind == HFSPLUS_NODE_HEAD) || (kind == HFSPLUS_NODE_MAP)) {
		if (height != 0)
			goto hk_error;
	} else if (kind == HFSPLUS_NODE_LEAF) {
		if (height != 1)
			goto hk_error;
	} else if (kind == HFSPLUS_NODE_NDX) {
		if ((height <= 1) || (height > tree->depth))
			goto hk_error;
	} else {
		printk("HFS+-fs: unknown node type in B*Tree\n");
		return 0;
	}
	return 1;
 hk_error:
	printk("HFS+-fs: corrupt node height in B*Tree\n");
	return 0;
}

static inline int hfsplus_bnode_hash(u32 num)
{
	num = (num >> 16) + num;
	num += num >> 8;
	return num & (NODE_HASH_SIZE - 1);
}

hfsplus_bnode *__hfsplus_find_bnode(hfsplus_btree *tree, u32 cnid)
{
	hfsplus_bnode *node;

	if (cnid >= tree->node_count) {
		printk("HFS+-fs: request for non-existent node %d in B*Tree\n", cnid);
		return NULL;
	}

	for (node = tree->node_hash[hfsplus_bnode_hash(cnid)];
	     node; node = node->next_hash) {
		if (node->this == cnid) {
			return node;
		}
	}
	return NULL;
}

hfsplus_bnode *__hfsplus_create_bnode(hfsplus_btree *tree, u32 cnid)
{
	struct super_block *sb;
	hfsplus_bnode *node, *node2;
	struct address_space *mapping;
	struct page *page;
	int size, block, i, hash;
	loff_t off;

	if (cnid >= tree->node_count) {
		printk("HFS+-fs: request for non-existent node %d in B*Tree\n", cnid);
		return NULL;
	}

	sb = tree->inode->i_sb;
	size = sizeof(hfsplus_bnode) + tree->pages_per_bnode *
		sizeof(struct page *);
	node = kmalloc(size, GFP_KERNEL);
	if (!node)
		return NULL;
	memset(node, 0, size);
	node->tree = tree;
	node->this = cnid;
	set_bit(HFSPLUS_BNODE_NEW, &node->flags);
	atomic_set(&node->refcnt, 1);
	dprint(DBG_BNODE_REFS, "new_node(%d:%d): 1\n",
	       node->tree->cnid, node->this);
	init_waitqueue_head(&node->lock_wq);
	spin_lock(&tree->hash_lock);
	node2 = __hfsplus_find_bnode(tree, cnid);
	if (!node2) {
		hash = hfsplus_bnode_hash(cnid);
		node->next_hash = tree->node_hash[hash];
		tree->node_hash[hash] = node;
		tree->node_hash_cnt++;
	} else {
		spin_unlock(&tree->hash_lock);
		kfree(node);
		wait_event(node2->lock_wq, !test_bit(HFSPLUS_BNODE_NEW, &node2->flags));
		return node2;
	}
	spin_unlock(&tree->hash_lock);

	mapping = tree->inode->i_mapping;
	off = (loff_t)cnid * tree->node_size;
	block = off >> PAGE_CACHE_SHIFT;
	node->page_offset = off & ~PAGE_CACHE_MASK;
	for (i = 0; i < tree->pages_per_bnode; i++) {
		page = grab_cache_page(mapping, block++);
		if (!page)
			goto fail;
		if (!PageUptodate(page)) {
			if (mapping->a_ops->readpage(NULL, page))
				goto fail;
			wait_on_page_locked(page);
			if (!PageUptodate(page))
				goto fail;
		} else
			unlock_page(page);
#if !REF_PAGES
		page_cache_release(page);
#endif
		node->page[i] = page;
	}

	return node;
fail:
	if (page)
		page_cache_release(page);
	set_bit(HFSPLUS_BNODE_ERROR, &node->flags);
	return node;
}

void __hfsplus_bnode_remove(hfsplus_bnode *node)
{
	hfsplus_bnode **p;

	dprint(DBG_BNODE_REFS, "remove_node(%d:%d): %d\n",
		node->tree->cnid, node->this, atomic_read(&node->refcnt));
	for (p = &node->tree->node_hash[hfsplus_bnode_hash(node->this)];
	     *p && *p != node; p = &(*p)->next_hash)
		;
	if (!*p)
		BUG();
	*p = node->next_hash;
	node->tree->node_hash_cnt--;
}

/* Load a particular node out of a tree */
hfsplus_bnode *hfsplus_find_bnode(hfsplus_btree *tree, u32 num)
{
	hfsplus_bnode *node;
	hfsplus_btree_node_desc *desc;
	int i, rec_off, off, next_off;
	int entry_size, key_size;

	spin_lock(&tree->hash_lock);
	node = __hfsplus_find_bnode(tree, num);
	if (node) {
		hfsplus_get_bnode(node);
		spin_unlock(&tree->hash_lock);
		wait_event(node->lock_wq, !test_bit(HFSPLUS_BNODE_NEW, &node->flags));
		return node;
	}
	spin_unlock(&tree->hash_lock);
	node = __hfsplus_create_bnode(tree, num);
	if (!node)
		return ERR_PTR(-ENOMEM);
	if (!test_bit(HFSPLUS_BNODE_NEW, &node->flags))
		return node;

	desc = (hfsplus_btree_node_desc *)(hfsplus_kmap(node->page[0]) + node->page_offset);
	node->prev = be32_to_cpu(desc->prev);
	node->next = be32_to_cpu(desc->next);
	node->num_recs = be16_to_cpu(desc->num_rec);
	node->kind = desc->kind;
	node->height = desc->height;

	if (!hfsplus_check_kh(tree, desc->kind, desc->height)) {
		hfsplus_kunmap(node->page[0]);
		goto node_error;
	}
	hfsplus_kunmap(node->page[0]);

	rec_off = tree->node_size - 2;
	off = hfsplus_bnode_read_u16(node, rec_off);
	if (off != sizeof(hfsplus_btree_node_desc))
		goto node_error;
	for (i = 1; i <= node->num_recs; off = next_off, i++) {
		rec_off -= 2;
		next_off = hfsplus_bnode_read_u16(node, rec_off);
		if (next_off <= off ||
		    next_off > tree->node_size ||
		    next_off & 1)
			goto node_error;
		entry_size = next_off - off;
		if (node->kind != HFSPLUS_NODE_NDX &&
		    node->kind != HFSPLUS_NODE_LEAF)
			continue;
		key_size = hfsplus_bnode_read_u16(node, off) + 2;
		if (key_size >= entry_size || key_size & 1)
			goto node_error;
	}
	clear_bit(HFSPLUS_BNODE_NEW, &node->flags);
	wake_up(&node->lock_wq);
	return node;

node_error:
	set_bit(HFSPLUS_BNODE_ERROR, &node->flags);
	clear_bit(HFSPLUS_BNODE_NEW, &node->flags);
	wake_up(&node->lock_wq);
	hfsplus_put_bnode(node);
	return ERR_PTR(-EIO);
}

void hfsplus_bnode_free(hfsplus_bnode *node)
{
	//int i;

	//for (i = 0; i < node->tree->pages_per_bnode; i++)
	//	if (node->page[i])
	//		page_cache_release(node->page[i]);
	kfree(node);
}

hfsplus_bnode *hfsplus_create_bnode(hfsplus_btree *tree, u32 num)
{
	hfsplus_bnode *node;
	struct page **pagep;
	int i;

	spin_lock(&tree->hash_lock);
	node = __hfsplus_find_bnode(tree, num);
	spin_unlock(&tree->hash_lock);
	if (node)
		BUG();
	node = __hfsplus_create_bnode(tree, num);
	if (!node)
		return ERR_PTR(-ENOMEM);

	pagep = node->page;
	memset(hfsplus_kmap(*pagep) + node->page_offset, 0,
	       min((int)PAGE_CACHE_SIZE, (int)tree->node_size));
	set_page_dirty(*pagep);
	hfsplus_kunmap(*pagep++);
	for (i = 1; i < tree->pages_per_bnode; i++) {
		memset(hfsplus_kmap(*pagep), 0, PAGE_CACHE_SIZE);
		set_page_dirty(*pagep);
		hfsplus_kunmap(*pagep++);
	}
	clear_bit(HFSPLUS_BNODE_NEW, &node->flags);
	wake_up(&node->lock_wq);

	return node;
}

void hfsplus_get_bnode(hfsplus_bnode *node)
{
	if (node) {
		atomic_inc(&node->refcnt);
#if REF_PAGES
		{
		int i;
		for (i = 0; i < node->tree->pages_per_bnode; i++)
			get_page(node->page[i]);
		}
#endif
		dprint(DBG_BNODE_REFS, "get_node(%d:%d): %d\n",
		       node->tree->cnid, node->this, atomic_read(&node->refcnt));
	}
}

/* Dispose of resources used by a node */
void hfsplus_put_bnode(hfsplus_bnode *node)
{
	if (node) {
		struct hfsplus_btree *tree = node->tree;
		int i;

		dprint(DBG_BNODE_REFS, "put_node(%d:%d): %d\n",
		       node->tree->cnid, node->this, atomic_read(&node->refcnt));
		if (!atomic_read(&node->refcnt))
			BUG();
		if (!atomic_dec_and_lock(&node->refcnt, &tree->hash_lock)) {
#if REF_PAGES
			for (i = 0; i < tree->pages_per_bnode; i++)
				put_page(node->page[i]);
#endif
			return;
		}
		for (i = 0; i < tree->pages_per_bnode; i++) {
			mark_page_accessed(node->page[i]);
#if REF_PAGES
			put_page(node->page[i]);
#endif
		}

		if (test_bit(HFSPLUS_BNODE_DELETED, &node->flags)) {
			__hfsplus_bnode_remove(node);
			spin_unlock(&tree->hash_lock);
			hfsplus_btree_free_node(node);
			hfsplus_bnode_free(node);
			return;
		}
		spin_unlock(&tree->hash_lock);
	}
}

void hfsplus_lock_bnode(hfsplus_bnode *node)
{
	wait_event(node->lock_wq, !test_and_set_bit(HFSPLUS_BNODE_LOCK, &node->flags));
}

void hfsplus_unlock_bnode(hfsplus_bnode *node)
{
	clear_bit(HFSPLUS_BNODE_LOCK, &node->flags);
	if (waitqueue_active(&node->lock_wq))
		wake_up(&node->lock_wq);
}
