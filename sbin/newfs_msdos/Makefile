# $FreeBSD$

PROG=	newfs_msdos
MAN8=	newfs_msdos.8

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
