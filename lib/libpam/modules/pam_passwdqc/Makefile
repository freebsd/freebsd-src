# $FreeBSD: src/lib/libpam/modules/pam_passwdqc/Makefile,v 1.5 2002/04/15 03:45:14 des Exp $

SRCDIR=		${.CURDIR}/../../../../contrib/pam_modules/pam_passwdqc

LIB=		pam_passwdqc
SRCS=		pam_passwdqc.c passwdqc_check.c passwdqc_random.c wordset_4k.c
CFLAGS+=	-I${SRCDIR}
NO_WERROR=	yes
DPADD=		${LIBCRYPT}
LDADD=		-lcrypt
MAN=		pam_passwdqc.8

.include <bsd.lib.mk>

.PATH:		${SRCDIR}
