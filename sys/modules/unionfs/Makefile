#	$Id: Makefile,v 1.3 1997/02/22 12:49:27 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/union
KMOD=	union
SRCS=	union_subr.c union_vfsops.c union_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+=	-DUNION

.include <bsd.kmod.mk>
