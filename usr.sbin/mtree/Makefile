#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$FreeBSD$

PROG=	mtree
SRCS=	compare.c crc.c create.c misc.c mtree.c spec.c verify.c
MAN8=	mtree.8
.PATH:	${.CURDIR}/../../usr.bin/cksum

DPADD+=	${LIBMD}
LDADD+=	-lmd

.include <bsd.prog.mk>
