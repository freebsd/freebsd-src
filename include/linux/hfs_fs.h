/* 
 * linux/include/linux/hfs_fs.h
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * The source code distribution of the Columbia AppleTalk Package for
 * UNIX, version 6.0, (CAP) was used as a specification of the
 * location and format of files used by CAP's Aufs.  No code from CAP
 * appears in hfs_fs.  hfs_fs is not a work ``derived'' from CAP in
 * the sense of intellectual property law.
 *
 * The source code distributions of Netatalk, versions 1.3.3b2 and
 * 1.4b2, were used as a specification of the location and format of
 * files used by Netatalk's afpd.  No code from Netatalk appears in
 * hfs_fs.  hfs_fs is not a work ``derived'' from Netatalk in the
 * sense of intellectual property law.
 */

#ifndef _LINUX_HFS_FS_H
#define _LINUX_HFS_FS_H

#include <linux/hfs_sysdep.h>

/* magic numbers for Apple Double header files */
#define HFS_DBL_MAGIC		0x00051607
#define HFS_SNGL_MAGIC		0x00051600
#define HFS_HDR_VERSION_1	0x00010000
#define HFS_HDR_VERSION_2	0x00020000

/* magic numbers for various internal structures */
#define HFS_INO_MAGIC		0x4821
#define HFS_SB_MAGIC		0x4822

/* The space used for the AppleDouble or AppleSingle headers */
#define HFS_DBL_HDR_LEN		1024

/* The space used for the Netatalk header */
#define HFS_NAT_HDR_LEN		1024  /* 589 for an exact match */

/* Macros to extract CNID and file "type" from the Linux inode number */
#define HFS_CNID(X)	((X) & 0x3FFFFFFF)
#define HFS_ITYPE(X)	((X) & 0xC0000000)

/* Macros to enumerate types */
#define HFS_ITYPE_TO_INT(X)	((X) >> 30)
#define HFS_INT_TO_ITYPE(X)	((X) << 30)

/* generic ITYPEs */
#define HFS_ITYPE_0	0x00000000
#define HFS_ITYPE_1	0x40000000
#define HFS_ITYPE_2	0x80000000
#define HFS_ITYPE_3	0xC0000000
#define HFS_ITYPE_NORM	HFS_ITYPE_0	/* "normal" directory or file */

/* ITYPEs for CAP */
#define HFS_CAP_NORM	HFS_ITYPE_0	/* data fork or normal directory */
#define HFS_CAP_DATA	HFS_ITYPE_0	/* data fork of file */
#define HFS_CAP_NDIR	HFS_ITYPE_0	/* normal directory */
#define HFS_CAP_FNDR	HFS_ITYPE_1	/* finder info for file or dir */
#define HFS_CAP_RSRC	HFS_ITYPE_2	/* resource fork of file */
#define HFS_CAP_RDIR	HFS_ITYPE_2	/* .resource directory */
#define HFS_CAP_FDIR	HFS_ITYPE_3	/* .finderinfo directory */

/* ITYPEs for Apple Double */
#define HFS_DBL_NORM	HFS_ITYPE_0	/* data fork or directory */
#define HFS_DBL_DATA	HFS_ITYPE_0	/* data fork of file */
#define HFS_DBL_DIR	HFS_ITYPE_0	/* directory */
#define HFS_DBL_HDR	HFS_ITYPE_1	/* AD header of file or dir */

/* ITYPEs for netatalk */
#define HFS_NAT_NORM	HFS_ITYPE_0	/* data fork or directory */
#define HFS_NAT_DATA	HFS_ITYPE_0	/* data fork of file */
#define HFS_NAT_NDIR	HFS_ITYPE_0	/* normal directory */
#define HFS_NAT_HDR	HFS_ITYPE_1	/* AD header of file or dir */
#define HFS_NAT_HDIR	HFS_ITYPE_2	/* directory holding AD headers */

/* ITYPEs for Apple Single */
#define HFS_SGL_NORM	HFS_ITYPE_0	/* AppleSingle file or directory */
#define HFS_SGL_SNGL	HFS_ITYPE_0	/* AppleSingle file */
#define HFS_SGL_DIR	HFS_ITYPE_0	/* directory */
#define HFS_SGL_DINF	HFS_ITYPE_1	/* %DirInfo for directory */

