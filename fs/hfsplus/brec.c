/*
 *  linux/fs/hfsplus/brec.c
 *
 * Copyright (C) 2001
 * Brad Boyer (flar@allandria.com)
 * (C) 2003 Ardis Technologies <roman@ardistech.com>
 *
 * Handle individual btree records
 */

#include "hfsplus_fs.h"
#include "hfsplus_raw.h"

/* Get the length and offset of the given record in the given node */
u16 hfsplus_brec_lenoff(hfsplus_bnode *node, u16 rec, u16 *off)
{
	u16 retval[2];
	u16 dataoff;

	dataoff = node->tree->node_size - (rec + 2) * 2;
	hfsplus_bnode_readbytes(node, retval, dataoff, 4);
	*off = be16_to_cpu(retval[1]);
	return be16_to_cpu(retval[0]) - *off;
}

/* Get the length of the key from a keyed record */
u16 hfsplus_brec_keylen(hfsplus_bnode *node, u16 rec)
{
	u16 klsz, retval, recoff;
	unsigned char buf[2];

	if ((node->kind != HFSPLUS_NODE_NDX)&&(node->kind != HFSPLUS_NODE_LEAF))
		return 0;

	klsz = (node->tree->attributes & HFSPLUS_TREE_BIGKEYS) ? 2 : 1;
	if ((node->kind == HFSPLUS_NODE_NDX) &&
	   !(node->tree->attributes & HFSPLUS_TREE_VAR_NDXKEY_SIZE)) {
		retval = node->tree->max_key_len;
	} else {
		recoff = hfsplus_bnode_read_u16(node, node->tree->node_size - (rec + 1) * 2);
		if (!recoff)
			return 0;
		hfsplus_bnode_readbytes(node, buf, recoff, klsz);
		if (klsz == 1)
			retval = buf[0];
		else
			retval = be16_to_cpu(*(u16 *)buf);
	}
	return (retval + klsz + 1) & 0xFFFE;
}
