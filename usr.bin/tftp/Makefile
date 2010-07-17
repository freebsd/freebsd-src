#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/tftp/Makefile,v 1.5.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	tftp
SRCS=	main.c tftp.c tftpsubs.c
DPADD=	${LIBEDIT} ${LIBTERMCAP}
LDADD=	-ledit -ltermcap

.include <bsd.prog.mk>
