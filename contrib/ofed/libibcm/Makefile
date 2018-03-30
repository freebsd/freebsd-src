# $FreeBSD$

.PATH: ${.CURDIR}

SHLIBDIR?=	/usr/lib
LIB=		ibcm
SHLIB_MAJOR=	1
MK_PROFILE=	no
SRCS=		cm.c
LIBADD=		ibverbs
CFLAGS+=	-I${.CURDIR}
MAN=
VERSION_MAP=	${.CURDIR}/libibcm.map

.include <bsd.lib.mk>
