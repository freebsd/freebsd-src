#	@(#)Makefile	5.1 (Berkeley) 4/21/91

PROG=	bdes

.if exists(/usr/lib/libcrypt.a)
CFLAGS+=-DDES
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
.endif

.include <bsd.prog.mk>
