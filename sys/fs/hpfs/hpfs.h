/*-
 * Copyright (c) 1998, 1999 Semen Ustimenko (semenu@FreeBSD.org)
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

/*#define HPFS_DEBUG 10*/
typedef u_int32_t lsn_t;	/* Logical Sector Number */
typedef	struct {
	lsn_t	lsn1;
	lsn_t	lsn2;
} rsp_t;			/* Redundant Sector Pointer */
typedef	struct {
	u_int32_t cnt;
	lsn_t	lsn;
} sptr_t;			/* Storage Pointer */

#define	SUBLOCK	0x10
#define	SUSIZE	DEV_BSIZE
#define	SPBLOCK	0x11
#define	SPSIZE	DEV_BSIZE
#define	BMSIZE	(4 * DEV_BSIZE)
#define	HPFS_MAXFILENAME	255

#define	SU_MAGIC	((u_int64_t)0xFA53E9C5F995E849)
struct sublock {
	u_int64_t	su_magic;	
	u_int8_t	su_hpfsver;
	u_int8_t	su_fnctver;
	u_int16_t	unused;
	lsn_t		su_rootfno;	/* Root Fnode */
	u_int32_t	su_btotal;	/* Total blocks */
	u_int32_t	su_badbtotal;	/* Bad Sectors total */
	rsp_t		su_bitmap;
	rsp_t		su_badbl;
	u_long		su_chkdskdate;
	u_long		su_dskoptdate;
	u_int32_t	su_dbbsz;	/* Sectors in DirBlock Band */
	lsn_t		su_dbbstart;
	lsn_t		su_dbbend;
	lsn_t		su_dbbbitmap;
	char		su_volname[0x20];
	lsn_t		su_uidt;	/* Ptr to User ID Table (8 sect) */
};

#define	SP_MAGIC	((u_int64_t)0xFA5229C5F9911849)
#define	SP_DIRTY	0x0001
#define	SP_SPDBINUSE	0x0002
#define	SP_HFINUSE	0x0004
#define	SP_BADSECT	0x0008
#define	SP_BADBMBL	0x0010
#define	SP_FASTFRMT	0x0020
#define	SP_OLDHPFS	0x0080
#define	SP_IDASD	0x0100
#define	SP_RDASD	0x0200
#define	SP_DASD		0x0400
#define	SP_MMACTIVE	0x0800
#define	SP_DCEACLS	0x1000
#define	SP_DSADDIRTY	0x2000
struct spblock {
	u_int64_t	sp_magic;
	u_int16_t	sp_flag;
	u_int8_t	sp_mmcontf;
	u_int8_t	unused;
	lsn_t		sp_hf;		/* HotFix list */
	u_int32_t	sp_hfinuse;	/* HotFixes in use */
	u_int32_t	sp_hfavail;	/* HotFixes available */
	u_int32_t	sp_spdbavail;	/* Spare DirBlocks available */
	u_int32_t	sp_spdbmax;	/* Spare DirBlocks maximum */
	lsn_t		sp_cpi;
	u_int32_t	sp_cpinum;
	u_int32_t	sp_suchecksum;
	u_int32_t	sp_spchecksum;
	u_int8_t	reserved[0x3C];
	lsn_t		sp_spdb[0x65];
};

