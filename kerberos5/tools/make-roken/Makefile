# $FreeBSD: src/kerberos5/tools/make-roken/Makefile,v 1.2.26.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	make-roken
NO_MAN=

CLEANFILES= make-roken.c

.include <bsd.prog.mk>

make-roken.c: ${KRB5DIR}/lib/roken/roken.awk ${KRB5DIR}/lib/roken/roken.h.in
	awk -f ${.ALLSRC} > ${.TARGET}
