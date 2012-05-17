# $FreeBSD$

PROG=	nandfs
SRCS=	nandfs.c lssnap.c mksnap.c rmsnap.c
MAN=	nandfs.8

DPADD=	${LIBNANDFS}
LDADD=	-lnandfs

.include <bsd.prog.mk>
