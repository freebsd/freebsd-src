#
# $FreeBSD: src/usr.sbin/auditd/Makefile,v 1.1.10.1 2009/04/15 03:14:26 kensmith Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/auditd

PROG=	auditd
SRCS=	auditd.c audit_warn.c
MAN=	auditd.8

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
