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
 * $Id: jffs2.h,v 1.19 2001/10/09 13:20:23 dwmw2 Exp $
 *
 */

#ifndef __LINUX_JFFS2_H__
#define __LINUX_JFFS2_H__

#include <asm/types.h>
#define JFFS2_SUPER_MAGIC 0x72b6

/* Values we may expect to find in the 'magic' field */
#define JFFS2_OLD_MAGIC_BITMASK 0x1984
#define JFFS2_MAGIC_BITMASK 0x1985
#define KSAMTIB_CIGAM_2SFFJ 0x5981 /* For detecting wrong-endian fs */
#define JFFS2_EMPTY_BITMASK 0xffff
#define JFFS2_DIRTY_BITMASK 0x0000

/* We only allow a single char for length, and 0xFF is empty flash so
   we don't want it confused with a real length. Hence max 254.
*/
#define JFFS2_MAX_NAME_LEN 254

/* How small can we sensibly write nodes? */
#define JFFS2_MIN_DATA_LEN 128

#define JFFS2_COMPR_NONE	0x00
#define JFFS2_COMPR_ZERO	0x01
#define JFFS2_COMPR_RTIME	0x02
#define JFFS2_COMPR_RUBINMIPS	0x03
#define JFFS2_COMPR_COPY	0x04
#define JFFS2_COMPR_DYNRUBIN	0x05
#define JFFS2_COMPR_ZLIB	0x06
/* Compatibility flags. */
#define JFFS2_COMPAT_MASK 0xc000      /* What do to if an unknown nodetype is found */
#define JFFS2_NODE_ACCURATE 0x2000
/* INCOMPAT: Fail to mount the filesystem */
#define JFFS2_FEATURE_INCOMPAT 0xc000
/* ROCOMPAT: Mount read-only */
#define JFFS2_FEATURE_ROCOMPAT 0x8000
/* RWCOMPAT_COPY: Mount read/write, and copy the node when it's GC'd */
#define JFFS2_FEATURE_RWCOMPAT_COPY 0x4000
/* RWCOMPAT_DELETE: Mount read/write, and delete the node when it's GC'd */
#define JFFS2_FEATURE_RWCOMPAT_DELETE 0x0000

#define JFFS2_NODETYPE_DIRENT (JFFS2_FEATURE_INCOMPAT | JFFS2_NODE_ACCURATE | 1)
#define JFFS2_NODETYPE_INODE (JFFS2_FEATURE_INCOMPAT | JFFS2_NODE_ACCURATE | 2)
#define JFFS2_NODETYPE_CLEANMARKER (JFFS2_FEATURE_RWCOMPAT_DELETE | JFFS2_NODE_ACCURATE | 3)

// Maybe later...
//#define JFFS2_NODETYPE_CHECKPOINT (JFFS2_FEATURE_RWCOMPAT_DELETE | JFFS2_NODE_ACCURATE | 3)
//#define JFFS2_NODETYPE_OPTIONS (JFFS2_FEATURE_RWCOMPAT_COPY | JFFS2_NODE_ACCURATE | 4)

/* Same as the non_ECC versions, but with extra space for real 
 * ECC instead of just the checksum. For use on NAND flash 
 */
//#define JFFS2_NODETYPE_DIRENT_ECC (JFFS2_FEATURE_INCOMPAT | JFFS2_NODE_ACCURATE | 5)
//#define JFFS2_NODETYPE_INODE_ECC (JFFS2_FEATURE_INCOMPAT | JFFS2_NODE_ACCURATE | 6)

#define JFFS2_INO_FLAG_PREREAD	  1	/* Do read_inode() for this one at 
					   mount time, don't wait for it to 
					   happen later */
#define JFFS2_INO_FLAG_USERCOMPR  2	/* User has requested a specific 
					   compression type */


struct jffs2_unknown_node
{
	/* All start like this */
	__u16 magic;
	__u16 nodetype;
	__u32 totlen; /* So we can skip over nodes we don't grok */
	__u32 hdr_crc;
} __attribute__((packed));

struct jffs2_raw_dirent
{
	__u16 magic;
	__u16 nodetype;	/* == JFFS_NODETYPE_DIRENT */
	__u32 totlen;
	__u32 hdr_crc;
	__u32 pino;
	__u32 version;
	__u32 ino; /* == zero for unlink */
	__u32 mctime;
	__u8 nsize;
	__u8 type;
	__u8 unused[2];
	__u32 node_crc;
	__u32 name_crc;
	__u8 name[0];
} __attribute__((packed));

/* The JFFS2 raw inode structure: Used for storage on physical media.  */
/* The uid, gid, atime, mtime and ctime members could be longer, but 
   are left like this for space efficiency. If and when people decide
   they really need them extended, it's simple enough to add support for
   a new type of raw node.
*/
struct jffs2_raw_inode
{
	__u16 magic;      /* A constant magic number.  */
	__u16 nodetype;   /* == JFFS_NODETYPE_INODE */
	__u32 totlen;     /* Total length of this node (inc data, etc.) */
	__u32 hdr_crc;
	__u32 ino;        /* Inode number.  */
	__u32 version;    /* Version number.  */
	__u32 mode;       /* The file's type or mode.  */
	__u16 uid;        /* The file's owner.  */
	__u16 gid;        /* The file's group.  */
	__u32 isize;      /* Total resultant size of this inode (used for truncations)  */
	__u32 atime;      /* Last access time.  */
	__u32 mtime;      /* Last modification time.  */
	__u32 ctime;      /* Change time.  */
	__u32 offset;     /* Where to begin to write.  */
	__u32 csize;      /* (Compressed) data size */
	__u32 dsize;	  /* Size of the node's data. (after decompression) */
	__u8 compr;       /* Compression algorithm used */
	__u8 usercompr;	  /* Compression algorithm requested by the user */
	__u16 flags;	  /* See JFFS2_INO_FLAG_* */
	__u32 data_crc;   /* CRC for the (compressed) data.  */
	__u32 node_crc;   /* CRC for the raw inode (excluding data)  */
//	__u8 data[dsize];
} __attribute__((packed));

union jffs2_node_union {
	struct jffs2_raw_inode i;
	struct jffs2_raw_dirent d;
	struct jffs2_unknown_node u;
};

#endif /* __LINUX_JFFS2_H__ */
