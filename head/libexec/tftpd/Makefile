#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

PROG=	tftpd
SRCS=	tftpd.c tftpsubs.c
DPADD=	${LIBUTIL}
LDADD=	-lutil

WARNS?=	1
WFORMAT=0

MAN=	tftpd.8
CFLAGS+=-I${.CURDIR}/../../usr.bin/tftp
.PATH:	${.CURDIR}/../../usr.bin/tftp

.include <bsd.prog.mk>
