
#	@(#)Makefile	5.6 (Berkeley) 3/5/91
#

PROG=	key
MAN1=   key.1 skey.1
CFLAGS+=-I${.CURDIR}/../../lib


DPADD=	/usr/bin/libskey.a
LDADD=	-lskey

.if exists(/usr/lib/libcrypt.a)
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
.endif

SRCS=	skey.c

.include <bsd.prog.mk>

