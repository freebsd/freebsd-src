#	@(#)Makefile	5.6 (Berkeley) 3/5/91
#
PROG=	keyinit
MAN1=   keyinit.1
SRCS=	skeyinit.c

CFLAGS+= -D_SKEY_INTERNAL

BINOWN=	root
BINMODE=4555

DPADD=	${LIBSKEY} ${LIBMD}
LDADD=	-lskey -lmd

.include <bsd.prog.mk>
