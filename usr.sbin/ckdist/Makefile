# $Id: Makefile,v 1.1.1.1 1997/01/21 12:58:53 jkh Exp $

PROG=	ckdist

.PATH: ${.CURDIR}/../../usr.bin/cksum

SRCS=   ckdist.c crc.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
