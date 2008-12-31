# $FreeBSD: src/kerberos5/tools/make-roken/Makefile,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	make-roken
NO_MAN=

CLEANFILES= make-roken.c

.include <bsd.prog.mk>

make-roken.c: ${KRB5DIR}/lib/roken/roken.awk ${KRB5DIR}/lib/roken/roken.h.in
	awk -f ${.ALLSRC} > ${.TARGET}
