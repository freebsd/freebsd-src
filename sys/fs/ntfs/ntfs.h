/*	$NetBSD: ntfs.h,v 1.9 1999/10/31 19:45:26 jdolecek Exp $	*/

/*-
 * Copyright (c) 1998, 1999 Semen Ustimenko
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

/*#define NTFS_DEBUG 1*/

typedef uint64_t cn_t;
typedef uint16_t wchar;

#pragma pack(1)
#define	BBSIZE			1024
#define	BBOFF			((off_t)(0))
#define	BBLOCK			0
#define	NTFS_MFTINO		0
#define	NTFS_VOLUMEINO		3
#define	NTFS_ATTRDEFINO		4
#define	NTFS_ROOTINO		5
#define	NTFS_BITMAPINO		6
#define	NTFS_BOOTINO		7
#define	NTFS_BADCLUSINO		8
#define	NTFS_UPCASEINO		10
#define	NTFS_MAXFILENAME	255

struct fixuphdr {
	uint32_t	fh_magic;
	uint16_t	fh_foff;
	uint16_t	fh_fnum;
};

#define	NTFS_AF_INRUN	0x00000001
struct attrhdr {
	uint32_t	a_type;
	uint32_t	reclen;
	uint8_t		a_flag;
	uint8_t		a_namelen;
	uint8_t		a_nameoff;
	uint8_t		reserved1;
	uint8_t		a_compression;
	uint8_t		reserved2;
	uint16_t	a_index;
};
#define	NTFS_A_STD	0x10
#define	NTFS_A_ATTRLIST	0x20
#define	NTFS_A_NAME	0x30
#define	NTFS_A_VOLUMENAME	0x60
#define	NTFS_A_DATA	0x80
#define	NTFS_A_INDXROOT	0x90
#define	NTFS_A_INDX	0xA0
#define	NTFS_A_INDXBITMAP 0xB0

#define	NTFS_MAXATTRNAME	255
struct attr {
	struct attrhdr	a_hdr;
	union {
		struct {
			uint16_t	a_datalen;
			uint16_t	reserved1;
			uint16_t	a_dataoff;
			uint16_t	a_indexed;
		} a_S_r;
		struct {
			cn_t		a_vcnstart;
			cn_t		a_vcnend;
			uint16_t	a_dataoff;
			uint16_t	a_compressalg;
			uint32_t	reserved1;
			uint64_t	a_allocated;
			uint64_t	a_datalen;
			uint64_t	a_initialized;
		} a_S_nr;
	} a_S;
};
#define	a_r	a_S.a_S_r
#define	a_nr	a_S.a_S_nr

typedef struct {
	uint64_t	t_create;
	uint64_t	t_write;
	uint64_t	t_mftwrite;
	uint64_t	t_access;
} ntfs_times_t;

#define	NTFS_FFLAG_RDONLY	0x01LL
#define	NTFS_FFLAG_HIDDEN	0x02LL
#define	NTFS_FFLAG_SYSTEM	0x04LL
#define	NTFS_FFLAG_ARCHIVE	0x20LL
#define	NTFS_FFLAG_COMPRESSED	0x0800LL
#define	NTFS_FFLAG_DIR		0x10000000LL

struct attr_name {
	uint32_t	n_pnumber;	/* Parent ntnode */
	uint32_t	reserved;
	ntfs_times_t	n_times;
	uint64_t	n_size;
	uint64_t	n_attrsz;
	uint64_t	n_flag;
	uint8_t		n_namelen;
	uint8_t		n_nametype;
	uint16_t	n_name[1];
};

#define	NTFS_IRFLAG_INDXALLOC	0x00000001
struct attr_indexroot {
	uint32_t	ir_unkn1;	/* always 0x30 */
	uint32_t	ir_unkn2;	/* always 0x1 */
	uint32_t	ir_size;/* ??? */
	uint32_t	ir_unkn3;	/* number of cluster */
	uint32_t	ir_unkn4;	/* always 0x10 */
	uint32_t	ir_datalen;	/* sizeof simething */
	uint32_t	ir_allocated;	/* same as above */
	uint16_t	ir_flag;/* ?? always 1 */
	uint16_t	ir_unkn7;
};

