/*
 * Copyright (c) 2000, 2002 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */
#ifndef __XFS_DINODE_H__
#define	__XFS_DINODE_H__

struct xfs_buf;
struct xfs_mount;

#define	XFS_DINODE_VERSION_1	1
#define	XFS_DINODE_VERSION_2	2
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DINODE_GOOD_VERSION)
int xfs_dinode_good_version(int v);
#define XFS_DINODE_GOOD_VERSION(v)	xfs_dinode_good_version(v)
#else
#define XFS_DINODE_GOOD_VERSION(v)	(((v) == XFS_DINODE_VERSION_1) || \
					 ((v) == XFS_DINODE_VERSION_2))
#endif
#define	XFS_DINODE_MAGIC	0x494e	/* 'IN' */

/*
 * Disk inode structure.
 * This is just the header; the inode is expanded to fill a variable size
 * with the last field expanding.  It is split into the core and "other"
 * because we only need the core part in the in-core inode.
 */
typedef struct xfs_timestamp {
	__int32_t	t_sec;		/* timestamp seconds */
	__int32_t	t_nsec;		/* timestamp nanoseconds */
} xfs_timestamp_t;

/*
 * Note: Coordinate changes to this structure with the XFS_DI_* #defines
 * below and the offsets table in xfs_ialloc_log_di().
 */
typedef struct xfs_dinode_core
{
	__uint16_t	di_magic;	/* inode magic # = XFS_DINODE_MAGIC */
	__uint16_t	di_mode;	/* mode and type of file */
	__int8_t	di_version;	/* inode version */
	__int8_t	di_format;	/* format of di_c data */
	__uint16_t	di_onlink;	/* old number of links to file */
	__uint32_t	di_uid;		/* owner's user id */
	__uint32_t	di_gid;		/* owner's group id */
	__uint32_t	di_nlink;	/* number of links to file */
	__uint16_t	di_projid;	/* owner's project id */
	__uint8_t	di_pad[8];	/* unused, zeroed space */
	__uint16_t	di_flushiter;	/* incremented on flush */
	xfs_timestamp_t	di_atime;	/* time last accessed */
	xfs_timestamp_t	di_mtime;	/* time last modified */
	xfs_timestamp_t	di_ctime;	/* time created/inode modified */
	xfs_fsize_t	di_size;	/* number of bytes in file */
	xfs_drfsbno_t	di_nblocks;	/* # of direct & btree blocks used */
	xfs_extlen_t	di_extsize;	/* basic/minimum extent size for file */
	xfs_extnum_t	di_nextents;	/* number of extents in data fork */
	xfs_aextnum_t	di_anextents;	/* number of extents in attribute fork*/
	__uint8_t	di_forkoff;	/* attr fork offs, <<3 for 64b align */
	__int8_t	di_aformat;	/* format of attr fork's data */
	__uint32_t	di_dmevmask;	/* DMIG event mask */
	__uint16_t	di_dmstate;	/* DMIG state info */
	__uint16_t	di_flags;	/* random flags, XFS_DIFLAG_... */
	__uint32_t	di_gen;		/* generation number */
} xfs_dinode_core_t;

#define DI_MAX_FLUSH 0xffff

typedef struct xfs_dinode
{
	xfs_dinode_core_t	di_core;
	/*
	 * In adding anything between the core and the union, be
	 * sure to update the macros like XFS_LITINO below and
	 * XFS_BMAP_RBLOCK_DSIZE in xfs_bmap_btree.h.
	 */
	xfs_agino_t		di_next_unlinked;/* agi unlinked list ptr */
	union {
		xfs_bmdr_block_t di_bmbt;	/* btree root block */
		xfs_bmbt_rec_32_t di_bmx[1];	/* extent list */
		xfs_dir_shortform_t di_dirsf;	/* shortform directory */
		xfs_dir2_sf_t	di_dir2sf;	/* shortform directory v2 */
		char		di_c[1];	/* local contents */
		xfs_dev_t	di_dev;		/* device for S_IFCHR/S_IFBLK */
		uuid_t		di_muuid;	/* mount point value */
		char		di_symlink[1];	/* local symbolic link */
	}		di_u;
	union {
		xfs_bmdr_block_t di_abmbt;	/* btree root block */
		xfs_bmbt_rec_32_t di_abmx[1];	/* extent list */
		xfs_attr_shortform_t di_attrsf;	/* shortform attribute list */
	}		di_a;
} xfs_dinode_t;

