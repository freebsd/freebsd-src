# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/amd/fsinfo

PROG=	fsinfo
MAN=	fsinfo.8
SRCS=	fsi_gram.y fsi_lex.l
SRCS+=	fsi_analyze.c fsi_dict.c fsi_util.c fsinfo.c wr_atab.c 
SRCS+=	wr_bparam.c wr_dumpset.c wr_exportfs.c wr_fstab.c

NO_PIE=	yes

CFLAGS+= -I${.CURDIR}/../../../contrib/amd/fsinfo

DPADD=	${LIBAMU}
LDADD=	${LIBAMU}

CLEANFILES+=	fsi_gram.c fsi_gram.h fsi_lex.c

fsi_lex.o: fsi_gram.h

.include <bsd.prog.mk>
