# $FreeBSD$

PACKAGE=runtime
PROG=	newfs_msdos
MAN=	newfs_msdos.8
SRCS=	newfs_msdos.c mkfs_msdos.c

# XXX - this is verboten
.if ${MACHINE_CPUARCH} == "arm"
WARNS?= 3
.endif
CSTD=	c11

.include <bsd.prog.mk>
