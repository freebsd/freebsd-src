#
# $FreeBSD$
#

PROG =	cc1plus
SRCS =	call.c class.c cvt.c decl.c decl2.c edsel.c errfn.c error.c except.c expr.c gc.c init.c lex.c method.c parse.c pt.c ptree.c search.c sig.c spew.c tree.c typeck.c typeck2.c xref.c
BINDIR=	/usr/libexec
NOMAN=	1
DPADD+=	${LIBCC_INT}
LDADD+=	-lcc_int

.include <bsd.prog.mk>
