
#	@(#)Makefile	5.6 (Berkeley) 3/5/91
#

PROG=	keyinit
MAN1=   keyinit.1
CFLAGS+=-I${.CURDIR}/../../lib
DPADD=	/usr/bin/libskey.a
LDADD=	-lskey

.if exists(/usr/lib/libcrypt.a)
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
.endif

SRCS=	skeyinit.c

BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
