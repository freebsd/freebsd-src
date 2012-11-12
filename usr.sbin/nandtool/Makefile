# $FreeBSD$

PROG=	nandtool
SRCS=	nandtool.c nand_read.c nand_write.c nand_erase.c nand_info.c
SRCS+=	nand_readoob.c nand_writeoob.c
BINDIR=	/usr/sbin
DPADD=	${LIBGEOM}
LDADD=	-lgeom
MAN=	nandtool.8

.include <bsd.prog.mk>
