#	@(#)Makefile	5.3 (Berkeley) 5/11/90

PROG=	makekey
MAN8=	makekey.8

.if exists(/usr/lib/libcrypt.a)
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
.endif

.include <bsd.prog.mk>
