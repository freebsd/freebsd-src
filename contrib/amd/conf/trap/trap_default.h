/* $FreeBSD: src/contrib/amd/conf/trap/trap_default.h,v 1.3.18.1.6.1 2010/02/10 00:26:20 kensmith Exp $ */
/* $srcdir/conf/trap/trap_default.h */
#define MOUNT_TRAP(type, mnt, flags, mnt_data) mount(type, mnt->mnt_dir, flags, mnt_data)
