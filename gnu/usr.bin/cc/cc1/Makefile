#
# $FreeBSD$
#

PROG =	cc1
SRCS =	c-aux-info.c c-convert.c c-decl.c c-iterate.c c-lang.c c-lex.c c-parse.c c-pragma.c c-typeck.c
BINDIR=	/usr/libexec
NOMAN=	1
LDDESTDIR+=	-L${.CURDIR}/../cc_int/obj
LDDESTDIR+=	-L${.CURDIR}/../cc_int
LDADD+=	-lcc_int

.include <bsd.prog.mk>
