#	@(#)Makefile	8.1 (Berkeley) 6/1/93
# $Id: Makefile,v 1.4 1994/07/20 09:21:07 g89r4222 Exp $

PROG=	register
SRCS=	register.c
CFLAGS+=-DCRYPT -DDEBUG -DKERBEROS -I${.CURDIR}/../include
.PATH:	${.CURDIR}/../../usr.bin/rlogin
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes -lcrypt
BINDIR=	/usr/bin
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
