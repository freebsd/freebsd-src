#
# $Id: Makefile,v 1.6 1995/09/22 14:14:26 phk Exp $
#

PROG =	cc1plus
SRCS =	call.c class.c cvt.c decl.c decl2.c edsel.c errfn.c error.c except.c expr.c gc.c init.c lex.c method.c parse.c pt.c ptree.c search.c sig.c spew.c tree.c typeck.c typeck2.c xref.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED=	makes_it_smaller_faster
DPADD+=	${LIBCC_INT} 
LDADD+=	-lcc_int 

.include <bsd.prog.mk>