/* IDs for elements of an AppleDouble or AppleSingle header */
#define HFS_HDR_DATA	1   /* data fork */
#define HFS_HDR_RSRC	2   /* resource fork */
#define HFS_HDR_FNAME	3   /* full (31-character) name */
#define HFS_HDR_COMNT	4   /* comment */
#define HFS_HDR_BWICN	5   /* b/w icon */
#define HFS_HDR_CICON	6   /* color icon info */
#define HFS_HDR_OLDI	7   /* old file info */
#define HFS_HDR_DATES	8   /* file dates info */
#define HFS_HDR_FINFO	9   /* Finder info */
#define HFS_HDR_MACI	10  /* Macintosh info */
#define HFS_HDR_PRODOSI 11  /* ProDOS info */
#define HFS_HDR_MSDOSI  12  /* MSDOS info */
#define HFS_HDR_SNAME   13  /* short name */
#define HFS_HDR_AFPI    14  /* AFP file info */
#define HFS_HDR_DID     15  /* directory id */
#define HFS_HDR_MAX	16

/*
 * There are three time systems.  All three are based on seconds since
 * a particular time/date.
 *	Unix:	unsigned lil-endian since 00:00 GMT, Jan. 1, 1970
 *	mac:	unsigned big-endian since 00:00 GMT, Jan. 1, 1904
 *	header:	  SIGNED big-endian since 00:00 GMT, Jan. 1, 2000
 *
 */
#define hfs_h_to_mtime(ARG)	htonl((hfs_s32)ntohl(ARG)+3029529600U)
#define hfs_m_to_htime(ARG)	((hfs_s32)htonl(ntohl(ARG)-3029529600U))
#define hfs_h_to_utime(ARG)	((hfs_s32)hfs_to_utc(ntohl(ARG)+946684800U))
#define hfs_u_to_htime(ARG)	((hfs_s32)htonl(hfs_from_utc(ARG)-946684800U))
#define hfs_u_to_mtime(ARG)	htonl(hfs_from_utc(ARG)+2082844800U)
#define hfs_m_to_utime(ARG)	(hfs_to_utc(ntohl(ARG)-2082844800U))

/*======== Data structures kept in memory ========*/

/*
 * A descriptor for a single entry within the header of an
 * AppleDouble or AppleSingle header file.
 * An array of these make up a table of contents for the file.
 */
struct hfs_hdr_descr {
	hfs_u32	id;	/* The Apple assigned ID for the entry type */
	hfs_u32	offset;	/* The offset to reach the entry */
	hfs_u32	length;	/* The length of the entry */
};

/*
 * The info needed to reconstruct a given header layout
 */
struct hfs_hdr_layout {
	hfs_u32		magic;			/* AppleSingle or AppleDouble */
	hfs_u32		version;		/* 0x00010000 or 0x00020000 */
	hfs_u16		entries;		/* How many entries used */
	struct hfs_hdr_descr	
			descr[HFS_HDR_MAX];	/* Descriptors */
	struct hfs_hdr_descr	
			*order[HFS_HDR_MAX];	/* 'descr' ordered by offset */
};

/* header layout for netatalk's v1 appledouble file format */
struct hfs_nat_hdr {
	hfs_lword_t	magic;
	hfs_lword_t	version;
	hfs_byte_t	homefs[16];
	hfs_word_t	entries;
	hfs_byte_t	descrs[12*5];
	hfs_byte_t	real_name[255];	/* id=3 */
	hfs_byte_t	comment[200];	/* id=4 XXX: not yet implemented */
	hfs_byte_t	old_info[16];	/* id=7 */
	hfs_u8		finderinfo[32]; /* id=9 */
};

/* 
 * Default header layout for Netatalk and AppleDouble
 */
struct hfs_dbl_hdr {
	hfs_lword_t	magic;
	hfs_lword_t	version;
	hfs_byte_t	filler[16];
	hfs_word_t	entries;
	hfs_byte_t	descrs[12*HFS_HDR_MAX];
	hfs_byte_t	real_name[255];	/* id=3 */
	hfs_byte_t	comment[200];	/* id=4 XXX: not yet implemented */
	hfs_u32		create_time;	/* \	          */
	hfs_u32		modify_time;	/*  | id=8 (or 7) */
	hfs_u32		backup_time;	/*  |	          */
	hfs_u32         access_time;    /* /  (attributes with id=7) */
	hfs_u8		finderinfo[32]; /* id=9 */
	hfs_u32		fileinfo;	/* id=10 */
        hfs_u32         cnid;           /* id=15 */
	hfs_u8          short_name[12]; /* id=13 */
	hfs_u8          prodosi[8];     /* id=11 */
};


