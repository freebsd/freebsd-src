#	@(#)Makefile	8.1 (Berkeley) 6/4/93

PROG=	rexecd
MAN8=	rexecd.8
CFLAGS+= -DSKEY

DPADD=	${LIBCRYPT} ${LIBSKEY}
LDADD=	-lcrypt -lskey

.include <bsd.prog.mk>
