# $FreeBSD$
#
OPIE_DIST?=	${.CURDIR}/../../contrib/opie

PROG=	opiekey

CFLAGS+= -I${OPIE_DIST}

.if defined(WANT_INSECURE_OPIE) && ${WANT_INSECURE_OPIE} == true
CFLAGS+=-DINSECURE_OVERRIDE
.endif

DPADD=	${LIBOPIE} ${LIBMD}
LDADD=	-lopie -lmd

LINKS=	${BINDIR}/opiekey ${BINDIR}/otp-md4
LINKS+=	${BINDIR}/opiekey ${BINDIR}/otp-md5
LINKS+=	${BINDIR}/opiekey ${BINDIR}/otp-sha

MLINKS=	opiekey.1 otp-md4.1 opiekey.1 otp-md5.1 opiekey.1 otp-sha.1

.PATH:	${OPIE_DIST}

.include <bsd.prog.mk>
