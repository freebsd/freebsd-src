#	$Id: Makefile,v 1.4 1998/10/16 04:30:50 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/union
KMOD=	union
SRCS=	union_subr.c union_vfsops.c union_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+=	-DUNION

.include <bsd.kmod.mk>
