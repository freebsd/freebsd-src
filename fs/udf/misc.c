/*
 * misc.c
 *
 * PURPOSE
 *	Miscellaneous routines for the OSTA-UDF(tm) filesystem.
 *
 * CONTACTS
 *	E-mail regarding any portion of the Linux UDF file system should be
 *	directed to the development team mailing list (run by majordomo):
 *		linux_udf@hpesjro.fc.hp.com
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL). Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 *
 *  (C) 1998 Dave Boynton
 *  (C) 1998-2001 Ben Fennema
 *  (C) 1999-2000 Stelias Computing Inc
 *
 * HISTORY
 *
 *  04/19/99 blf  partial support for reading/writing specific EA's
 */

#include "udfdecl.h"

#include <linux/fs.h>
#include <linux/string.h>
#include <linux/udf_fs.h>

#include "udf_i.h"
#include "udf_sb.h"

uint32_t
udf64_low32(uint64_t indat)
{
	return indat & 0x00000000FFFFFFFFULL;
}

uint32_t
udf64_high32(uint64_t indat)
{
	return indat >> 32;
}

extern struct buffer_head *
udf_tgetblk(struct super_block *sb, int block)
{
	if (UDF_QUERY_FLAG(sb, UDF_FLAG_VARCONV))
		return sb_getblk(sb, udf_fixed_to_variable(block));
	else
		return sb_getblk(sb, block);
}

extern struct buffer_head *
udf_tread(struct super_block *sb, int block)
{
	if (UDF_QUERY_FLAG(sb, UDF_FLAG_VARCONV))
		return sb_bread(sb, udf_fixed_to_variable(block));
	else
		return sb_bread(sb, block);
}

extern struct genericFormat *
udf_add_extendedattr(struct inode * inode, uint32_t size, uint32_t type,
	uint8_t loc, struct buffer_head **bh)
{
	uint8_t *ea = NULL, *ad = NULL;
	long_ad eaicb;
	int offset;

	*bh = udf_tread(inode->i_sb, inode->i_ino);

	if (UDF_I_EXTENDED_FE(inode) == 0)
	{
		struct fileEntry *fe;

		fe = (struct fileEntry *)(*bh)->b_data;
		eaicb = lela_to_cpu(fe->extendedAttrICB);
		offset = sizeof(struct fileEntry);
	}
	else
	{
		struct extendedFileEntry *efe;

		efe = (struct extendedFileEntry *)(*bh)->b_data;
		eaicb = lela_to_cpu(efe->extendedAttrICB);
		offset = sizeof(struct extendedFileEntry);
	}

	ea = &(*bh)->b_data[offset];
	if (UDF_I_LENEATTR(inode))
		offset += UDF_I_LENEATTR(inode);
	else
		size += sizeof(struct extendedAttrHeaderDesc);

	ad = &(*bh)->b_data[offset];
	if (UDF_I_LENALLOC(inode))
		offset += UDF_I_LENALLOC(inode);

	offset = inode->i_sb->s_blocksize - offset;

	/* TODO - Check for FreeEASpace */

	if (loc & 0x01 && offset >= size)
	{
		struct extendedAttrHeaderDesc *eahd;
		eahd = (struct extendedAttrHeaderDesc *)ea;

		if (UDF_I_LENALLOC(inode))
		{
			memmove(&ad[size], ad, UDF_I_LENALLOC(inode));
		}

		if (UDF_I_LENEATTR(inode))
		{
			/* check checksum/crc */
			if (le16_to_cpu(eahd->descTag.tagIdent) != TAG_IDENT_EAHD ||
				le32_to_cpu(eahd->descTag.tagLocation) != UDF_I_LOCATION(inode).logicalBlockNum)
			{
				udf_release_data(*bh);
				return NULL;
			}
		}
		else
		{
			size -= sizeof(struct extendedAttrHeaderDesc);
			UDF_I_LENEATTR(inode) += sizeof(struct extendedAttrHeaderDesc);
			eahd->descTag.tagIdent = cpu_to_le16(TAG_IDENT_EAHD);
			eahd->descTag.descVersion = cpu_to_le16(2);
			eahd->descTag.tagSerialNum = cpu_to_le16(1);
			eahd->descTag.tagLocation = cpu_to_le32(UDF_I_LOCATION(inode).logicalBlockNum);
			eahd->impAttrLocation = cpu_to_le32(0xFFFFFFFF);
			eahd->appAttrLocation = cpu_to_le32(0xFFFFFFFF);
		}

		offset = UDF_I_LENEATTR(inode);
		if (type < 2048)
		{
			if (le32_to_cpu(eahd->appAttrLocation) < UDF_I_LENEATTR(inode))
			{
				uint32_t aal = le32_to_cpu(eahd->appAttrLocation);
				memmove(&ea[offset - aal + size],
					&ea[aal], offset - aal);
				offset -= aal;
				eahd->appAttrLocation = cpu_to_le32(aal + size);
			}
			if (le32_to_cpu(eahd->impAttrLocation) < UDF_I_LENEATTR(inode))
			{
				uint32_t ial = le32_to_cpu(eahd->impAttrLocation);
				memmove(&ea[offset - ial + size],
					&ea[ial], offset - ial);
				offset -= ial;
				eahd->impAttrLocation = cpu_to_le32(ial + size);
			}
		}
		else if (type < 65536)
		{
			if (le32_to_cpu(eahd->appAttrLocation) < UDF_I_LENEATTR(inode))
			{
				uint32_t aal = le32_to_cpu(eahd->appAttrLocation);
				memmove(&ea[offset - aal + size],
					&ea[aal], offset - aal);
				offset -= aal;
				eahd->appAttrLocation = cpu_to_le32(aal + size);
			}
		}
		/* rewrite CRC + checksum of eahd */
		UDF_I_LENEATTR(inode) += size;
		return (struct genericFormat *)&ea[offset];
	}
	if (loc & 0x02)
	{
	}
	udf_release_data(*bh);
	return NULL;
}

