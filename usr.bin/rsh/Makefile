#	@(#)Makefile	5.6 (Berkeley) 9/27/90

PROG=	rsh
SRCS=	rsh.c
BINOWN=	root
BINMODE=4555
.PATH:	${.CURDIR}/../rlogin

.if exists(/usr/lib/libcrypt.a)
#CFLAGS+=-DCRYPT -DKERBEROS
DPADD+= ${LIBCRYPT} #${LIBKRB}
LDADD+= -lcrypt #-lkrb
.endif

.include <bsd.prog.mk>