#define	DE_SPECIAL	0x0001
#define	DE_ACL		0x0002
#define	DE_DOWN		0x0004
#define	DE_END		0x0008
#define	DE_EALIST	0x0010
#define	DE_EPERM	0x0020
#define	DE_EXPLACL	0x0040
#define	DE_NEEDEA	0x0080
#define	DE_RONLY	0x0100
#define	DE_HIDDEN	0x0200
#define	DE_SYSTEM	0x0400
#define	DE_VOLLABEL	0x0800
#define	DE_DIR		0x1000
#define	DE_ARCHIV	0x2000
#define	DE_DOWNLSN(dep) (*(lsn_t *)((caddr_t)(dep) + (dep)->de_reclen - sizeof(lsn_t)))
#define	DE_NEXTDE(dep)	((struct hpfsdirent *)((caddr_t)(dep) + (dep)->de_reclen))
typedef struct hpfsdirent {
	u_int16_t	de_reclen;
	u_int16_t	de_flag;
	lsn_t		de_fnode;
	u_long		de_mtime;
	u_int32_t	de_size;
	u_long		de_atime;
	u_long		de_ctime;
	u_int32_t	de_ealen;
	u_int8_t	de_flexflag;
	u_int8_t	de_cpid;
	u_int8_t	de_namelen;
	char		de_name[1];
/*	...		de_flex; */
/*	lsn_t		de_down; */
} hpfsdirent_t;

#define	D_BSIZE	(DEV_BSIZE*4)
#define D_MAGIC	0x77E40AAE
#define	D_DIRENT(dbp)	((hpfsdirent_t *)((caddr_t)dbp + sizeof(dirblk_t)))
#define	D_DE(dbp, deoff) ((hpfsdirent_t *)((caddr_t)dbp + sizeof(dirblk_t) + (deoff)))
typedef struct dirblk {
	u_int32_t	d_magic;
	u_int32_t	d_freeoff;	/* Offset of first free byte */
	u_int32_t	d_chcnt;	/* Change count */
	lsn_t		d_parent;
	lsn_t		d_self;
} dirblk_t;

/*
 * Allocation Block (ALBLK)
 */
#define	AB_HBOFFEO	0x01
#define	AB_FNPARENT	0x20
#define	AB_SUGGBSCH	0x40
#define	AB_NODES	0x80
#define	AB_ALLEAF(abp)	((alleaf_t *)((caddr_t)(abp) + sizeof(alblk_t)))
#define	AB_ALNODE(abp)	((alnode_t *)((caddr_t)(abp) + sizeof(alblk_t)))
#define	AB_FREEALP(abp)	((alleaf_t *)((caddr_t)(abp) + (abp)->ab_freeoff))
#define	AB_FREEANP(abp)	((alnode_t *)((caddr_t)(abp) + (abp)->ab_freeoff))
#define	AB_LASTALP(abp)	(AB_ALLEAF(abp) + (abp)->ab_busycnt - 1)
#define	AB_LASTANP(abp)	(AB_ALNODE(abp) + (abp)->ab_busycnt - 1)
#define	AB_ADDNREC(abp, sz, n)	{		\
	(abp)->ab_busycnt += (n);		\
	(abp)->ab_freecnt -= (n);		\
	(abp)->ab_freeoff += (n) * (sz);	\
}
#define	AB_RMNREC(abp, sz, n)		{	\
	(abp)->ab_busycnt -= (n);		\
	(abp)->ab_freecnt += (n);		\
	(abp)->ab_freeoff -= (n) * (sz);\
}
#define	AB_ADDAL(abp)	AB_ADDNREC(abp,sizeof(alleaf_t), 1)
#define	AB_ADDAN(abp)	AB_ADDNREC(abp,sizeof(alnode_t), 1)
#define	AB_RMAL(abp)	AB_RMNREC(abp,sizeof(alleaf_t), 1)
#define	AB_RMAN(abp)	AB_RMNREC(abp,sizeof(alnode_t), 1)
typedef struct alblk {
	u_int8_t	ab_flag;
	u_int8_t	ab_res[3];
	u_int8_t	ab_freecnt;
	u_int8_t	ab_busycnt;
	u_int16_t	ab_freeoff;
} alblk_t;

/*
 * FNode
 */
