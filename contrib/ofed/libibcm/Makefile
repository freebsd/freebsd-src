# $FreeBSD$

.PATH: ${.CURDIR}

SHLIBDIR?=	/usr/lib
LIB=		ibcm
SHLIB_MAJOR=	1
MK_PROFILE=	no
SRCS=		cm.c
.if defined(TESTBUILD)
LDFLAGS=	-libverbs
.else
LDADD=		ibverbs
.endif
CFLAGS+=	-I${.CURDIR}
MAN=
VERSION_MAP=	${.CURDIR}/libibcm.map

.include <bsd.lib.mk>
