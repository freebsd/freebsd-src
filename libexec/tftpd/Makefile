#	@(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.6 1999/04/06 23:05:59 brian Exp $

PROG=	tftpd
SRCS=	tftpd.c tftpsubs.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN8=	tftpd.8
CFLAGS+=-I${.CURDIR}/../../usr.bin/tftp
.PATH:	${.CURDIR}/../../usr.bin/tftp

.include <bsd.prog.mk>
