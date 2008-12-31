#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/usr.bin/su/Makefile,v 1.41.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

PROG=	su

DPADD=	${LIBUTIL} ${LIBPAM}
LDADD=	-lutil ${MINUSLPAM}

.if ${MK_AUDIT} != "no"
CFLAGS+= -DUSE_BSM_AUDIT
DPADD+=  ${LIBBSM}
LDADD+=  -lbsm
.endif

BINOWN=	root
BINMODE=4555
PRECIOUSPROG=

.include <bsd.prog.mk>
