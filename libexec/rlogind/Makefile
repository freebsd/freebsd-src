#	$Id: Makefile,v 1.5 1994/03/04 20:32:55 wollman Exp $
#	From: @(#)Makefile	5.9 (Berkeley) 9/27/90

PROG=	rlogind
SRCS=	rlogind.c
MAN8=	rlogind.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
.PATH: ${.CURDIR}/../../usr.bin/rlogin

.if exists(${DESTDIR}/usr/lib/libcrypt.a)
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
.endif
.if exists(${DESTDIR}/usr/lib/libkrb.a)
CFLAGS+=	-DKERBEROS
DPADD+= ${LIBKRB} ${LIBDES}
LDADD+= -lkrb -ldes
.endif

.include <bsd.prog.mk>
