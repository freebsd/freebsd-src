# $FreeBSD: src/usr.bin/opiekey/Makefile,v 1.4.2.2 2001/04/25 11:29:31 ru Exp $
#
OPIE_DIST?=	${.CURDIR}/../../contrib/opie

PROG=	opiekey

CFLAGS+= -I${OPIE_DIST}

DPADD=	${LIBOPIE} ${LIBMD}
LDADD=	-lopie -lmd

LINKS=	${BINDIR}/opiekey ${BINDIR}/otp-md4
LINKS+=	${BINDIR}/opiekey ${BINDIR}/otp-md5

MLINKS=	opiekey.1 otp-md4.1 opiekey.1 otp-md5.1

.PATH:	${OPIE_DIST}

.include <bsd.prog.mk>
