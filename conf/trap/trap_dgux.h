/* $srcdir/conf/trap/trap_dgux.h */
#define MOUNT_TRAP(type, mnt, flags, mnt_data) 	mount_dgux(type, mnt->mnt_dir, flags, mnt_data)
