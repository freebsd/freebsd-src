/* $srcdir/conf/trap/trap_netbsd.h */
#if __NetBSD_Version__ >= 499002300
#define MOUNT_TRAP(type, mnt, flags, mnt_data) mount(type, mnt->mnt_dir, flags, mnt_data, 0)
#else
#define MOUNT_TRAP(type, mnt, flags, mnt_data) mount(type, mnt->mnt_dir, flags, mnt_data)
#endif
