#	@(#)Makefile	8.1 (Berkeley) 6/1/93
# $Id: Makefile,v 1.5 1995/07/18 16:41:22 mark Exp $

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
