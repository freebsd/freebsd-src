#	$Id$

.PATH:	${.CURDIR}/../../sys/i386/ibcs2
KMOD=	ibcs2_coff_mod
SRCS= 	coff.c imgact_coff.c 
VFS_LKM=
NOMAN=
CFLAGS+= -DLKM -I. -DCOMPAT_IBCS2

.include <bsd.kmod.mk>
