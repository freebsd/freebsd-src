# $FreeBSD: src/kerberos5/tools/make-roken/Makefile,v 1.2.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	make-roken
NO_MAN=

CLEANFILES= make-roken.c

.include <bsd.prog.mk>

make-roken.c: ${KRB5DIR}/lib/roken/roken.awk ${KRB5DIR}/lib/roken/roken.h.in
	awk -f ${.ALLSRC} > ${.TARGET}
