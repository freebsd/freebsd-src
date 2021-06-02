# $FreeBSD$

.include "../Makefile.inc"

.include "../Makefile.crypto"

.PATH.c:${HOSTAPD_DISTDIR}

PACKAGE=	hostapd
PROG=	hostapd_cli
SRCS=	hostapd_cli.c

LIBADD+=	util wpacommon wpautils

MAN=	hostapd_cli.8

.include <bsd.prog.mk>
