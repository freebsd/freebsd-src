#
# $Id$
#
 
PROG =	cc1
SRCS =	c-parse.c \
	c-aux-info.c c-convert.c c-decl.c c-iterate.c c-lang.c c-lex.c \
	c-typeck.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED= true
DPADD+=	${LIBCC_INT} 
LDADD+=	-lcc_int 

.include <bsd.prog.mk>
