/*-
 * Copyright (c) 2017, Fedor Uporov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/endian.h>
#include <sys/conf.h>
#include <sys/extattr.h>

#include <fs/ext2fs/fs.h>
#include <fs/ext2fs/ext2fs.h>
#include <fs/ext2fs/inode.h>
#include <fs/ext2fs/ext2_dinode.h>
#include <fs/ext2fs/ext2_mount.h>
#include <fs/ext2fs/ext2_extattr.h>


static int
ext2_extattr_index_to_bsd(int index)
{
	switch (index) {
		case EXT4_XATTR_INDEX_USER:
			return EXTATTR_NAMESPACE_USER;

		case EXT4_XATTR_INDEX_SYSTEM:
			return EXTATTR_NAMESPACE_SYSTEM;

		default:
			return EXTATTR_NAMESPACE_EMPTY;
	}
}

int
ext2_extattr_inode_list(struct inode *ip, int attrnamespace,
    struct uio *uio, size_t *size)
{
	struct m_ext2fs *fs;
	struct buf *bp;
	struct ext2fs_extattr_dinode_header *header;
	struct ext2fs_extattr_entry *entry;
	struct ext2fs_extattr_entry *next;
	char *end;
	int error;

	fs = ip->i_e2fs;

	if ((error = bread(ip->i_devvp,
	    fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
	    (int)fs->e2fs_bsize, NOCRED, &bp)) != 0) {
		brelse(bp);
		return (error);
	}

	struct ext2fs_dinode *dinode = (struct ext2fs_dinode *)
	    ((char *)bp->b_data +
	    EXT2_INODE_SIZE(fs) * ino_to_fsbo(fs, ip->i_number));

	/* Check attributes magic value */
	header = (struct ext2fs_extattr_dinode_header *)((char *)dinode +
	    E2FS_REV0_INODE_SIZE + dinode->e2di_extra_isize);

	if (header->h_magic != EXTATTR_MAGIC) {
		brelse(bp);
		return (0);
	}

	/* Check attributes integrity */
	entry = EXT2_IFIRST(header);
	end = (char *)dinode + EXT2_INODE_SIZE(fs);
	while (!EXT2_IS_LAST_ENTRY(entry)) {
		next = EXT2_EXTATTR_NEXT(entry);
		if ((char *)next >= end) {
			brelse(bp);
			return (EIO);
		}

		entry = next;
	}

	for (entry = EXT2_IFIRST(header); !EXT2_IS_LAST_ENTRY(entry);
		entry = EXT2_EXTATTR_NEXT(entry)) {
		if (ext2_extattr_index_to_bsd(entry->e_name_index) != attrnamespace)
			continue;

		if (uio == NULL)
			*size += entry->e_name_len + 1;
		else {
			char *attr_name = malloc(entry->e_name_len + 1, M_TEMP, M_WAITOK);
			attr_name[0] = entry->e_name_len;
			memcpy(&attr_name[1], entry->e_name, entry->e_name_len);
			error = uiomove(attr_name, entry->e_name_len + 1, uio);
			free(attr_name, M_TEMP);
		}
	}

	brelse(bp);

	return (0);
}

int
ext2_extattr_block_list(struct inode *ip, int attrnamespace,
    struct uio *uio, size_t *size)
{
	struct m_ext2fs *fs;
	struct buf *bp;
	struct ext2fs_extattr_header *header;
	struct ext2fs_extattr_entry *entry;
	struct ext2fs_extattr_entry *next;
	char *end;
	int error;

	fs = ip->i_e2fs;

	error = bread(ip->i_devvp, fsbtodb(fs, ip->i_facl),
	    fs->e2fs_bsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (error);
	}

	/* Check attributes magic value */
	header = EXT2_HDR(bp);
	if (header->h_magic != EXTATTR_MAGIC || header->h_blocks != 1) {
		brelse(bp);
		return (EINVAL);
	}

	/* Check attributes integrity */
	end = bp->b_data + bp->b_bufsize;
	entry = EXT2_FIRST_ENTRY(bp);
	while (!EXT2_IS_LAST_ENTRY(entry)) {
		next = EXT2_EXTATTR_NEXT(entry);
		if ((char *)next >= end) {
			brelse(bp);
			return (EIO);
		}

		entry = next;
	}

