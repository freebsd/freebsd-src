# $Id: Makefile,v 1.1 1997/01/14 14:50:52 rnordier Exp $

PROG=	ckdist

.PATH: ${.CURDIR}/../../usr.bin/cksum

SRCS=   ckdist.c crc.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
