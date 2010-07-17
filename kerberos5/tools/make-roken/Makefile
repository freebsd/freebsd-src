# $FreeBSD: src/kerberos5/tools/make-roken/Makefile,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	make-roken
NO_MAN=

CLEANFILES= make-roken.c

.include <bsd.prog.mk>

make-roken.c: ${KRB5DIR}/lib/roken/roken.awk ${KRB5DIR}/lib/roken/roken.h.in
	awk -f ${.ALLSRC} > ${.TARGET}
