#	@(#)Makefile	8.1 (Berkeley) 6/4/93
#	$FreeBSD$

PROG=	rexecd
MAN8=	rexecd.8
CFLAGS+= -DSKEY

DPADD=  ${LIBSKEY} ${LIBMD} ${LIBCRYPT}
LDADD=	-lskey -lmd -lcrypt

.include <bsd.prog.mk>
