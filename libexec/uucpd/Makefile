#	@(#)Makefile	5.3 (Berkeley) 6/25/90

PROG=	uucpd
NOMAN=	noman

.if exists(/usr/lib/libcrypt.a)
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
.endif

.include <bsd.prog.mk>
