#	@(#)Makefile	8.1 (Berkeley) 7/19/93

PROG=	rcp
SRCS=	rcp.c krcmd.c kcmd.c util.c
CFLAGS+=-DKERBEROS -DCRYPT
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes
BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg
.PATH:	${.CURDIR}/../../usr.bin/rlogin

.include <bsd.prog.mk>
