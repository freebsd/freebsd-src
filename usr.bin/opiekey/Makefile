# $FreeBSD: src/usr.bin/opiekey/Makefile,v 1.4.2.1 2000/05/14 21:15:05 obrien Exp $
#
OPIE_DIST?=	${.CURDIR}/../../contrib/opie

PROG=	opiekey
SRCS=	opiekey.c
MAN1=	opiekey.1

CFLAGS+= -I${OPIE_DIST}

DPADD=	${LIBOPIE} ${LIBMD}
LDADD=	-lopie -lmd

LINKS=	${BINDIR}/opiekey ${BINDIR}/otp-md4
LINKS+=	${BINDIR}/opiekey ${BINDIR}/otp-md5

MLINKS=	opiekey.1 otp-md4.1 opiekey.1 otp-md5.1

.PATH:	${OPIE_DIST}

.include <bsd.prog.mk>
