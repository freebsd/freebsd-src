#	@(#)Makefile	5.9 (Berkeley) 9/27/90

PROG=	rlogind
SRCS=	rlogind.c
MAN8=	rlogind.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
.PATH: ${.CURDIR}/../../usr.bin/rlogin

.if exists(/usr/lib/libcrypt.a)
#CFLAGS+=-DCRYPT -DKERBEROS
DPADD+= ${LIBCRYPT} #${LIBKRB}
LDADD+= -lcrypt #-lkrb
.endif

.include <bsd.prog.mk>
