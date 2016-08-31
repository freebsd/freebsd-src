/* $srcdir/conf/trap/trap_hcx.h */
#define	MOUNT_TRAP(type, mnt, flags, mnt_data) 	mountsyscall(type, mnt->mnt_dir, flags, mnt_data)
