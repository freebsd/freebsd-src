/* $srcdir/conf/trap/trap_aux.h */
#define MOUNT_TRAP(type, mnt, flags, mnt_data) 	fsmount(type, mnt->mnt_dir, flags, mnt_data)