/*
 * The 32 bit link count in the inode theoretically maxes out at UINT_MAX.
 * Since the pathconf interface is signed, we use 2^31 - 1 instead.
 * The old inode format had a 16 bit link count, so its maximum is USHRT_MAX.
 */
#define	XFS_MAXLINK		((1U << 31) - 1U)
#define	XFS_MAXLINK_1		65535U

/*
 * Bit names for logging disk inodes only
 */
#define	XFS_DI_MAGIC		0x0000001
#define	XFS_DI_MODE		0x0000002
#define	XFS_DI_VERSION		0x0000004
#define	XFS_DI_FORMAT		0x0000008
#define	XFS_DI_ONLINK		0x0000010
#define	XFS_DI_UID		0x0000020
#define	XFS_DI_GID		0x0000040
#define	XFS_DI_NLINK		0x0000080
#define	XFS_DI_PROJID		0x0000100
#define	XFS_DI_PAD		0x0000200
#define	XFS_DI_ATIME		0x0000400
#define	XFS_DI_MTIME		0x0000800
#define	XFS_DI_CTIME		0x0001000
#define	XFS_DI_SIZE		0x0002000
#define	XFS_DI_NBLOCKS		0x0004000
#define	XFS_DI_EXTSIZE		0x0008000
#define	XFS_DI_NEXTENTS		0x0010000
#define	XFS_DI_NAEXTENTS	0x0020000
#define	XFS_DI_FORKOFF		0x0040000
#define	XFS_DI_AFORMAT		0x0080000
#define	XFS_DI_DMEVMASK		0x0100000
#define	XFS_DI_DMSTATE		0x0200000
#define	XFS_DI_FLAGS		0x0400000
#define	XFS_DI_GEN		0x0800000
#define	XFS_DI_NEXT_UNLINKED	0x1000000
#define	XFS_DI_U		0x2000000
#define	XFS_DI_A		0x4000000
#define	XFS_DI_NUM_BITS		27
#define	XFS_DI_ALL_BITS		((1 << XFS_DI_NUM_BITS) - 1)
#define	XFS_DI_CORE_BITS	(XFS_DI_ALL_BITS & ~(XFS_DI_U|XFS_DI_A))

/*
 * Values for di_format
 */
typedef enum xfs_dinode_fmt
{
	XFS_DINODE_FMT_DEV,		/* CHR, BLK: di_dev */
	XFS_DINODE_FMT_LOCAL,		/* DIR, REG: di_c */
					/* LNK: di_symlink */
	XFS_DINODE_FMT_EXTENTS,		/* DIR, REG, LNK: di_bmx */
	XFS_DINODE_FMT_BTREE,		/* DIR, REG, LNK: di_bmbt */
	XFS_DINODE_FMT_UUID		/* MNT: di_uuid */
} xfs_dinode_fmt_t;

/*
 * Inode minimum and maximum sizes.
 */
#define	XFS_DINODE_MIN_LOG	8
#define	XFS_DINODE_MAX_LOG	11
#define	XFS_DINODE_MIN_SIZE	(1 << XFS_DINODE_MIN_LOG)
#define	XFS_DINODE_MAX_SIZE	(1 << XFS_DINODE_MAX_LOG)

/*
 * Inode size for given fs.
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_LITINO)
int xfs_litino(struct xfs_mount *mp);
#define	XFS_LITINO(mp)		xfs_litino(mp)
#else
#define	XFS_LITINO(mp)	((mp)->m_litino)
#endif
#define	XFS_BROOT_SIZE_ADJ	\
	(sizeof(xfs_bmbt_block_t) - sizeof(xfs_bmdr_block_t))

/*
 * Fork identifiers.  Here so utilities can use them without including
 * xfs_inode.h.
 */
