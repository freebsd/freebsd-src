#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/id/Makefile,v 1.11.8.1 2006/09/26 02:12:05 rwatson Exp $

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
