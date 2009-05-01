#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/tftp/Makefile,v 1.5.34.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	tftp
SRCS=	main.c tftp.c tftpsubs.c
DPADD=	${LIBEDIT} ${LIBTERMCAP}
LDADD=	-ledit -ltermcap

.include <bsd.prog.mk>
