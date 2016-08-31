/* $srcdir/conf/trap/trap_rtu6.h */
#define	MOUNT_TRAP(type, mnt, flags, mnt_data) 	vmount(type, mnt->mnt_dir, flags, mnt_data)
