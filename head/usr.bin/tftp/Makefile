#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	tftp
SRCS=	main.c tftp.c tftpsubs.c
DPADD=	${LIBEDIT} ${LIBTERMCAP}
LDADD=	-ledit -ltermcap

.include <bsd.prog.mk>
