# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}/cp

PROG=	cc1plus
SRCS=	parse.c parse.h
SRCS+=	call.c class.c cvt.c decl.c decl2.c errfn.c error.c except.c expr.c \
	friend.c init.c lex.c method.c pt.c ptree.c repo.c rtti.c \
	search.c semantics.c sig.c spew.c tree.c typeck.c typeck2.c xref.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED=yes

CFLAGS+= -I${GCCDIR}/cp -I.

DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

.ORDER: parse.c parse.h
parse.c parse.h: parse.y
	${YACC} -d -o parse.c ${.ALLSRC}
	grep '^#define[   ]*YYEMPTY' parse.c >>parse.h

CLEANFILES+=	parse.c parse.h

CPPHDRS=	exception new new.h typeinfo

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${CPPHDRS:S;^;${GCCDIR}/cp/inc/;} \
	    ${DESTDIR}/usr/include/g++

.include <bsd.prog.mk>
