# $FreeBSD: src/usr.sbin/pppctl/Makefile,v 1.14 2003/04/04 17:49:17 obrien Exp $

PROG=	pppctl
MAN=	pppctl.8

DPADD=	${LIBC_R} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lc_r -ledit -ltermcap

.include <bsd.prog.mk>