	for (entry = EXT2_FIRST_ENTRY(bp); !EXT2_IS_LAST_ENTRY(entry);
	    entry = EXT2_EXTATTR_NEXT(entry)) {
		if (ext2_extattr_index_to_bsd(entry->e_name_index) != attrnamespace)
			continue;

		if (uio == NULL)
			*size += entry->e_name_len + 1;
		else {
			char *attr_name = malloc(entry->e_name_len + 1, M_TEMP, M_WAITOK);
			attr_name[0] = entry->e_name_len;
			memcpy(&attr_name[1], entry->e_name, entry->e_name_len);
			error = uiomove(attr_name, entry->e_name_len + 1, uio);
			free(attr_name, M_TEMP);
		}
	}

	brelse(bp);

	return (0);
}

int
ext2_extattr_inode_get(struct inode *ip, int attrnamespace,
    const char *name, struct uio *uio, size_t *size)
{
	struct m_ext2fs *fs;
	struct buf *bp;
	struct ext2fs_extattr_dinode_header *header;
	struct ext2fs_extattr_entry *entry;
	struct ext2fs_extattr_entry *next;
	char *end;
	int error;

	fs = ip->i_e2fs;

	if ((error = bread(ip->i_devvp,
	    fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
	    (int)fs->e2fs_bsize, NOCRED, &bp)) != 0) {
		brelse(bp);
		return (error);
	}

	struct ext2fs_dinode *dinode = (struct ext2fs_dinode *)
	    ((char *)bp->b_data +
	    EXT2_INODE_SIZE(fs) * ino_to_fsbo(fs, ip->i_number));

	/* Check attributes magic value */
	header = (struct ext2fs_extattr_dinode_header *)((char *)dinode +
	    E2FS_REV0_INODE_SIZE + dinode->e2di_extra_isize);

	if (header->h_magic != EXTATTR_MAGIC) {
		brelse(bp);
		return (0);
	}

	/* Check attributes integrity */
	entry = EXT2_IFIRST(header);
	end = (char *)dinode + EXT2_INODE_SIZE(fs);
	while (!EXT2_IS_LAST_ENTRY(entry)) {
		next = EXT2_EXTATTR_NEXT(entry);
		if ((char *)next >= end) {
			brelse(bp);
			return (EIO);
		}

		entry = next;
	}

	for (entry = EXT2_IFIRST(header); !EXT2_IS_LAST_ENTRY(entry);
	    entry = EXT2_EXTATTR_NEXT(entry)) {
		if (ext2_extattr_index_to_bsd(entry->e_name_index) != attrnamespace)
			continue;

		if (strlen(name) == entry->e_name_len &&
		    0 == strncmp(entry->e_name, name, entry->e_name_len)) {
			if (uio == NULL)
				*size += entry->e_value_size;
			else {
				error = uiomove(((char *)EXT2_IFIRST(header)) + entry->e_value_offs,
				    entry->e_value_size, uio);
				if (error) {
					brelse(bp);
					return (error);
				}
			}
		}
	 }

	brelse(bp);

	return (0);
}

int
ext2_extattr_block_get(struct inode *ip, int attrnamespace,
    const char *name, struct uio *uio, size_t *size)
{
	struct m_ext2fs *fs;
	struct buf *bp;
	struct ext2fs_extattr_header *header;
	struct ext2fs_extattr_entry *entry;
	struct ext2fs_extattr_entry *next;
	char *end;
	int error;

	fs = ip->i_e2fs;

	error = bread(ip->i_devvp, fsbtodb(fs, ip->i_facl),
	    fs->e2fs_bsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (error);
	}

	/* Check attributes magic value */
	header = EXT2_HDR(bp);
	if (header->h_magic != EXTATTR_MAGIC || header->h_blocks != 1) {
		brelse(bp);
		return (EINVAL);
	}

	/* Check attributes integrity */
	end = bp->b_data + bp->b_bufsize;
	entry = EXT2_FIRST_ENTRY(bp);
	while (!EXT2_IS_LAST_ENTRY(entry)) {
		next = EXT2_EXTATTR_NEXT(entry);
		if ((char *)next >= end) {
			brelse(bp);
			return (EIO);
		}

		entry = next;
	}

	for (entry = EXT2_FIRST_ENTRY(bp); !EXT2_IS_LAST_ENTRY(entry);
	    entry = EXT2_EXTATTR_NEXT(entry)) {
		if (ext2_extattr_index_to_bsd(entry->e_name_index) != attrnamespace)
			continue;

		if (strlen(name) == entry->e_name_len &&
		    0 == strncmp(entry->e_name, name, entry->e_name_len)) {
			if (uio == NULL)
				*size += entry->e_value_size;
			else {
				error = uiomove(bp->b_data + entry->e_value_offs,
				    entry->e_value_size, uio);
				if (error) {
					brelse(bp);
					return (error);
				}
			}
		}
	 }

	brelse(bp);

	return (0);
}
