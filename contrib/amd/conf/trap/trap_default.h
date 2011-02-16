/* $FreeBSD: src/contrib/amd/conf/trap/trap_default.h,v 1.4.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $ */
/* $srcdir/conf/trap/trap_default.h */
#define MOUNT_TRAP(type, mnt, flags, mnt_data) mount(type, mnt->mnt_dir, flags, mnt_data)