#define	XFS_DATA_FORK	0
#define	XFS_ATTR_FORK	1

/*
 * Inode data & attribute fork sizes, per inode.
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_CFORK_Q)
int xfs_cfork_q_arch(xfs_dinode_core_t *dcp, xfs_arch_t arch);
int xfs_cfork_q(xfs_dinode_core_t *dcp);
#define	XFS_CFORK_Q_ARCH(dcp,arch)          xfs_cfork_q_arch(dcp,arch)
#define	XFS_CFORK_Q(dcp)                    xfs_cfork_q(dcp)
#else
#define	XFS_CFORK_Q_ARCH(dcp,arch)	    (!INT_ISZERO((dcp)->di_forkoff, arch))
#define XFS_CFORK_Q(dcp)                    ((dcp)->di_forkoff != 0)

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_CFORK_BOFF)
int xfs_cfork_boff_arch(xfs_dinode_core_t *dcp, xfs_arch_t arch);
int xfs_cfork_boff(xfs_dinode_core_t *dcp);
#define	XFS_CFORK_BOFF_ARCH(dcp,arch)	    xfs_cfork_boff_arch(dcp,arch)
#define	XFS_CFORK_BOFF(dcp)	            xfs_cfork_boff(dcp)
#else
#define	XFS_CFORK_BOFF_ARCH(dcp,arch)	    ((int)(INT_GET((dcp)->di_forkoff, arch) << 3))
#define XFS_CFORK_BOFF(dcp)                 ((int)((dcp)->di_forkoff << 3))

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_CFORK_DSIZE)
int xfs_cfork_dsize_arch(xfs_dinode_core_t *dcp, struct xfs_mount *mp, xfs_arch_t arch);
int xfs_cfork_dsize(xfs_dinode_core_t *dcp, struct xfs_mount *mp);
#define	XFS_CFORK_DSIZE_ARCH(dcp,mp,arch)   xfs_cfork_dsize_arch(dcp,mp,arch)
#define	XFS_CFORK_DSIZE(dcp,mp)             xfs_cfork_dsize(dcp,mp)
#else
#define	XFS_CFORK_DSIZE_ARCH(dcp,mp,arch) \
	(XFS_CFORK_Q_ARCH(dcp, arch) ? XFS_CFORK_BOFF_ARCH(dcp, arch) : XFS_LITINO(mp))
#define XFS_CFORK_DSIZE(dcp,mp) \
	(XFS_CFORK_Q(dcp) ? XFS_CFORK_BOFF(dcp) : XFS_LITINO(mp))

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_CFORK_ASIZE)
int xfs_cfork_asize_arch(xfs_dinode_core_t *dcp, struct xfs_mount *mp, xfs_arch_t arch);
int xfs_cfork_asize(xfs_dinode_core_t *dcp, struct xfs_mount *mp);
#define	XFS_CFORK_ASIZE_ARCH(dcp,mp,arch)   xfs_cfork_asize_arch(dcp,mp,arch)
#define	XFS_CFORK_ASIZE(dcp,mp)             xfs_cfork_asize(dcp,mp)
#else
#define	XFS_CFORK_ASIZE_ARCH(dcp,mp,arch) \
	(XFS_CFORK_Q_ARCH(dcp, arch) ? XFS_LITINO(mp) - XFS_CFORK_BOFF_ARCH(dcp, arch) : 0)
#define XFS_CFORK_ASIZE(dcp,mp) \
	(XFS_CFORK_Q(dcp) ? XFS_LITINO(mp) - XFS_CFORK_BOFF(dcp) : 0)

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_CFORK_SIZE)
int xfs_cfork_size_arch(xfs_dinode_core_t *dcp, struct xfs_mount *mp, int w, xfs_arch_t arch);
int xfs_cfork_size(xfs_dinode_core_t *dcp, struct xfs_mount *mp, int w);
#define	XFS_CFORK_SIZE_ARCH(dcp,mp,w,arch)  xfs_cfork_size_arch(dcp,mp,w,arch)
#define	XFS_CFORK_SIZE(dcp,mp,w)            xfs_cfork_size(dcp,mp,w)
#else
#define	XFS_CFORK_SIZE_ARCH(dcp,mp,w,arch) \
	((w) == XFS_DATA_FORK ? \
		XFS_CFORK_DSIZE_ARCH(dcp, mp, arch) : XFS_CFORK_ASIZE_ARCH(dcp, mp, arch))
#define XFS_CFORK_SIZE(dcp,mp,w) \
	((w) == XFS_DATA_FORK ? \
		XFS_CFORK_DSIZE(dcp, mp) : XFS_CFORK_ASIZE(dcp, mp))

#endif

#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DFORK_DSIZE)
int xfs_dfork_dsize_arch(xfs_dinode_t *dip, struct xfs_mount *mp, xfs_arch_t arch);
int xfs_dfork_dsize(xfs_dinode_t *dip, struct xfs_mount *mp);
#define	XFS_DFORK_DSIZE_ARCH(dip,mp,arch)   xfs_dfork_dsize_arch(dip,mp,arch)
#define	XFS_DFORK_DSIZE(dip,mp)             xfs_dfork_dsize(dip,mp)
#else
#define	XFS_DFORK_DSIZE_ARCH(dip,mp,arch)   XFS_CFORK_DSIZE_ARCH(&(dip)->di_core, mp, arch)
#define XFS_DFORK_DSIZE(dip,mp)             XFS_DFORK_DSIZE_ARCH(dip,mp,ARCH_NOCONVERT)

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DFORK_ASIZE)
int xfs_dfork_asize_arch(xfs_dinode_t *dip, struct xfs_mount *mp, xfs_arch_t arch);
int xfs_dfork_asize(xfs_dinode_t *dip, struct xfs_mount *mp);
#define	XFS_DFORK_ASIZE_ARCH(dip,mp,arch)   xfs_dfork_asize_arch(dip,mp,arch)
#define	XFS_DFORK_ASIZE(dip,mp)             xfs_dfork_asize(dip,mp)
#else
#define	XFS_DFORK_ASIZE_ARCH(dip,mp,arch)   XFS_CFORK_ASIZE_ARCH(&(dip)->di_core, mp, arch)
#define XFS_DFORK_ASIZE(dip,mp)             XFS_DFORK_ASIZE_ARCH(dip,mp,ARCH_NOCONVERT)

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DFORK_SIZE)
int xfs_dfork_size_arch(xfs_dinode_t *dip, struct xfs_mount *mp, int w, xfs_arch_t arch);
int xfs_dfork_size(xfs_dinode_t *dip, struct xfs_mount *mp, int w);
#define	XFS_DFORK_SIZE_ARCH(dip,mp,w,arch)  xfs_dfork_size_arch(dip,mp,w,arch)
#define	XFS_DFORK_SIZE(dip,mp,w)            xfs_dfork_size(dip,mp,w)
#else
#define	XFS_DFORK_SIZE_ARCH(dip,mp,w,arch)  XFS_CFORK_SIZE_ARCH(&(dip)->di_core, mp, w, arch)
#define XFS_DFORK_SIZE(dip,mp,w)            XFS_DFORK_SIZE_ARCH(dip,mp,w,ARCH_NOCONVERT)

#endif

/*
 * Macros for accessing per-fork disk inode information.
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DFORK_Q)
int xfs_dfork_q_arch(xfs_dinode_t *dip, xfs_arch_t arch);
int xfs_dfork_q(xfs_dinode_t *dip);
#define	XFS_DFORK_Q_ARCH(dip,arch)	    xfs_dfork_q_arch(dip,arch)
#define	XFS_DFORK_Q(dip)	            xfs_dfork_q(dip)
#else
#define	XFS_DFORK_Q_ARCH(dip,arch)	    XFS_CFORK_Q_ARCH(&(dip)->di_core, arch)
#define XFS_DFORK_Q(dip)                    XFS_DFORK_Q_ARCH(dip,ARCH_NOCONVERT)

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DFORK_BOFF)
int xfs_dfork_boff_arch(xfs_dinode_t *dip, xfs_arch_t arch);
int xfs_dfork_boff(xfs_dinode_t *dip);
#define	XFS_DFORK_BOFF_ARCH(dip,arch)	    xfs_dfork_boff_arch(dip,arch)
#define	XFS_DFORK_BOFF(dip)	            xfs_dfork_boff(dip)
#else
#define	XFS_DFORK_BOFF_ARCH(dip,arch)	    XFS_CFORK_BOFF_ARCH(&(dip)->di_core, arch)
#define XFS_DFORK_BOFF(dip)                 XFS_DFORK_BOFF_ARCH(dip,ARCH_NOCONVERT)

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DFORK_DPTR)
char *xfs_dfork_dptr_arch(xfs_dinode_t *dip, xfs_arch_t arch);
char *xfs_dfork_dptr(xfs_dinode_t *dip);
#define	XFS_DFORK_DPTR_ARCH(dip,arch)	    xfs_dfork_dptr_arch(dip,arch)
#define	XFS_DFORK_DPTR(dip)	            xfs_dfork_dptr(dip)
#else
#define	XFS_DFORK_DPTR_ARCH(dip,arch)	    ((dip)->di_u.di_c)
#define XFS_DFORK_DPTR(dip)                 XFS_DFORK_DPTR_ARCH(dip,ARCH_NOCONVERT)

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DFORK_APTR)
char *xfs_dfork_aptr_arch(xfs_dinode_t *dip, xfs_arch_t arch);
char *xfs_dfork_aptr(xfs_dinode_t *dip);
#define	XFS_DFORK_APTR_ARCH(dip,arch)       xfs_dfork_aptr_arch(dip,arch)
#define	XFS_DFORK_APTR(dip)                 xfs_dfork_aptr(dip)
#else
#define	XFS_DFORK_APTR_ARCH(dip,arch)	    ((dip)->di_u.di_c + XFS_DFORK_BOFF_ARCH(dip, arch))
#define XFS_DFORK_APTR(dip)                 XFS_DFORK_APTR_ARCH(dip,ARCH_NOCONVERT)

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DFORK_PTR)
char *xfs_dfork_ptr_arch(xfs_dinode_t *dip, int w, xfs_arch_t arch);
char *xfs_dfork_ptr(xfs_dinode_t *dip, int w);
#define	XFS_DFORK_PTR_ARCH(dip,w,arch)      xfs_dfork_ptr_arch(dip,w,arch)
#define	XFS_DFORK_PTR(dip,w)                xfs_dfork_ptr(dip,w)
#else
#define	XFS_DFORK_PTR_ARCH(dip,w,arch)	\
	((w) == XFS_DATA_FORK ? XFS_DFORK_DPTR_ARCH(dip, arch) : XFS_DFORK_APTR_ARCH(dip, arch))
#define XFS_DFORK_PTR(dip,w)                XFS_DFORK_PTR_ARCH(dip,w,ARCH_NOCONVERT)

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_CFORK_FORMAT)
int xfs_cfork_format_arch(xfs_dinode_core_t *dcp, int w, xfs_arch_t arch);
int xfs_cfork_format(xfs_dinode_core_t *dcp, int w);
#define	XFS_CFORK_FORMAT_ARCH(dcp,w,arch)   xfs_cfork_format_arch(dcp,w,arch)
#define	XFS_CFORK_FORMAT(dcp,w)             xfs_cfork_format(dcp,w)
#else
#define	XFS_CFORK_FORMAT_ARCH(dcp,w,arch) \
	((w) == XFS_DATA_FORK ? INT_GET((dcp)->di_format, arch) : INT_GET((dcp)->di_aformat, arch))
#define XFS_CFORK_FORMAT(dcp,w)             XFS_CFORK_FORMAT_ARCH(dcp,w,ARCH_NOCONVERT)

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_CFORK_FMT_SET)
void xfs_cfork_fmt_set_arch(xfs_dinode_core_t *dcp, int w, int n, xfs_arch_t arch);
void xfs_cfork_fmt_set(xfs_dinode_core_t *dcp, int w, int n);
#define	XFS_CFORK_FMT_SET_ARCH(dcp,w,n,arch) xfs_cfork_fmt_set_arch(dcp,w,n,arch)
#define	XFS_CFORK_FMT_SET(dcp,w,n)           xfs_cfork_fmt_set(dcp,w,n)
#else
#define	XFS_CFORK_FMT_SET_ARCH(dcp,w,n,arch) \
	((w) == XFS_DATA_FORK ? \
		(INT_SET((dcp)->di_format, arch, (n))) : \
		(INT_SET((dcp)->di_aformat, arch, (n))))
#define XFS_CFORK_FMT_SET(dcp,w,n)           XFS_CFORK_FMT_SET_ARCH(dcp,w,n,ARCH_NOCONVERT)

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_CFORK_NEXTENTS)
int xfs_cfork_nextents_arch(xfs_dinode_core_t *dcp, int w, xfs_arch_t arch);
int xfs_cfork_nextents(xfs_dinode_core_t *dcp, int w);
#define	XFS_CFORK_NEXTENTS_ARCH(dcp,w,arch)  xfs_cfork_nextents_arch(dcp,w,arch)
#define	XFS_CFORK_NEXTENTS(dcp,w)            xfs_cfork_nextents(dcp,w)
#else
#define	XFS_CFORK_NEXTENTS_ARCH(dcp,w,arch) \
	((w) == XFS_DATA_FORK ? INT_GET((dcp)->di_nextents, arch) : INT_GET((dcp)->di_anextents, arch))
#define XFS_CFORK_NEXTENTS(dcp,w)            XFS_CFORK_NEXTENTS_ARCH(dcp,w,ARCH_NOCONVERT)

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_CFORK_NEXT_SET)
void xfs_cfork_next_set_arch(xfs_dinode_core_t *dcp, int w, int n, xfs_arch_t arch);
void xfs_cfork_next_set(xfs_dinode_core_t *dcp, int w, int n);
#define	XFS_CFORK_NEXT_SET_ARCH(dcp,w,n,arch)	xfs_cfork_next_set_arch(dcp,w,n,arch)
#define	XFS_CFORK_NEXT_SET(dcp,w,n)	        xfs_cfork_next_set(dcp,w,n)
#else
#define	XFS_CFORK_NEXT_SET_ARCH(dcp,w,n,arch) \
	((w) == XFS_DATA_FORK ? \
		(INT_SET((dcp)->di_nextents, arch, (n))) : \
		(INT_SET((dcp)->di_anextents, arch, (n))))
#define XFS_CFORK_NEXT_SET(dcp,w,n)             XFS_CFORK_NEXT_SET_ARCH(dcp,w,n,ARCH_NOCONVERT)

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DFORK_FORMAT)
int xfs_dfork_format_arch(xfs_dinode_t *dip, int w, xfs_arch_t arch);
int xfs_dfork_format(xfs_dinode_t *dip, int w);
#define	XFS_DFORK_FORMAT_ARCH(dip,w,arch)   xfs_dfork_format_arch(dip,w,arch)
#define	XFS_DFORK_FORMAT(dip,w)             xfs_dfork_format(dip,w)
#else
#define	XFS_DFORK_FORMAT_ARCH(dip,w,arch)   XFS_CFORK_FORMAT_ARCH(&(dip)->di_core, w, arch)
#define XFS_DFORK_FORMAT(dip,w)             XFS_DFORK_FORMAT_ARCH(dip,w,ARCH_NOCONVERT)

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DFORK_FMT_SET)
void xfs_dfork_fmt_set_arch(xfs_dinode_t *dip, int w, int n, xfs_arch_t arch);
void xfs_dfork_fmt_set(xfs_dinode_t *dip, int w, int n);
#define	XFS_DFORK_FMT_SET_ARCH(dip,w,n,arch)    xfs_dfork_fmt_set_arch(dip,w,n,arch)
#define	XFS_DFORK_FMT_SET(dip,w,n)              xfs_dfork_fmt_set(dip,w,n)
#else
#define	XFS_DFORK_FMT_SET_ARCH(dip,w,n,arch)	XFS_CFORK_FMT_SET_ARCH(&(dip)->di_core, w, n, arch)
#define XFS_DFORK_FMT_SET(dip,w,n)              XFS_DFORK_FMT_SET_ARCH(dip,w,n,ARCH_NOCONVERT)

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DFORK_NEXTENTS)
int xfs_dfork_nextents_arch(xfs_dinode_t *dip, int w, xfs_arch_t arch);
int xfs_dfork_nextents(xfs_dinode_t *dip, int w);
#define	XFS_DFORK_NEXTENTS_ARCH(dip,w,arch) xfs_dfork_nextents_arch(dip,w,arch)
#define	XFS_DFORK_NEXTENTS(dip,w)           xfs_dfork_nextents(dip,w)
#else
#define	XFS_DFORK_NEXTENTS_ARCH(dip,w,arch) XFS_CFORK_NEXTENTS_ARCH(&(dip)->di_core, w, arch)
#define XFS_DFORK_NEXTENTS(dip,w)           XFS_DFORK_NEXTENTS_ARCH(dip,w,ARCH_NOCONVERT)

#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DFORK_NEXT_SET)
void xfs_dfork_next_set_arch(xfs_dinode_t *dip, int w, int n, xfs_arch_t arch);
void xfs_dfork_next_set(xfs_dinode_t *dip, int w, int n);
#define	XFS_DFORK_NEXT_SET_ARCH(dip,w,n,arch)   xfs_dfork_next_set_arch(dip,w,n,arch)
#define	XFS_DFORK_NEXT_SET(dip,w,n)             xfs_dfork_next_set(dip,w,n)
#else
#define	XFS_DFORK_NEXT_SET_ARCH(dip,w,n,arch)	XFS_CFORK_NEXT_SET_ARCH(&(dip)->di_core, w, n, arch)
#define XFS_DFORK_NEXT_SET(dip,w,n)             XFS_DFORK_NEXT_SET_ARCH(dip,w,n,ARCH_NOCONVERT)

#endif

#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_BUF_TO_DINODE)
xfs_dinode_t *xfs_buf_to_dinode(struct xfs_buf *bp);
#define	XFS_BUF_TO_DINODE(bp)	xfs_buf_to_dinode(bp)
#else
#define	XFS_BUF_TO_DINODE(bp)	((xfs_dinode_t *)(XFS_BUF_PTR(bp)))
#endif

/*
 * Values for di_flags
 * There should be a one-to-one correspondence between these flags and the
 * XFS_XFLAG_s.
 */
