#
# $FreeBSD: src/usr.sbin/auditreduce/Makefile,v 1.1.2.2.6.1 2008/10/02 02:57:24 kensmith Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/auditreduce

CFLAGS+=	-I${OPENBSMDIR}

PROG=	auditreduce
MAN=	auditreduce.1

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
