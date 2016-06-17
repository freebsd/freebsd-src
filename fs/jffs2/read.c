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
 * $Id: read.c,v 1.13.2.2 2003/11/02 13:51:18 dwmw2 Exp $
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/jffs2.h>
#include <linux/mtd/mtd.h>
#include "nodelist.h"
#include <linux/crc32.h>

int jffs2_read_dnode(struct jffs2_sb_info *c, struct jffs2_full_dnode *fd, unsigned char *buf, int ofs, int len)
{
	struct jffs2_raw_inode *ri;
	size_t readlen;
	__u32 crc;
	unsigned char *decomprbuf = NULL;
	unsigned char *readbuf = NULL;
	int ret = 0;

	ri = jffs2_alloc_raw_inode();
	if (!ri)
		return -ENOMEM;

	ret = c->mtd->read(c->mtd, fd->raw->flash_offset & ~3, sizeof(*ri), &readlen, (char *)ri);
	if (ret) {
		jffs2_free_raw_inode(ri);
		printk(KERN_WARNING "Error reading node from 0x%08x: %d\n", fd->raw->flash_offset & ~3, ret);
		return ret;
	}
	if (readlen != sizeof(*ri)) {
		jffs2_free_raw_inode(ri);
		printk(KERN_WARNING "Short read from 0x%08x: wanted 0x%x bytes, got 0x%x\n", 
		       fd->raw->flash_offset & ~3, sizeof(*ri), readlen);
		return -EIO;
	}
	crc = crc32(0, ri, sizeof(*ri)-8);

	D1(printk(KERN_DEBUG "Node read from %08x: node_crc %08x, calculated CRC %08x. dsize %x, csize %x, offset %x, buf %p\n", fd->raw->flash_offset & ~3, ri->node_crc, crc, ri->dsize, ri->csize, ri->offset, buf));
	if (crc != ri->node_crc) {
		printk(KERN_WARNING "Node CRC %08x != calculated CRC %08x for node at %08x\n", ri->node_crc, crc, fd->raw->flash_offset & ~3);
		ret = -EIO;
		goto out_ri;
	}
	/* There was a bug where we wrote hole nodes out with csize/dsize
	   swapped. Deal with it */
	if (ri->compr == JFFS2_COMPR_ZERO && !ri->dsize && ri->csize) {
		ri->dsize = ri->csize;
		ri->csize = 0;
	}

	D1(if(ofs + len > ri->dsize) {
		printk(KERN_WARNING "jffs2_read_dnode() asked for %d bytes at %d from %d-byte node\n", len, ofs, ri->dsize);
		ret = -EINVAL;
		goto out_ri;
	});

	
	if (ri->compr == JFFS2_COMPR_ZERO) {
		memset(buf, 0, len);
		goto out_ri;
	}

	/* Cases:
	   Reading whole node and it's uncompressed - read directly to buffer provided, check CRC.
	   Reading whole node and it's compressed - read into comprbuf, check CRC and decompress to buffer provided 
	   Reading partial node and it's uncompressed - read into readbuf, check CRC, and copy 
	   Reading partial node and it's compressed - read into readbuf, check checksum, decompress to decomprbuf and copy
	*/
	if (ri->compr == JFFS2_COMPR_NONE && len == ri->dsize) {
		readbuf = buf;
	} else {
		readbuf = kmalloc(ri->csize, GFP_KERNEL);
		if (!readbuf) {
			ret = -ENOMEM;
			goto out_ri;
		}
	}
	if (ri->compr != JFFS2_COMPR_NONE) {
		if (len < ri->dsize) {
			decomprbuf = kmalloc(ri->dsize, GFP_KERNEL);
			if (!decomprbuf) {
				ret = -ENOMEM;
				goto out_readbuf;
			}
		} else {
			decomprbuf = buf;
		}
	} else {
		decomprbuf = readbuf;
	}

	D2(printk(KERN_DEBUG "Read %d bytes to %p\n", ri->csize, readbuf));
	ret = c->mtd->read(c->mtd, (fd->raw->flash_offset &~3) + sizeof(*ri), ri->csize, &readlen, readbuf);

	if (!ret && readlen != ri->csize)
		ret = -EIO;
	if (ret)
		goto out_decomprbuf;

	crc = crc32(0, readbuf, ri->csize);
	if (crc != ri->data_crc) {
		printk(KERN_WARNING "Data CRC %08x != calculated CRC %08x for node at %08x\n", ri->data_crc, crc, fd->raw->flash_offset & ~3);
		ret = -EIO;
		goto out_decomprbuf;
	}
	D2(printk(KERN_DEBUG "Data CRC matches calculated CRC %08x\n", crc));
	if (ri->compr != JFFS2_COMPR_NONE) {
		D2(printk(KERN_DEBUG "Decompress %d bytes from %p to %d bytes at %p\n", ri->csize, readbuf, ri->dsize, decomprbuf)); 
		ret = jffs2_decompress(ri->compr, readbuf, decomprbuf, ri->csize, ri->dsize);
		if (ret) {
			printk(KERN_WARNING "Error: jffs2_decompress returned %d\n", ret);
			goto out_decomprbuf;
		}
	}

	if (len < ri->dsize) {
		memcpy(buf, decomprbuf+ofs, len);
	}
 out_decomprbuf:
	if(decomprbuf != buf && decomprbuf != readbuf)
		kfree(decomprbuf);
 out_readbuf:
	if(readbuf != buf)
		kfree(readbuf);
 out_ri:
	jffs2_free_raw_inode(ri);

	return ret;
}
