/* $FreeBSD: src/contrib/amd/conf/trap/trap_default.h,v 1.3 2004/07/06 14:14:26 mbr Exp $ */
/* $srcdir/conf/trap/trap_default.h */
#define MOUNT_TRAP(type, mnt, flags, mnt_data) mount(type, mnt->mnt_dir, flags, mnt_data)
