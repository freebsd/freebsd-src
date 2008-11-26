#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/id/Makefile,v 1.11.8.1.6.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	id
WARNS?=	6
LINKS=	${BINDIR}/id ${BINDIR}/groups
LINKS+=	${BINDIR}/id ${BINDIR}/whoami
MAN=	id.1 groups.1 whoami.1

.if !defined(NO_AUDIT)
CFLAGS+=	-DUSE_BSM_AUDIT
DPADD+=	${LIBBSM}
LDADD+=	-lbsm
.endif

.include <bsd.prog.mk>
