#	@(#)Makefile	8.1 (Berkeley) 6/28/93
# 	$Id: $

PROG=	fsinfo
MAN8=	fsinfo.8
SRCS=	fsinfo.c fsi_gram.c fsi_lex.c \
	fsi_util.c fsi_analyze.c fsi_dict.c \
	wr_atab.c wr_bparam.c wr_dumpset.c \
	wr_exportfs.c wr_fstab.c
CLEANFILES= \
	fsi_gram.c y.tab.c fsi_gram.h y.tab.h \
	fsi_lex.c lex.yy.c y.output
CFLAGS+=-I.
CFLAGS+=-I${.CURDIR}/../include
CFLAGS+=-I${.CURDIR}/../config
CFLAGS+=-DOS_HDR=\"os-bsd44.h\"

fsi_lex.o fsinfo.o: fsi_gram.h
fsi_gram.c fsi_gram.h: ../fsinfo/fsi_gram.y
	@echo "# expect 2 shift/reduce conflicts"
	${YACC} -d ${.CURDIR}/fsi_gram.y
	mv -f y.tab.c fsi_gram.c
	mv -f y.tab.h fsi_gram.h

fsi_lex.c: ../fsinfo/fsi_lex.l
	${LEX} ${.CURDIR}/fsi_lex.l
	mv -f lex.yy.c fsi_lex.c

.PATH: ${.CURDIR}/../config
.include "../../Makefile.inc"
.include <bsd.prog.mk>
