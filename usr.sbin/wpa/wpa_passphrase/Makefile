# $FreeBSD$

.include "../Makefile.inc"

.PATH.c:${WPA_SUPPLICANT_DISTDIR}

PACKAGE=	wpa
PROG=	wpa_passphrase
SRCS=	wpa_passphrase.c

CFLAGS+= -DCONFIG_CRYPTO_INTERNAL -DINTERNAL_SHA1 -DINTERNAL_MD5

LIBADD+=	util wpacrypto wpautils

MAN=	wpa_passphrase.8

.include "../Makefile.crypto"

.include <bsd.prog.mk>
