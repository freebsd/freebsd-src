/* $FreeBSD: src/contrib/amd/conf/trap/trap_default.h,v 1.4.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $ */
/* $srcdir/conf/trap/trap_default.h */
#define MOUNT_TRAP(type, mnt, flags, mnt_data) mount(type, mnt->mnt_dir, flags, mnt_data)
