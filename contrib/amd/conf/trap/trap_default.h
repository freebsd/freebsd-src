/* $FreeBSD: src/contrib/amd/conf/trap/trap_default.h,v 1.3.8.1.2.1 2008/10/02 02:57:24 kensmith Exp $ */
/* $srcdir/conf/trap/trap_default.h */
#define MOUNT_TRAP(type, mnt, flags, mnt_data) mount(type, mnt->mnt_dir, flags, mnt_data)
