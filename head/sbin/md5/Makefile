#	@(#)Makefile	8.1 (Berkeley) 6/9/93
# $FreeBSD$

PROG=	md5

LINKS=	${BINDIR}/md5 ${BINDIR}/rmd160 \
	${BINDIR}/md5 ${BINDIR}/sha1 \
	${BINDIR}/md5 ${BINDIR}/sha256 \
	${BINDIR}/md5 ${BINDIR}/sha512

MLINKS=	md5.1 rmd160.1 \
	md5.1 sha1.1 \
	md5.1 sha256.1 \
	md5.1 sha512.1

NO_WMISSING_VARIABLE_DECLARATIONS=
WFORMAT?=	1

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