struct attr_attrlist {
	uint32_t	al_type;	/* Attribute type */
	uint16_t	reclen;		/* length of this entry */
	uint8_t		al_namelen;	/* Attribute name len */
	uint8_t		al_nameoff;	/* Name offset from entry start */
	uint64_t	al_vcnstart;	/* VCN number */
	uint32_t	al_inumber;	/* Parent ntnode */
	uint32_t	reserved;
	uint16_t	al_index;	/* Attribute index in MFT record */
	uint16_t	al_name[1];	/* Name */
};

#define	NTFS_INDXMAGIC	(uint32_t)(0x58444E49)
struct attr_indexalloc {
	struct fixuphdr ia_fixup;
	uint64_t	unknown1;
	cn_t		ia_bufcn;
	uint16_t	ia_hdrsize;
	uint16_t	unknown2;
	uint32_t	ia_inuse;
	uint32_t	ia_allocated;
};

#define	NTFS_IEFLAG_SUBNODE	0x00000001
#define	NTFS_IEFLAG_LAST	0x00000002

struct attr_indexentry {
	uint32_t	ie_number;
	uint32_t	unknown1;
	uint16_t	reclen;
	uint16_t	ie_size;
	uint32_t	ie_flag; /* 1 - has subnodes, 2 - last */
	uint32_t	ie_fpnumber;
	uint32_t	unknown2;
	ntfs_times_t	ie_ftimes;
	uint64_t	ie_fallocated;
	uint64_t	ie_fsize;
	uint64_t	ie_fflag;
	uint8_t		ie_fnamelen;
	uint8_t		ie_fnametype;
	wchar		ie_fname[NTFS_MAXFILENAME];
	/* cn_t		ie_bufcn;	 buffer with subnodes */
};

#define	NTFS_FILEMAGIC	(uint32_t)(0x454C4946)
#define	NTFS_BLOCK_SIZE	512
#define	NTFS_FRFLAG_DIR	0x0002
struct filerec {
	struct fixuphdr	fr_fixup;
	uint8_t		reserved[8];
	uint16_t	fr_seqnum;	/* Sequence number */
	uint16_t	fr_nlink;
	uint16_t	fr_attroff;	/* offset to attributes */
	uint16_t	fr_flags;	/* 1-nonresident attr, 2-directory */
	uint32_t	fr_size;/* hdr + attributes */
	uint32_t	fr_allocated;	/* allocated length of record */
	uint64_t	fr_mainrec;	/* main record */
	uint16_t	fr_attrnum;	/* maximum attr number + 1 ??? */
};

#define	NTFS_ATTRNAME_MAXLEN	0x40
#define	NTFS_ADFLAG_NONRES	0x0080	/* Attrib can be non resident */
#define	NTFS_ADFLAG_INDEX	0x0002	/* Attrib can be indexed */
struct attrdef {
	wchar		ad_name[NTFS_ATTRNAME_MAXLEN];
	uint32_t	ad_type;
	uint32_t	reserved1[2];
	uint32_t	ad_flag;
	uint64_t	ad_minlen;
	uint64_t	ad_maxlen;	/* -1 for nonlimited */
};

struct ntvattrdef {
	char		ad_name[0x40];
	int		ad_namelen;
	uint32_t	ad_type;
};

#define	NTFS_BBID	"NTFS    "
#define	NTFS_BBIDLEN	8
struct bootfile {
	uint8_t		reserved1[3];	/* asm jmp near ... */
	uint8_t		bf_sysid[8];	/* 'NTFS    ' */
	uint16_t	bf_bps;		/* bytes per sector */
	uint8_t		bf_spc;		/* sectors per cluster */
	uint8_t		reserved2[7];	/* unused (zeroed) */
	uint8_t		bf_media;	/* media desc. (0xF8) */
	uint8_t		reserved3[2];
	uint16_t	bf_spt;		/* sectors per track */
	uint16_t	bf_heads;	/* number of heads */
	uint8_t		reserver4[12];
	uint64_t	bf_spv;		/* sectors per volume */
	cn_t		bf_mftcn;	/* $MFT cluster number */
	cn_t		bf_mftmirrcn;	/* $MFTMirr cn */
	uint8_t		bf_mftrecsz;	/* MFT record size (clust) */
					/* 0xF6 inducates 1/4 */
	uint32_t	bf_ibsz;	/* index buffer size */
	uint32_t	bf_volsn;	/* volume ser. num. */
};