#define	FNODESIZE	DEV_BSIZE
#define	FN_MAGIC	0xF7E40AAE
struct fnode {
	u_int32_t	fn_magic;
	u_int64_t	fn_readhist;
	u_int8_t	fn_namelen;
	char		fn_name[0xF];		/* First 15 symbols or less */
	lsn_t		fn_parent;
	sptr_t		fn_extacl;
	u_int16_t	fn_acllen;
	u_int8_t	fn_extaclflag;
	u_int8_t	fn_histbitcount;
	sptr_t		fn_extea;
	u_int16_t	fn_ealen;		/* Len of EAs in Fnode */
	u_int8_t	fn_exteaflag;		/* EAs in exteas */
	u_int8_t	fn_flag;
	alblk_t		fn_ab;
	u_int8_t	fn_abd[0x60];
	u_int32_t	fn_size;
	u_int32_t	fn_reqea;
	u_int8_t	fn_uid[0x10];
	u_int16_t	fn_intoff;
	u_int8_t	fn_1dasdthr;
	u_int8_t	fn_dasdthr;
	u_int32_t	fn_dasdlim;
	u_int32_t	fn_dasdusage;
	u_int8_t	fn_int[0x13c];
};

#define	EA_NAME(eap)	((char *)(((caddr_t)(eap)) + sizeof(struct ea)))
struct ea {
	u_int8_t	ea_type;	/* 0 - plain val */
					/* 1 - sptr to val */
					/* 3 - lsn point to AlSec, cont. val */
	u_int8_t	ea_namelen;
	u_int16_t	ea_vallen;
	/*u_int8_t	ea_name[]; */
	/*u_int8_t	ea_val[]; */
};

/*
 * Allocation Block Data (ALNODE)
 *
 * NOTE: AlNodes are used when there are too many fragments
 * to represent the data in the AlBlk
 */
#define	AN_SET(anp,nextoff,lsn)		{	\
	(anp)->an_nextoff = (nextoff); 		\
	(anp)->an_lsn = (lsn); 			\
}
typedef struct alnode {
	u_int32_t	an_nextoff;	/* next node offset in blocks */
	lsn_t		an_lsn;		/* position of AlSec structure */
} alnode_t;

/*
 * Allocaion  Block Data (ALLEAF)
 *
 * NOTE: Leaves are used to point at contiguous block of data
 * (a fragment or an "extent");
 */
#define	AL_SET(alp,off,len,lsn)		{	\
	(alp)->al_off = (off); 			\
	(alp)->al_len = (len); 			\
	(alp)->al_lsn = (lsn); 			\
}
typedef struct alleaf {
	u_int32_t	al_off;		/* offset in blocks */
	u_int32_t	al_len;		/* len in blocks */
	lsn_t		al_lsn;		/* phys position */
} alleaf_t;

/*
 * Allocation Sector
 *
 * NOTE: AlSecs  are not  initialized before use, so they ussually
 * look full of junk. Use the AlBlk  tto validate the data.
 */
#define	AS_MAGIC	0x37E40AAE
typedef struct alsec {
	u_int32_t	as_magic;
	lsn_t		as_self;
	lsn_t		as_parent;
	alblk_t		as_ab;
	u_int8_t	as_abd[0x1E0];
} alsec_t;

/*
 * Code Page structures
 */
struct cpdblk {
	u_int16_t	b_country;	/* Country code */
	u_int16_t	b_cpid;		/* CP ID */
	u_int16_t	b_dbcscnt;	/* Count of DBCS ranges in CP */
	char		b_upcase[0x80];	/* Case conversion table */
	u_int16_t	b_dbcsrange;	/* Start/End DBCS range pairs */
	
};

#define	CPD_MAGIC	((u_int32_t)0x894521F7)
struct cpdsec {
	u_int32_t	d_magic;
	u_int16_t	d_cpcnt;	/* CP Data count */
	u_int16_t	d_cpfirst;	/* Index of first CP Data */
	u_int32_t	d_checksum[3];	/* CP Data checksumms */
	u_int16_t	d_offset[3];	/* Offsets of CP Data blocks */
	struct cpdblk	d_cpdblk[3];	/* Array of CP Data Blocks */
};

