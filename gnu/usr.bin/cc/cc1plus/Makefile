# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}/cp ${GCCDIR}

PROG=	cc1plus
SRCS=	parse+%DIKED.c parse.h cfns.h
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

cfns.h: cfns.gperf
	gperf -o -C -E -k '1-6,$$' -j1 -D -N 'libc_name_p' \
	    ${.ALLSRC} > ${.TARGET}

CLEANFILES=	parse+%DIKED.c parse.c parse.h y.tab.h cfns.h

.include <bsd.prog.mk>
