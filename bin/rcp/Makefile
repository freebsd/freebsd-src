#	@(#)Makefile	5.4 (Berkeley) 10/21/90

PROG=	rcp
SRCS=	rcp.c 
BINOWN=	root
BINMODE=4555
.PATH:	${.CURDIR}/../../usr.bin/rlogin

.if exists(/usr/lib/libcrypt.a)
#CFLAGS+=-DCRYPT -DKERBEROS
#DPADD+= ${LIBCRYPT} ${LIBKRB}
#LDADD+= -lcrypt -lkrb
.endif

.include <bsd.prog.mk>
