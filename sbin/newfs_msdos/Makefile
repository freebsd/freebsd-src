# $FreeBSD$

PROG=	newfs_msdos
MAN=	newfs_msdos.8

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
