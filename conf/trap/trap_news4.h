/* $srcdir/conf/trap/trap_news4.h */
#define MOUNT_TRAP(type, mnt, flags, mnt_data)         mount(type, mnt->mnt_dir, M_NEWTYPE | flags, mnt_data)
