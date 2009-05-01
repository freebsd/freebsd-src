/* $FreeBSD: src/contrib/amd/conf/trap/trap_default.h,v 1.3.18.1.4.1 2009/04/15 03:14:26 kensmith Exp $ */
/* $srcdir/conf/trap/trap_default.h */
#define MOUNT_TRAP(type, mnt, flags, mnt_data) mount(type, mnt->mnt_dir, flags, mnt_data)