struct cpiblk {
	u_int16_t	b_country;	/* Country code */
	u_int16_t	b_cpid;		/* CP ID */
	u_int32_t	b_checksum;
	lsn_t		b_cpdsec;	/* Pointer to CP Data Sector */
	u_int16_t	b_vcpid;	/* Volume spec. CP ID */
	u_int16_t	b_dbcscnt;	/* Count of DBCS ranges in CP */
};

#define	CPI_MAGIC	((u_int32_t)0x494521F7)
struct cpisec {
	u_int32_t	s_magic;
	u_int32_t	s_cpicnt;	/* Count of CPI's in this sector */
	u_int32_t	s_cpifirst;	/* Index of first CPI in this sector */
	lsn_t		s_next;		/* Pointer to next CPI Sector */
	struct cpiblk	s_cpi[0x1F];	/* Array of CPI blocks */
};

struct hpfsmount {
	struct sublock	hpm_su;
	struct spblock	hpm_sp;
	struct mount *	hpm_mp;
	struct vnode *	hpm_devvp;
	struct g_consumer *hpm_cp;
	struct bufobj *hpm_bo;
	struct cdev *hpm_dev;
	uid_t          	hpm_uid;
	gid_t           hpm_gid;
	mode_t          hpm_mode;

	lsn_t *		hpm_bmind;
	struct cpdblk *	hpm_cpdblk;	/* Array of CP Data Blocks */
	u_char		hpm_u2d[0x80];	/* Unix to DOS Table*/
	u_char		hpm_d2u[0x80];	/* DOS to Unix Table*/

	u_long		hpm_bavail;	/* Blocks available */
	u_long		hpm_dbnum;	/* Data Band number */
	u_int8_t *	hpm_bitmap;
};

#define	H_PARVALID	0x0002		/* parent info is valid */
#define	H_CHANGE	0x0004		/* node date was changed */
#define	H_PARCHANGE	0x0008		/* parent node date was changed */
#define	H_INVAL		0x0010		/* Invalid node */
struct hpfsnode {
	struct mtx h_interlock;

	struct hpfsmount *h_hpmp;
	struct fnode 	h_fn;
	struct vnode *	h_vp;
	struct vnode *	h_devvp;
	struct cdev *h_dev;
	lsn_t		h_no;
	uid_t          	h_uid;
	gid_t           h_gid;
	mode_t          h_mode;
	u_int32_t	h_flag;

	/* parent dir information */
	u_long		h_mtime;
	u_long		h_atime;
	u_long		h_ctime;
	char 		h_name[HPFS_MAXFILENAME+1]; /* Used to speedup dirent */
	int 		h_namelen;		    /* lookup */
};

/* This overlays the fid structure (see <sys/mount.h>) */
struct hpfid {
        u_int16_t hpfid_len;     /* Length of structure. */
        u_int16_t hpfid_pad;     /* Force 32-bit alignment. */
        lsn_t     hpfid_ino;     /* File number (ino). */
        int32_t   hpfid_gen;     /* Generation number. */
};

#if defined(HPFS_DEBUG)
#define dprintf(a) printf a
#if HPFS_DEBUG > 1
#define ddprintf(a) printf a
#else
#define ddprintf(a)
#endif
#else
#define dprintf(a)
#define ddprintf(a)
#endif

#if __FreeBSD_version >= 300000
MALLOC_DECLARE(M_HPFSMNT);
MALLOC_DECLARE(M_HPFSNO);
#endif
#define VFSTOHPFS(mp)	((struct hpfsmount *)((mp)->mnt_data))
#define	VTOHP(v)	((struct hpfsnode *)((v)->v_data))
#define	HPTOV(h)	((struct vnode *)((h)->h_vp))
#define	FID(f)		(*((lsn_t *)(f)->fid_data))

extern struct vop_vector hpfs_vnodeops;
