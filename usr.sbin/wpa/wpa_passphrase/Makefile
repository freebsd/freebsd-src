# $FreeBSD$

WPA_SUPPLICANT_DISTDIR?= ${.CURDIR}/../../../contrib/wpa_supplicant
.PATH: ${WPA_SUPPLICANT_DISTDIR}

PROG=	wpa_passphrase
SRCS=	wpa_passphrase.c sha1.c md5.c

CFLAGS+= -DINTERNAL_SHA1
CFLAGS+= -DINTERNAL_MD5

MAN=	wpa_passphrase.8

.include <bsd.prog.mk>
