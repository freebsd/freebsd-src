#	@(#)Makefile	5.4 (Berkeley) 10/21/90

PROG=	rcp
SRCS=	rcp.c krcmd.c kcmd.c
BINOWN=	root
BINMODE=4555
.PATH:	${.CURDIR}/../../usr.bin/rlogin

.if exists(${DESTDIR}/usr/lib/libkrb.a)
CFLAGS+=-DCRYPT -DKERBEROS
DPADD+= ${LIBKRB} ${LIBDES}
LDADD+= -lkrb -ldes
.endif

.include <bsd.prog.mk>
