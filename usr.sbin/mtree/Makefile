#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.6 1997/02/22 16:07:51 peter Exp $

PROG=	mtree
SRCS=	compare.c crc.c create.c misc.c mtree.c spec.c verify.c
MAN8=	mtree.8
.PATH:	${.CURDIR}/../../usr.bin/cksum

DPADD+=	${LIBMD}
LDADD+=	-lmd
CFLAGS+= -DMD5 -DSHA1 -DRMD160

.include <bsd.prog.mk>
