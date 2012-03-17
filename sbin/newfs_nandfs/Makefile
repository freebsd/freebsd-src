# $FreeBSD$

PROG=	newfs_nandfs
MAN=	newfs_nandfs.8
.if ${MACHINE_ARCH} == "arm"
WARNS?= 3
.else
WARNS?=	6
.endif

LDADD+= -lgeom

.include <bsd.prog.mk>
