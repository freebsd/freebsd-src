#
# $FreeBSD: src/usr.sbin/auditd/Makefile,v 1.1.2.1.6.1 2008/10/02 02:57:24 kensmith Exp $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/auditd

PROG=	auditd
SRCS=	auditd.c audit_warn.c
MAN=	auditd.8

DPADD=	${LIBBSM}
LDADD=	-lbsm

.include <bsd.prog.mk>
