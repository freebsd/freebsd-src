# $FreeBSD: src/kerberos5/tools/make-roken/Makefile,v 1.2.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	make-roken
NO_MAN=

CLEANFILES= make-roken.c

.include <bsd.prog.mk>

make-roken.c: ${KRB5DIR}/lib/roken/roken.awk ${KRB5DIR}/lib/roken/roken.h.in
	awk -f ${.ALLSRC} > ${.TARGET}
