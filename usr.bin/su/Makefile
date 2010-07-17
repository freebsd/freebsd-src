#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/usr.bin/su/Makefile,v 1.41.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

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
