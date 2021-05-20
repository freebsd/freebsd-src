# $FreeBSD$

.include "../Makefile.inc"

.PATH.c:${HOSTAPD_DISTDIR}

PACKAGE=	hostapd
PROG=	hostapd_cli
SRCS=	hostapd_cli.c

LIBADD+=	util wpacommon wpautils

MAN=	hostapd_cli.8

.include "../Makefile.crypto"

.include <bsd.prog.mk>
