#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/tftp/Makefile,v 1.5.30.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	tftp
SRCS=	main.c tftp.c tftpsubs.c
DPADD=	${LIBEDIT} ${LIBTERMCAP}
LDADD=	-ledit -ltermcap

.include <bsd.prog.mk>
