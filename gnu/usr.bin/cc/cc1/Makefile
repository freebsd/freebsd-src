#
# $Id: Makefile,v 1.6 1995/09/22 14:14:21 phk Exp $
#

PROG =	cc1
SRCS =	c-aux-info.c c-convert.c c-decl.c c-iterate.c c-lang.c c-lex.c c-parse.c c-pragma.c c-typeck.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED= true
DPADD+=	${LIBCC_INT} 
LDADD+=	-lcc_int 

.include <bsd.prog.mk>
