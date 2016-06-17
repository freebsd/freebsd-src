#ifndef _HPFS_FS_SB
#define _HPFS_FS_SB

struct hpfs_sb_info {
	ino_t sb_root;			/* inode number of root dir */
	unsigned sb_fs_size;		/* file system size, sectors */
	unsigned sb_bitmaps;		/* sector number of bitmap list */
	unsigned sb_dirband_start;	/* directory band start sector */
	unsigned sb_dirband_size;	/* directory band size, dnodes */
	unsigned sb_dmap;		/* sector number of dnode bit map */
	unsigned sb_n_free;		/* free blocks for statfs, or -1 */
	unsigned sb_n_free_dnodes;	/* free dnodes for statfs, or -1 */
	uid_t sb_uid;			/* uid from mount options */
	gid_t sb_gid;			/* gid from mount options */
	umode_t sb_mode;		/* mode from mount options */
	unsigned sb_conv : 2;		/* crlf->newline hackery */
	unsigned sb_eas : 2;		/* eas: 0-ignore, 1-ro, 2-rw */
	unsigned sb_err : 2;		/* on errs: 0-cont, 1-ro, 2-panic */
	unsigned sb_chk : 2;		/* checks: 0-no, 1-normal, 2-strict */
	unsigned sb_lowercase : 1;	/* downcase filenames hackery */
	unsigned sb_was_error : 1;	/* there was an error, set dirty flag */
	unsigned sb_chkdsk : 2;		/* chkdsk: 0-no, 1-on errs, 2-allways */
	unsigned sb_rd_fnode : 2;	/* read fnode 0-no 1-dirs 2-all */
	unsigned sb_rd_inode : 2;	/* lookup tells read_inode: 1-read fnode
					   2-don't read fnode, file
					   3-don't read fnode, direcotry */
	wait_queue_head_t sb_iget_q;
	unsigned char *sb_cp_table;	/* code page tables: */
					/* 	128 bytes uppercasing table & */
					/*	128 bytes lowercasing table */
	unsigned *sb_bmp_dir;		/* main bitmap directory */
	unsigned sb_c_bitmap;		/* current bitmap */
	wait_queue_head_t sb_creation_de;/* when creating dirents, nobody else
					   can alloc blocks */
	unsigned sb_creation_de_lock : 1;
	/*unsigned sb_mounting : 1;*/
	int sb_timeshift;
};

#define s_hpfs_root u.hpfs_sb.sb_root
#define s_hpfs_fs_size u.hpfs_sb.sb_fs_size
#define s_hpfs_bitmaps u.hpfs_sb.sb_bitmaps
#define s_hpfs_dirband_start u.hpfs_sb.sb_dirband_start
#define s_hpfs_dirband_size u.hpfs_sb.sb_dirband_size
#define s_hpfs_dmap u.hpfs_sb.sb_dmap
#define s_hpfs_uid u.hpfs_sb.sb_uid
#define s_hpfs_gid u.hpfs_sb.sb_gid
#define s_hpfs_mode u.hpfs_sb.sb_mode
#define s_hpfs_n_free u.hpfs_sb.sb_n_free
#define s_hpfs_n_free_dnodes u.hpfs_sb.sb_n_free_dnodes
#define s_hpfs_lowercase u.hpfs_sb.sb_lowercase
#define s_hpfs_conv u.hpfs_sb.sb_conv
#define s_hpfs_eas u.hpfs_sb.sb_eas
#define s_hpfs_err u.hpfs_sb.sb_err
#define s_hpfs_chk u.hpfs_sb.sb_chk
#define s_hpfs_was_error u.hpfs_sb.sb_was_error
#define s_hpfs_chkdsk u.hpfs_sb.sb_chkdsk
/*#define s_hpfs_rd_fnode u.hpfs_sb.sb_rd_fnode*/
#define s_hpfs_rd_inode u.hpfs_sb.sb_rd_inode
#define s_hpfs_cp_table u.hpfs_sb.sb_cp_table
#define s_hpfs_bmp_dir u.hpfs_sb.sb_bmp_dir
#define s_hpfs_c_bitmap u.hpfs_sb.sb_c_bitmap
#define s_hpfs_creation_de u.hpfs_sb.sb_creation_de
#define s_hpfs_creation_de_lock u.hpfs_sb.sb_creation_de_lock
#define s_hpfs_iget_q u.hpfs_sb.sb_iget_q
/*#define s_hpfs_mounting u.hpfs_sb.sb_mounting*/
#define s_hpfs_timeshift u.hpfs_sb.sb_timeshift

#endif
