#	@(#)Makefile	8.1 (Berkeley) 6/28/93
# 	$Id: Makefile,v 1.6 1997/10/05 09:40:08 jkh Exp $

PROG=	fsinfo
MAN8=	fsinfo.8
SRCS=	fsinfo.c fsi_gram.y fsi_lex.l \
	fsi_util.c fsi_analyze.c fsi_dict.c \
	wr_atab.c wr_bparam.c wr_dumpset.c \
	wr_exportfs.c wr_fstab.c
CFLAGS+=-I.
CFLAGS+=-I${.CURDIR}/../include
CFLAGS+=-I${.CURDIR}/../config
CFLAGS+=-DOS_HDR=\"os-bsd44.h\"

.PATH: ${.CURDIR}/../config
.include "../../Makefile.inc"
.include <bsd.prog.mk>