#define XFS_DIFLAG_REALTIME_BIT  0	/* file's blocks come from rt area */
#define XFS_DIFLAG_PREALLOC_BIT  1	/* file space has been preallocated */
#define XFS_DIFLAG_NEWRTBM_BIT   2	/* for rtbitmap inode, new format */
#define XFS_DIFLAG_IMMUTABLE_BIT 3	/* inode is immutable */
#define XFS_DIFLAG_APPEND_BIT    4	/* inode is append-only */
#define XFS_DIFLAG_SYNC_BIT      5	/* inode is written synchronously */
#define XFS_DIFLAG_NOATIME_BIT   6	/* do not update atime */
#define XFS_DIFLAG_NODUMP_BIT    7	/* do not dump */
#define XFS_DIFLAG_REALTIME      (1 << XFS_DIFLAG_REALTIME_BIT)
#define XFS_DIFLAG_PREALLOC      (1 << XFS_DIFLAG_PREALLOC_BIT)
#define XFS_DIFLAG_NEWRTBM       (1 << XFS_DIFLAG_NEWRTBM_BIT)
#define XFS_DIFLAG_IMMUTABLE     (1 << XFS_DIFLAG_IMMUTABLE_BIT)
#define XFS_DIFLAG_APPEND        (1 << XFS_DIFLAG_APPEND_BIT)
#define XFS_DIFLAG_SYNC          (1 << XFS_DIFLAG_SYNC_BIT)
#define XFS_DIFLAG_NOATIME       (1 << XFS_DIFLAG_NOATIME_BIT)
#define XFS_DIFLAG_NODUMP        (1 << XFS_DIFLAG_NODUMP_BIT)

#endif	/* __XFS_DINODE_H__ */
