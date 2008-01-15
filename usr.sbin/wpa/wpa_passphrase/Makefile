# $FreeBSD: src/usr.sbin/wpa/wpa_passphrase/Makefile,v 1.1.2.1 2006/12/23 22:44:47 sam Exp $

WPA_SUPPLICANT_DISTDIR?= ${.CURDIR}/../../../contrib/wpa_supplicant
.PATH: ${WPA_SUPPLICANT_DISTDIR}

PROG=	wpa_passphrase
SRCS=	wpa_passphrase.c sha1.c md5.c

MAN=	wpa_passphrase.8

.include <bsd.prog.mk>
