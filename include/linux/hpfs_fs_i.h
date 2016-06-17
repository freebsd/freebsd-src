#ifndef _HPFS_FS_I
#define _HPFS_FS_I

struct hpfs_inode_info {
	unsigned long mmu_private;
	ino_t i_parent_dir;	/* (directories) gives fnode of parent dir */
	unsigned i_dno;		/* (directories) root dnode */
	unsigned i_dpos;	/* (directories) temp for readdir */
	unsigned i_dsubdno;	/* (directories) temp for readdir */
	unsigned i_file_sec;	/* (files) minimalist cache of alloc info */
	unsigned i_disk_sec;	/* (files) minimalist cache of alloc info */
	unsigned i_n_secs;	/* (files) minimalist cache of alloc info */
	unsigned i_ea_size;	/* size of extended attributes */
	unsigned i_conv : 2;	/* (files) crlf->newline hackery */
	unsigned i_ea_mode : 1;	/* file's permission is stored in ea */
	unsigned i_ea_uid : 1;	/* file's uid is stored in ea */
	unsigned i_ea_gid : 1;	/* file's gid is stored in ea */
	unsigned i_dirty : 1;
	struct semaphore i_sem;	/* semaphore */
	loff_t **i_rddir_off;
};

#define i_hpfs_dno u.hpfs_i.i_dno
#define i_hpfs_parent_dir u.hpfs_i.i_parent_dir
#define i_hpfs_n_secs u.hpfs_i.i_n_secs
#define i_hpfs_file_sec u.hpfs_i.i_file_sec
#define i_hpfs_disk_sec u.hpfs_i.i_disk_sec
#define i_hpfs_dpos u.hpfs_i.i_dpos
#define i_hpfs_dsubdno u.hpfs_i.i_dsubdno
#define i_hpfs_ea_size u.hpfs_i.i_ea_size
#define i_hpfs_conv u.hpfs_i.i_conv
#define i_hpfs_ea_mode u.hpfs_i.i_ea_mode
#define i_hpfs_ea_uid u.hpfs_i.i_ea_uid
#define i_hpfs_ea_gid u.hpfs_i.i_ea_gid
/*#define i_hpfs_lock u.hpfs_i.i_lock*/
/*#define i_hpfs_queue u.hpfs_i.i_queue*/
#define i_hpfs_sem u.hpfs_i.i_sem
#define i_hpfs_rddir_off u.hpfs_i.i_rddir_off
#define i_hpfs_dirty u.hpfs_i.i_dirty

#endif