extern struct genericFormat *
udf_get_extendedattr(struct inode * inode, uint32_t type, uint8_t subtype,
	struct buffer_head **bh)
{
	struct genericFormat *gaf;
	uint8_t *ea = NULL;
	long_ad eaicb;
	uint32_t offset;

	*bh = udf_tread(inode->i_sb, inode->i_ino);

	if (UDF_I_EXTENDED_FE(inode) == 0)
	{
		struct fileEntry *fe;

		fe = (struct fileEntry *)(*bh)->b_data;
		eaicb = lela_to_cpu(fe->extendedAttrICB);
		if (UDF_I_LENEATTR(inode))
			ea = fe->extendedAttr;
	}
	else
	{
		struct extendedFileEntry *efe;

		efe = (struct extendedFileEntry *)(*bh)->b_data;
		eaicb = lela_to_cpu(efe->extendedAttrICB);
		if (UDF_I_LENEATTR(inode))
			ea = efe->extendedAttr;
	}

	if (UDF_I_LENEATTR(inode))
	{
		struct extendedAttrHeaderDesc *eahd;
		eahd = (struct extendedAttrHeaderDesc *)ea;

		/* check checksum/crc */
		if (le16_to_cpu(eahd->descTag.tagIdent) != TAG_IDENT_EAHD ||
			le32_to_cpu(eahd->descTag.tagLocation) != UDF_I_LOCATION(inode).logicalBlockNum)
		{
			udf_release_data(*bh);
			return NULL;
		}
	
		if (type < 2048)
			offset = sizeof(struct extendedAttrHeaderDesc);
		else if (type < 65536)
			offset = le32_to_cpu(eahd->impAttrLocation);
		else
			offset = le32_to_cpu(eahd->appAttrLocation);

		while (offset < UDF_I_LENEATTR(inode))
		{
			gaf = (struct genericFormat *)&ea[offset];
			if (le32_to_cpu(gaf->attrType) == type && gaf->attrSubtype == subtype)
				return gaf;
			else
				offset += le32_to_cpu(gaf->attrLength);
		}
	}

