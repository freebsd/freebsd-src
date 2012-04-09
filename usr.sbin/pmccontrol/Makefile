#
# $FreeBSD: src/usr.sbin/pmccontrol/Makefile,v 1.2.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $
#

PROG=	pmccontrol
MAN=	pmccontrol.8

DPADD=	${LIBPMC}
LDADD=	-lpmc

WARNS?=	6

SRCS=	pmccontrol.c

.include <bsd.prog.mk>
