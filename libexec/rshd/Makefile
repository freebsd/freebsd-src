#	@(#)Makefile	5.6 (Berkeley) 9/27/90

PROG=	rshd
SRCS=	rshd.c
MAN8=	rshd.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
.PATH: ${.CURDIR}/../../usr.bin/rlogin

.if exists(/usr/lib/libcrypt.a)
#CFLAGS+=-DCRYPT -DKERBEROS
DPADD+= ${LIBCRYPT} #${LIBKRB}
LDADD+= -lcrypt #-lkrb
.endif


.include <bsd.prog.mk>