/* finder metadata for CAP */
struct hfs_cap_info {
	hfs_byte_t	fi_fndr[32];	/* Finder's info */
	hfs_word_t	fi_attr;	/* AFP attributes (f=file/d=dir) */
#define HFS_AFP_INV             0x001   /* Invisible bit (f/d) */
#define HFS_AFP_EXPFOLDER       0x002   /* exported folder (d) */
#define HFS_AFP_MULTI           0x002   /* Multiuser bit (f) */
#define HFS_AFP_SYS             0x004   /* System bit (f/d) */
#define HFS_AFP_DOPEN           0x008   /* data fork already open (f) */
#define HFS_AFP_MOUNTED         0x008   /* mounted folder (d) */
#define HFS_AFP_ROPEN           0x010   /* resource fork already open (f) */
#define HFS_AFP_INEXPFOLDER     0x010   /* folder in shared area (d) */
#define HFS_AFP_WRI		0x020	/* Write inhibit bit (readonly) (f) */
#define HFS_AFP_BACKUP          0x040   /* backup needed bit (f/d)  */
#define HFS_AFP_RNI		0x080	/* Rename inhibit bit (f/d) */
#define HFS_AFP_DEI		0x100	/* Delete inhibit bit (f/d) */
#define HFS_AFP_NOCOPY          0x400   /* Copy protect bit (f) */
#define HFS_AFP_RDONLY	(	HFS_AFP_WRI|HFS_AFP_RNI|HFS_AFP_DEI)
	hfs_byte_t	fi_magic1;	/* Magic number: */
#define HFS_CAP_MAGIC1		0xFF
	hfs_byte_t	fi_version;	/* Version of this structure: */
#define HFS_CAP_VERSION		0x10
	hfs_byte_t	fi_magic;	/* Another magic number: */
#define HFS_CAP_MAGIC		0xDA
	hfs_byte_t	fi_bitmap;	/* Bitmap of which names are valid: */
#define HFS_CAP_SHORTNAME	0x01
#define HFS_CAP_LONGNAME	0x02
	hfs_byte_t	fi_shortfilename[12+1];	/* "short name" (unused) */
	hfs_byte_t	fi_macfilename[32+1];	/* Original (Macintosh) name */
	hfs_byte_t	fi_comln;	/* Length of comment (always 0) */
	hfs_byte_t	fi_comnt[200];	/* Finder comment (unused) */
	/* optional: 	used by aufs only if compiled with USE_MAC_DATES */
	hfs_byte_t	fi_datemagic;	/* Magic number for dates extension: */
#define HFS_CAP_DMAGIC		0xDA
	hfs_byte_t	fi_datevalid;	/* Bitmap of which dates are valid: */
#define HFS_CAP_MDATE		0x01
#define HFS_CAP_CDATE		0x02
	hfs_lword_t	fi_ctime;	/* Creation date (in AFP format) */
	hfs_lword_t	fi_mtime;	/* Modify date (in AFP format) */
	hfs_lword_t	fi_utime;	/* Un*x time of last mtime change */
	hfs_byte_t	pad;
};

#ifdef __KERNEL__

typedef ssize_t hfs_rwret_t;
typedef size_t hfs_rwarg_t;

#include <asm/uaccess.h>

/* Some forward declarations */
struct hfs_fork;
struct hfs_cat_key;
struct hfs_cat_entry;
extern struct hfs_cat_entry *hfs_cat_get(struct hfs_mdb *,
					 const struct hfs_cat_key *);

/* dir.c */
extern int hfs_create(struct inode *, struct dentry *, int);
extern int hfs_mkdir(struct inode *, struct dentry *, int);
extern int hfs_unlink(struct inode *, struct dentry *);
extern int hfs_rmdir(struct inode *, struct dentry *);
extern int hfs_rename(struct inode *, struct dentry *,
		      struct inode *, struct dentry *);

/* dir_cap.c */
extern const struct hfs_name hfs_cap_reserved1[];
extern const struct hfs_name hfs_cap_reserved2[];
extern struct inode_operations hfs_cap_ndir_inode_operations;
extern struct inode_operations hfs_cap_fdir_inode_operations;
extern struct inode_operations hfs_cap_rdir_inode_operations;
extern struct file_operations hfs_cap_dir_operations;
extern void hfs_cap_drop_dentry(struct dentry *, const ino_t);