#define	NTFS_SYSNODESNUM	0x0B
struct ntfsmount {
	struct mount   *ntm_mountp;	/* filesystem vfs structure */
	struct bootfile	ntm_bootfile;
	struct g_consumer *ntm_cp;
	struct bufobj  *ntm_bo;
	struct vnode   *ntm_devvp;	/* block device mounted vnode */
	struct vnode   *ntm_sysvn[NTFS_SYSNODESNUM];
	uint32_t	ntm_bpmftrec;
	uid_t		ntm_uid;
	gid_t		ntm_gid;
	mode_t		ntm_mode;
	uint64_t	ntm_flag;
	cn_t		ntm_cfree;
	struct ntvattrdef *ntm_ad;
	int		ntm_adnum;
	wchar *		ntm_82u;	/* 8bit to Unicode */
	char **		ntm_u28;	/* Unicode to 8 bit */
	void *		ntm_ic_l2u;	/* Local to Unicode (iconv) */
	void *		ntm_ic_u2l;	/* Unicode to Local (iconv) */
	uint8_t		ntm_multiplier; /* NTFS blockno to DEV_BSIZE sectorno */
};

#define	ntm_mftcn	ntm_bootfile.bf_mftcn
#define	ntm_mftmirrcn	ntm_bootfile.bf_mftmirrcn
#define	ntm_mftrecsz	ntm_bootfile.bf_mftrecsz
#define	ntm_spc		ntm_bootfile.bf_spc
#define	ntm_bps		ntm_bootfile.bf_bps

#pragma pack()

#define	NTFS_NEXTREC(s, type) ((type)(((caddr_t) s) + (s)->reclen))

/* Convert mount ptr to ntfsmount ptr. */
#define	VFSTONTFS(mp)	((struct ntfsmount *)((mp)->mnt_data))
#define	VTONT(v)	FTONT(VTOF(v))
#define	VTOF(v)		((struct fnode *)((v)->v_data))
#define	FTOV(f)		((f)->f_vp)
#define	FTONT(f)	((f)->f_ip)
#define	ntfs_cntobn(cn)	(daddr_t)((cn) * (ntmp->ntm_spc))
#define	ntfs_cntob(cn)	(off_t)((cn) * (ntmp)->ntm_spc * (ntmp)->ntm_bps)
#define	ntfs_btocn(off)	(cn_t)((off) / ((ntmp)->ntm_spc * (ntmp)->ntm_bps))
#define	ntfs_btocl(off)	(cn_t)((off + ntfs_cntob(1) - 1) / ((ntmp)->ntm_spc * (ntmp)->ntm_bps))
#define	ntfs_btocnoff(off)	(off_t)((off) % ((ntmp)->ntm_spc * (ntmp)->ntm_bps))
#define	ntfs_bntob(bn)	(daddr_t)((bn) * (ntmp)->ntm_bps)

#define	ntfs_bpbl	(daddr_t)((ntmp)->ntm_bps)

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_NTFSNTNODE);
MALLOC_DECLARE(M_NTFSFNODE);
MALLOC_DECLARE(M_NTFSDIR);
MALLOC_DECLARE(M_NTFSNTHASH);
#endif

#if defined(NTFS_DEBUG)
#define	dprintf(a)	printf a
#if NTFS_DEBUG > 1
#define	ddprintf(a)	printf a
#else
#define	ddprintf(a)	(void)0
#endif
#else
#define	dprintf(a)	(void)0
#define	ddprintf(a)	(void)0
#endif

extern struct vop_vector ntfs_vnodeops;
