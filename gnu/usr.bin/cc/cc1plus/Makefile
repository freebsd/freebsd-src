# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}/cp ${GCCDIR}

PROG=	cc1plus
SRCS=	parse+%DIKED.c parse.h
SRCS+=	main.c cp-lang.c \
	call.c class.c cvt.c decl.c decl2.c error.c except.c expr.c friend.c \
	init.c lex.c mangle.c method.c pt.c ptree.c repo.c rtti.c search.c \
	semantics.c spew.c tree.c typeck.c typeck2.c dump.c optimize.c 

BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED?=yes

CFLAGS+= -I${GCCDIR}/cp -I.

DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

#-----------------------------------------------------------------------
# C++ parser

parse+%DIKED.c y.tab.h: parse.c
	sed -e "s/malloc/xmalloc/g" \
	    -e "s/realloc/xrealloc/g" \
	    ${.ALLSRC:M*c} > ${.TARGET}

parse.h: y.tab.h
	cp -pf y.tab.h ${.TARGET}
	grep '^#define[   ]*YYEMPTY' ${.TARGET:S/h$/c/} >>${.TARGET}

CLEANFILES=	parse+%DIKED.c parse.c parse.h y.tab.h

.include <bsd.prog.mk>
