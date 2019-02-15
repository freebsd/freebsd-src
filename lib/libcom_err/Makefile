# $FreeBSD$

PACKAGE=lib${LIB}
LIB=	com_err
PRIVATELIB=	true
SRCS=	com_err.c error.c
INCS=	com_err.h com_right.h
MAN=	com_err.3
COM_ERRDIR=	${SRCTOP}/contrib/com_err
CFLAGS+=	-I${COM_ERRDIR}

LDFLAGS=	-Wl,--no-undefined
VERSION_MAP=	${COM_ERRDIR}/version-script.map

.include <bsd.lib.mk>

.PATH:	${COM_ERRDIR}
