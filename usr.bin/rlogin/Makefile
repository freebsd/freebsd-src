#	@(#)Makefile	5.6 (Berkeley) 9/27/90

PROG=	rlogin
SRCS=	rlogin.c
BINOWN=	root
BINMODE=4555

.if exists(/usr/lib/libcrypt.a)
#CFLAGS+=-DCRYPT -DKERBEROS
DPADD+= ${LIBCRYPT} #${LIBKRB}
LDADD+= -lcrypt #-lkrb
.endif

.include <bsd.prog.mk>
