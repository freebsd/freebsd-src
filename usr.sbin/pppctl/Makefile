# $FreeBSD: src/usr.sbin/pppctl/Makefile,v 1.13 2001/07/20 06:20:14 obrien Exp $

PROG=	pppctl
MAN=	pppctl.8

LDADD=	-lc_r -ledit -ltermcap
DPADD=	${LIBC_R} ${LIBEDIT} ${LIBTERMCAP}

.include <bsd.prog.mk>
