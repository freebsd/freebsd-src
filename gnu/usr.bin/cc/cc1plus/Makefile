#
# $Id: Makefile,v 1.9 1996/09/21 14:27:32 peter Exp $
#

#First, so that we get cp/tree.c and cp/expr.c instead of the C version
.PATH:	${.CURDIR}/../../../../contrib/gcc/cp

PROG =	cc1plus
SRCS =	parse.c \
	call.c class.c cvt.c decl.c decl2.c edsel.c errfn.c \
	error.c except.c expr.c gc.c init.c lex.c method.c pt.c \
	ptree.c repo.c search.c sig.c spew.c tree.c typeck.c typeck2.c xref.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED=yes
DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}
CFLAGS+= -I.	# I mean it.

parse.c parse.h: parse.y
	${BISON} -d ${GCCDIR}/cp/parse.y -o parse.c 
	grep '^#define[   ]*YYEMPTY' parse.c >>parse.h

CLEANFILES+=	parse.c parse.h

.include <bsd.prog.mk>