	udf_release_data(*bh);
	if (eaicb.extLength)
	{
		/* TODO */
	}
	return NULL;
}

/*
 * udf_read_tagged
 *
 * PURPOSE
 *	Read the first block of a tagged descriptor.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
extern struct buffer_head *
udf_read_tagged(struct super_block *sb, uint32_t block, uint32_t location, uint16_t *ident)
{
	tag *tag_p;
	struct buffer_head *bh = NULL;
	register uint8_t checksum;
	register int i;

	/* Read the block */
	if (block == 0xFFFFFFFF)
		return NULL;

	bh = udf_tread(sb, block);
	if (!bh)
	{
		udf_debug("block=%d, location=%d: read failed\n", block, location);
		return NULL;
	}

	tag_p = (tag *)(bh->b_data);

	*ident = le16_to_cpu(tag_p->tagIdent);

	if ( location != le32_to_cpu(tag_p->tagLocation) )
	{
		udf_debug("location mismatch block %u, tag %u != %u\n",
			block, le32_to_cpu(tag_p->tagLocation), location);
		goto error_out;
	}
	
	/* Verify the tag checksum */
	checksum = 0U;
	for (i = 0; i < 4; i++)
		checksum += (uint8_t)(bh->b_data[i]);
	for (i = 5; i < 16; i++)
		checksum += (uint8_t)(bh->b_data[i]);
	if (checksum != tag_p->tagChecksum) {
		printk(KERN_ERR "udf: tag checksum failed block %d\n", block);
		goto error_out;
	}

	/* Verify the tag version */
	if (le16_to_cpu(tag_p->descVersion) != 0x0002U &&
		le16_to_cpu(tag_p->descVersion) != 0x0003U)
	{
		udf_debug("tag version 0x%04x != 0x0002 || 0x0003 block %d\n",
			le16_to_cpu(tag_p->descVersion), block);
		goto error_out;
	}

	/* Verify the descriptor CRC */
	if (le16_to_cpu(tag_p->descCRCLength) + sizeof(tag) > sb->s_blocksize ||
		le16_to_cpu(tag_p->descCRC) == udf_crc(bh->b_data + sizeof(tag),
			le16_to_cpu(tag_p->descCRCLength), 0))
	{
		return bh;
	}
	udf_debug("Crc failure block %d: crc = %d, crclen = %d\n",
		block, le16_to_cpu(tag_p->descCRC), le16_to_cpu(tag_p->descCRCLength));

error_out:
	brelse(bh);
	return NULL;
}

extern struct buffer_head *
udf_read_ptagged(struct super_block *sb, lb_addr loc, uint32_t offset, uint16_t *ident)
{
	return udf_read_tagged(sb, udf_get_lb_pblock(sb, loc, offset),
		loc.logicalBlockNum + offset, ident);
}

void udf_release_data(struct buffer_head *bh)
{
	if (bh)
		brelse(bh);
}

void udf_update_tag(char *data, int length)
{
	tag *tptr = (tag *)data;
	int i;

	length -= sizeof(tag);

	tptr->tagChecksum = 0;
	tptr->descCRCLength = le16_to_cpu(length);
	tptr->descCRC = le16_to_cpu(udf_crc(data + sizeof(tag), length, 0));

	for (i=0; i<16; i++)
		if (i != 4)
			tptr->tagChecksum += (uint8_t)(data[i]);
}

void udf_new_tag(char *data, uint16_t ident, uint16_t version, uint16_t snum,
	uint32_t loc, int length)
{
	tag *tptr = (tag *)data;
	tptr->tagIdent = le16_to_cpu(ident);
	tptr->descVersion = le16_to_cpu(version);
	tptr->tagSerialNum = le16_to_cpu(snum);
	tptr->tagLocation = le32_to_cpu(loc);
	udf_update_tag(data, length);
}
