/* $srcdir/conf/trap/trap_freebsd3.h */
extern int mount_freebsd3(MTYPE_TYPE type, const char *dir, int flags, voidp data);
#define MOUNT_TRAP(type, mnt, flags, mnt_data) mount_freebsd3(type, mnt->mnt_dir, flags, mnt_data)
