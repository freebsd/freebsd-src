/* $FreeBSD: src/contrib/amd/conf/trap/trap_default.h,v 1.4.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $ */
/* $srcdir/conf/trap/trap_default.h */
#define MOUNT_TRAP(type, mnt, flags, mnt_data) mount(type, mnt->mnt_dir, flags, mnt_data)
