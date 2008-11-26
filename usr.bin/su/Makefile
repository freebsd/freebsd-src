#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/usr.bin/su/Makefile,v 1.40.2.1.6.1 2008/10/02 02:57:24 kensmith Exp $

.include <bsd.own.mk>

PROG=	su

DPADD=	${LIBUTIL} ${LIBPAM}
LDADD=	-lutil ${MINUSLPAM}

.if !defined(NO_AUDIT)
CFLAGS+= -DUSE_BSM_AUDIT
DPADD+=  ${LIBBSM}
LDADD+=  -lbsm
.endif

BINOWN=	root
BINMODE=4555
PRECIOUSPROG=

.include <bsd.prog.mk>
