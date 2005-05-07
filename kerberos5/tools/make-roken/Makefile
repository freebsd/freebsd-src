# $FreeBSD: src/kerberos5/tools/make-roken/Makefile,v 1.1 2004/01/31 08:15:56 ru Exp $

PROG=	make-roken
NOMAN=

CLEANFILES= make-roken.c

.include <bsd.prog.mk>

make-roken.c: ${KRB5DIR}/lib/roken/roken.awk ${KRB5DIR}/lib/roken/roken.h.in
	awk -f ${.ALLSRC} > ${.TARGET}
