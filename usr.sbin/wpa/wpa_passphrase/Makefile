# $FreeBSD: src/usr.sbin/wpa/wpa_passphrase/Makefile,v 1.3.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include "${.CURDIR}/../Makefile.inc"

.PATH.c:${WPA_SUPPLICANT_DISTDIR}

PROG=	wpa_passphrase
SRCS=	wpa_passphrase.c sha1.c md5.c

CFLAGS+= -DINTERNAL_SHA1
CFLAGS+= -DINTERNAL_MD5

MAN=	wpa_passphrase.8

.include <bsd.prog.mk>