/* dir_dbl.c */
extern const struct hfs_name hfs_dbl_reserved1[];
extern const struct hfs_name hfs_dbl_reserved2[];
extern struct inode_operations hfs_dbl_dir_inode_operations;
extern struct file_operations hfs_dbl_dir_operations;
extern void hfs_dbl_drop_dentry(struct dentry *, const ino_t);

/* dir_nat.c */
extern const struct hfs_name hfs_nat_reserved1[];
extern const struct hfs_name hfs_nat_reserved2[];
extern struct inode_operations hfs_nat_ndir_inode_operations;
extern struct inode_operations hfs_nat_hdir_inode_operations;
extern struct file_operations hfs_nat_dir_operations;
extern void hfs_nat_drop_dentry(struct dentry *, const ino_t);

/* file.c */
extern hfs_s32 hfs_do_read(struct inode *, struct hfs_fork *, hfs_u32,
			   char *, hfs_u32, int);
extern hfs_s32 hfs_do_write(struct inode *, struct hfs_fork *, hfs_u32,
			    const char *, hfs_u32);
extern void hfs_file_fix_mode(struct hfs_cat_entry *entry);
extern struct inode_operations hfs_file_inode_operations;
extern struct file_operations hfs_file_operations;

/* file_cap.c */
extern struct inode_operations hfs_cap_info_inode_operations;
extern struct file_operations hfs_cap_info_operations;

/* file_hdr.c */
extern struct inode_operations hfs_hdr_inode_operations;
extern struct file_operations hfs_hdr_operations;
extern const struct hfs_hdr_layout hfs_dbl_fil_hdr_layout;
extern const struct hfs_hdr_layout hfs_dbl_dir_hdr_layout;
extern const struct hfs_hdr_layout hfs_nat_hdr_layout;
extern const struct hfs_hdr_layout hfs_nat2_hdr_layout;
extern const struct hfs_hdr_layout hfs_sngl_hdr_layout;
extern void hdr_truncate(struct inode *,size_t);

/* inode.c */
extern void hfs_put_inode(struct inode *);
extern int hfs_notify_change(struct dentry *, struct iattr *);
extern int hfs_notify_change_cap(struct dentry *, struct iattr *);
extern int hfs_notify_change_hdr(struct dentry *, struct iattr *);
extern struct inode *hfs_iget(struct hfs_cat_entry *, ino_t, struct dentry *);

extern void hfs_cap_ifill(struct inode *, ino_t, const int);
extern void hfs_dbl_ifill(struct inode *, ino_t, const int);
extern void hfs_nat_ifill(struct inode *, ino_t, const int);
extern void hfs_sngl_ifill(struct inode *, ino_t, const int);

/* super.c */
extern struct super_block *hfs_read_super(struct super_block *,void *,int);
extern int hfs_remount(struct super_block *, int *, char *);

/* trans.c */
extern void hfs_colon2mac(struct hfs_name *, const char *, int);
extern void hfs_prcnt2mac(struct hfs_name *, const char *, int);
extern void hfs_triv2mac(struct hfs_name *, const char *, int);
extern void hfs_latin2mac(struct hfs_name *, const char *, int);
extern int hfs_mac2cap(char *, const struct hfs_name *);
extern int hfs_mac2nat(char *, const struct hfs_name *);
extern int hfs_mac2latin(char *, const struct hfs_name *);
extern int hfs_mac2seven(char *, const struct hfs_name *);
extern int hfs_mac2eight(char *, const struct hfs_name *);
extern int hfs_mac2alpha(char *, const struct hfs_name *);
extern int hfs_mac2triv(char *, const struct hfs_name *);
extern void hfs_tolower(unsigned char *, int);

#define	HFS_I(X)	(&((X)->u.hfs_i))
#define	HFS_SB(X)	(&((X)->u.hfs_sb))

static inline void hfs_nameout(struct inode *dir, struct hfs_name *out,
				   const char *in, int len) {
	HFS_SB(dir->i_sb)->s_nameout(out, in, len);
}

static inline int hfs_namein(struct inode *dir, char *out,
				 const struct hfs_name *in) {
	int len = HFS_SB(dir->i_sb)->s_namein(out, in);
	if (HFS_SB(dir->i_sb)->s_lowercase) {
		hfs_tolower(out, len);
	}
	return len;
}

#endif /* __KERNEL__ */
#endif
