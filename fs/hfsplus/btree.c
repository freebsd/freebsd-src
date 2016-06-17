/*
 *  linux/fs/hfsplus/btree.c
 *
 * Copyright (C) 2001
 * Brad Boyer (flar@allandria.com)
 * (C) 2003 Ardis Technologies <roman@ardistech.com>
 *
 * Handle opening/closing btree
 */

#include <linux/slab.h>
#include <linux/pagemap.h>
#include <asm/div64.h>

#include "hfsplus_fs.h"
#include "hfsplus_raw.h"

/* Release resources used by a btree */
void hfsplus_close_btree(struct hfsplus_btree *tree)
{
	hfsplus_bnode *node;
	int i;

	if (!tree)
		return;

	for (i = 0; i < NODE_HASH_SIZE; i++) {
		while ((node = tree->node_hash[i])) {
			tree->node_hash[i] = node->next_hash;
			if (atomic_read(&node->refcnt))
				printk("HFS+: node %d:%d still has %d user(s)!\n",
					node->tree->cnid, node->this, atomic_read(&node->refcnt));
			hfsplus_bnode_free(node);
			tree->node_hash_cnt--;
		}
	}
	iput(tree->inode);
	kfree(tree);
}

/* Fill in extra data in tree structure from header node */
static void hfsplus_read_treeinfo(hfsplus_btree *tree, hfsplus_btree_head *hdr)
{
	unsigned int shift, size;

	if (!tree || !hdr)
		return;

	tree->root = be32_to_cpu(hdr->root);
	tree->leaf_count = be32_to_cpu(hdr->leaf_count);
	tree->leaf_head = be32_to_cpu(hdr->leaf_head);
	tree->leaf_tail = be32_to_cpu(hdr->leaf_tail);
	tree->node_count = be32_to_cpu(hdr->node_count);
	tree->free_nodes = be32_to_cpu(hdr->free_nodes);
	tree->attributes = be32_to_cpu(hdr->attributes);
	tree->node_size = be16_to_cpu(hdr->node_size);
	tree->max_key_len = be16_to_cpu(hdr->max_key_len);
	tree->depth = be16_to_cpu(hdr->depth);

	size = tree->node_size;
	if (size & (size - 1))
		/* panic */;
	for (shift = 0; size >>= 1; shift += 1)
		;
	tree->node_size_shift = shift;

	tree->pages_per_bnode = (tree->node_size + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
}

static void hfsplus_write_treeinfo(hfsplus_btree *tree, hfsplus_btree_head *hdr)
{
	hdr->root = cpu_to_be32(tree->root);
	hdr->leaf_count = cpu_to_be32(tree->leaf_count);
	hdr->leaf_head = cpu_to_be32(tree->leaf_head);
	hdr->leaf_tail = cpu_to_be32(tree->leaf_tail);
	hdr->node_count = cpu_to_be32(tree->node_count);
	hdr->free_nodes = cpu_to_be32(tree->free_nodes);
	hdr->attributes = cpu_to_be32(tree->attributes);
	hdr->depth = cpu_to_be16(tree->depth);
}

/* Get a reference to a B*Tree and do some initial checks */
hfsplus_btree *hfsplus_open_btree(struct super_block *sb, u32 id)
{
	hfsplus_btree *tree;
	hfsplus_btree_head *head;
	struct address_space *mapping;
	struct page *page;

	tree = kmalloc(sizeof(struct hfsplus_btree), GFP_KERNEL);
	if (!tree)
		return NULL;
	memset(tree, 0, sizeof(struct hfsplus_btree));

	init_MUTEX(&tree->tree_lock);
	spin_lock_init(&tree->hash_lock);
	/* Set the correct compare function */
	tree->sb = sb;
	tree->cnid = id;
	if (id == HFSPLUS_EXT_CNID) {
		tree->keycmp = hfsplus_cmp_ext_key;
	} else if (id == HFSPLUS_CAT_CNID) {
		tree->keycmp = hfsplus_cmp_cat_key;
	} else {
		printk("HFS+-fs: unknown B*Tree requested\n");
		goto free_tree;
	}
	tree->inode = iget(sb, id);
	if (!tree->inode)
		goto free_tree;

	mapping = tree->inode->i_mapping;
	page = grab_cache_page(mapping, 0);
	if (!page)
		goto free_tree;
	if (!PageUptodate(page)) {
		if (mapping->a_ops->readpage(NULL, page))
			goto fail_page;
		wait_on_page_locked(page);
		if (!PageUptodate(page))
			goto fail_page;
	} else
		unlock_page(page);

	/* Load the header */
	head = (hfsplus_btree_head *)(kmap(page) + sizeof(hfsplus_btree_node_desc));
	hfsplus_read_treeinfo(tree, head);
	kunmap(page);
	page_cache_release(page);
	return tree;

 fail_page:
	page_cache_release(page);
 free_tree:
	iput(tree->inode);
	kfree(tree);
	return NULL;
}

void hfsplus_write_btree(struct hfsplus_btree *tree)
{
	hfsplus_btree_head *head;
	hfsplus_bnode *node;
	struct page *page;

	node = hfsplus_find_bnode(tree, 0);
	if (!node)
		/* panic? */
		return;
	/* Load the header */
	page = node->page[0];
	head = (hfsplus_btree_head *)(kmap(page) + sizeof(hfsplus_btree_node_desc));
	hfsplus_write_treeinfo(tree, head);
	kunmap(page);
	set_page_dirty(page);
	hfsplus_put_bnode(node);
}

hfsplus_bnode *hfsplus_btree_alloc_node(hfsplus_btree *tree)
{
	hfsplus_bnode *node;
	struct page **pagep;
	u32 nidx;
	u16 idx, off, len;
	u8 *data, byte, m;
	int i;

	while (!tree->free_nodes) {
		loff_t size;
		int res;

		res = hfsplus_extend_file(tree->inode);
		if (res)
			return ERR_PTR(res);
		HFSPLUS_I(tree->inode).total_blocks = HFSPLUS_I(tree->inode).alloc_blocks;
		size = HFSPLUS_I(tree->inode).total_blocks;
		size <<= tree->sb->s_blocksize_bits;
		tree->inode->i_size = size;
		do_div(size, (u32)tree->node_size);
		tree->free_nodes = (u32)size - tree->node_count;
		tree->node_count = size;
	}

	nidx = 0;
	node = hfsplus_find_bnode(tree, nidx);
	len = hfsplus_brec_lenoff(node, 2, &off);

	off += node->page_offset;
	pagep = node->page + (off >> PAGE_CACHE_SHIFT);
	data = hfsplus_kmap(*pagep);
	off &= ~PAGE_CACHE_MASK;
	idx = 0;

	for (;;) {
		while (len) {
			byte = data[off];
			if (byte != 0xff) {
				for (m = 0x80, i = 0; i < 8; m >>= 1, i++) {
					if (!(byte & m)) {
						idx += i;
						data[off] |= m;
						set_page_dirty(*pagep);
						hfsplus_kunmap(*pagep);
						tree->free_nodes--;
						mark_inode_dirty(tree->inode);
						hfsplus_put_bnode(node);
						return hfsplus_create_bnode(tree, idx);
					}
				}
			}
			if (++off >= PAGE_CACHE_SIZE) {
				hfsplus_kunmap(*pagep++);
				data = hfsplus_kmap(*pagep);
				off = 0;
			}
			idx += 8;
			len--;
		}
		nidx = node->next;
		hfsplus_put_bnode(node);
		if (!nidx) {
			printk("need new bmap node...\n");
			hfsplus_kunmap(*pagep);
			return ERR_PTR(-ENOSPC);
		}
		node = hfsplus_find_bnode(tree, nidx);
		len = hfsplus_brec_lenoff(node, 0, &off);

		off += node->page_offset;
		pagep = node->page + (off >> PAGE_CACHE_SHIFT);
		data = hfsplus_kmap(*pagep);
		off &= ~PAGE_CACHE_MASK;
	}
}

void hfsplus_btree_remove_node(hfsplus_bnode *node)
{
	hfsplus_btree *tree;
	hfsplus_bnode *tmp;
	u32 cnid;

	tree = node->tree;
	if (node->prev) {
		tmp = hfsplus_find_bnode(tree, node->prev);
		tmp->next = node->next;
		cnid = cpu_to_be32(tmp->next);
		hfsplus_bnode_writebytes(tmp, &cnid, offsetof(hfsplus_btree_node_desc, next), 4);
		hfsplus_put_bnode(tmp);
	} else if (node->kind == HFSPLUS_NODE_LEAF)
		tree->leaf_head = node->next;

	if (node->next) {
		tmp = hfsplus_find_bnode(tree, node->next);
		tmp->prev = node->prev;
		cnid = cpu_to_be32(tmp->prev);
		hfsplus_bnode_writebytes(tmp, &cnid, offsetof(hfsplus_btree_node_desc, prev), 4);
		hfsplus_put_bnode(tmp);
	} else if (node->kind == HFSPLUS_NODE_LEAF)
		tree->leaf_tail = node->prev;

	// move down?
	if (!node->prev && !node->next) {
		printk("hfsplus_btree_del_level\n");
	}
	if (!node->parent) {
		tree->root = 0;
		tree->depth = 0;
	}
	set_bit(HFSPLUS_BNODE_DELETED, &node->flags);
}

void hfsplus_btree_free_node(hfsplus_bnode *node)
{
	hfsplus_btree *tree;
	struct page *page;
	u16 off, len;
	u32 nidx;
	u8 *data, byte, m;

	dprint(DBG_BNODE_MOD, "btree_free_node: %u\n", node->this);
	tree = node->tree;
	nidx = node->this;
	node = hfsplus_find_bnode(tree, 0);
	len = hfsplus_brec_lenoff(node, 2, &off);
	while (nidx >= len * 8) {
		u32 i;

		nidx -= len * 8;
		i = node->next;
		hfsplus_put_bnode(node);
		if (!nidx)
			/* panic */;
		node = hfsplus_find_bnode(tree, nidx);
		if (node->kind != HFSPLUS_NODE_MAP)
			/* panic */;
		len = hfsplus_brec_lenoff(node, 0, &off);
	}
	off += node->page_offset + nidx / 8;
	page = node->page[off >> PAGE_CACHE_SHIFT];
	data = hfsplus_kmap(page);
	off &= ~PAGE_CACHE_MASK;
	m = 1 << (~nidx & 7);
	byte = data[off];
	if (!(byte & m)) {
		BUG();
		/* panic */
		hfsplus_kunmap(page);
		hfsplus_put_bnode(node);
		return;
	}
	data[off] = byte & ~m;
	set_page_dirty(page);
	hfsplus_kunmap(page);
	hfsplus_put_bnode(node);
	tree->free_nodes++;
	mark_inode_dirty(tree->inode);
}
