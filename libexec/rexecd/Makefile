#	@(#)Makefile	5.3 (Berkeley) 5/11/90

PROG=	rexecd
MAN8=	rexecd.8

.if exists(/usr/lib/libcrypt.a)
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
.endif

.include <bsd.prog.mk>
