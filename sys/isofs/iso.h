/*
 *	$Id: iso.h,v 1.5 1993/12/19 00:51:02 wollman Exp $
 */

#ifndef _ISOFS_ISO_H_
#define _ISOFS_ISO_H_ 1

#define ISODCL(from, to) (to - from + 1)

struct iso_volume_descriptor {
	char type[ISODCL(1,1)]; /* 711 */
	char id[ISODCL(2,6)];
	char version[ISODCL(7,7)];
	char data[ISODCL(8,2048)];
};

/* volume descriptor types */
#define ISO_VD_PRIMARY 1
#define ISO_VD_END 255

#define ISO_STANDARD_ID "CD001"
#define ISO_ECMA_ID     "CDW01"

struct iso_primary_descriptor {
	char type			[ISODCL (  1,   1)]; /* 711 */
	char id				[ISODCL (  2,   6)];
	char version			[ISODCL (  7,   7)]; /* 711 */
	char unused1			[ISODCL (  8,   8)];
	char system_id			[ISODCL (  9,  40)]; /* achars */
	char volume_id			[ISODCL ( 41,  72)]; /* dchars */
	char unused2			[ISODCL ( 73,  80)];
	char volume_space_size		[ISODCL ( 81,  88)]; /* 733 */
	char unused3			[ISODCL ( 89, 120)];
	char volume_set_size		[ISODCL (121, 124)]; /* 723 */
	char volume_sequence_number	[ISODCL (125, 128)]; /* 723 */
	char logical_block_size		[ISODCL (129, 132)]; /* 723 */
	char path_table_size		[ISODCL (133, 140)]; /* 733 */
	char type_l_path_table		[ISODCL (141, 144)]; /* 731 */
	char opt_type_l_path_table	[ISODCL (145, 148)]; /* 731 */
	char type_m_path_table		[ISODCL (149, 152)]; /* 732 */
	char opt_type_m_path_table	[ISODCL (153, 156)]; /* 732 */
	char root_directory_record	[ISODCL (157, 190)]; /* 9.1 */
	char volume_set_id		[ISODCL (191, 318)]; /* dchars */
	char publisher_id		[ISODCL (319, 446)]; /* achars */
	char preparer_id		[ISODCL (447, 574)]; /* achars */
	char application_id		[ISODCL (575, 702)]; /* achars */
	char copyright_file_id		[ISODCL (703, 739)]; /* 7.5 dchars */
	char abstract_file_id		[ISODCL (740, 776)]; /* 7.5 dchars */
	char bibliographic_file_id	[ISODCL (777, 813)]; /* 7.5 dchars */
	char creation_date		[ISODCL (814, 830)]; /* 8.4.26.1 */
	char modification_date		[ISODCL (831, 847)]; /* 8.4.26.1 */
	char expiration_date		[ISODCL (848, 864)]; /* 8.4.26.1 */
	char effective_date		[ISODCL (865, 881)]; /* 8.4.26.1 */
	char file_structure_version	[ISODCL (882, 882)]; /* 711 */
	char unused4			[ISODCL (883, 883)];
	char application_data		[ISODCL (884, 1395)];
	char unused5			[ISODCL (1396, 2048)];
};

struct iso_directory_record {
	char length			[ISODCL (1, 1)]; /* 711 */
	char ext_attr_length		[ISODCL (2, 2)]; /* 711 */
	unsigned char extent		[ISODCL (3, 10)]; /* 733 */
	unsigned char size		[ISODCL (11, 18)]; /* 733 */
	char date			[ISODCL (19, 25)]; /* 7 by 711 */
	char flags			[ISODCL (26, 26)];
	char file_unit_size		[ISODCL (27, 27)]; /* 711 */
	char interleave			[ISODCL (28, 28)]; /* 711 */
	char volume_sequence_number	[ISODCL (29, 32)]; /* 723 */
	char name_len			[ISODCL (33, 33)]; /* 711 */
	char name			[0];
};

/* CD-ROM Fromat type */
enum ISO_FTYPE  { ISO_FTYPE_9660, ISO_FTYPE_RRIP, ISO_FTYPE_ECMA };

struct iso_mnt {
	int logical_block_size;
	int volume_space_size;
	struct vnode *im_devvp;
	char im_fsmnt[50];
	
	int im_ronly;
	int im_fmod;
	struct mount *im_mountp;
	dev_t im_dev;

	int im_bshift;
	int im_bmask;
	int im_bsize;

	char root[ISODCL (157, 190)];
	int root_extent;
	int root_size;
	enum ISO_FTYPE  iso_ftype;
};

#define VFSTOISOFS(mp)	((struct iso_mnt *)((mp)->mnt_data))

#define iso_blkoff(imp, loc) ((loc) & ~(imp)->im_bmask)
#define iso_lblkno(imp, loc) ((loc) >> (imp)->im_bshift)
#define iso_blksize(imp, ip, lbn) ((imp)->im_bsize)
#define iso_lblktosize(imp, blk) ((blk) << (imp)->im_bshift)


int isofs_mount __P((struct mount *mp, char *path, caddr_t data,
	struct nameidata *ndp, struct proc *p));
int isofs_start __P((struct mount *mp, int flags, struct proc *p));
int isofs_unmount __P((struct mount *mp, int mntflags, struct proc *p));
int isofs_root __P((struct mount *mp, struct vnode **vpp));
int isofs_statfs __P((struct mount *mp, struct statfs *sbp, struct proc *p));
int isofs_sync __P((struct mount *mp, int waitfor));
int isofs_fhtovp __P((struct mount *mp, struct fid *fhp, struct vnode **vpp));
int isofs_vptofh __P((struct vnode *vp, struct fid *fhp));
void isofs_init __P((void));

/* From isofs_util.c: */
extern int isonum_711(char *);
extern int isonum_712(char *);
extern int isonum_721(char *);
extern int isonum_722(char *);
extern int isonum_723(char *);
extern int isonum_731(u_char *);
extern int isonum_732(u_char *);
extern int isonum_733(u_char *);
extern int isofncmp(char *, int, char *, int);
extern void isofntrans(char *, int, char *, int *);

#endif /* _ISOFS_ISO_H_ */
