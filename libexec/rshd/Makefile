#	$Id: Makefile,v 1.5 1994/03/04 20:36:58 wollman Exp $
#	From: @(#)Makefile	5.6 (Berkeley) 9/27/90

PROG=	rshd
SRCS=	rshd.c
MAN8=	rshd.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
.PATH: ${.CURDIR}/../../usr.bin/rlogin

.if exists(${DESTDIR}/usr/lib/libcrypt.a)
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
.endif
.if exists(${DESTDIR}/usr/lib/libkrb.a)
DPADD+= ${LIBKRB} ${LIBDES}
LDADD+= -lkrb -ldes
CFLAGS+= -DKERBEROS -DCRYPT
.endif

.include <bsd.prog.mk>
