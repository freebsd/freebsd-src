#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/id/Makefile,v 1.12.14.1 2010/12/21 17:10:29 kensmith Exp $

.include <bsd.own.mk>

PROG=	id
WARNS?=	6
LINKS=	${BINDIR}/id ${BINDIR}/groups
LINKS+=	${BINDIR}/id ${BINDIR}/whoami
MAN=	id.1 groups.1 whoami.1

.if ${MK_AUDIT} != "no"
CFLAGS+=	-DUSE_BSM_AUDIT
DPADD+=	${LIBBSM}
LDADD+=	-lbsm
.endif

.include <bsd.prog.mk>
