# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}/cp ${GCCDIR}

PROG=	cc1plus
SRCS=	parse.c parse.h cfns.h
SRCS+=	call.c class.c cvt.c decl.c decl2.c errfn.c error.c except.c expr.c \
	friend.c init.c lex.c mangle.c method.c pt.c ptree.c repo.c rtti.c \
	search.c semantics.c spew.c tree.c typeck.c typeck2.c xref.c dump.c optimize.c

BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED=yes

CFLAGS+= -I${GCCDIR}/cp -I.

DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

.ORDER: parse.c parse.h
parse.c parse.h: parse.y
	${YACC} -d -o parse.c ${GCCDIR}/cp/parse.y
	grep '^#define[   ]*YYEMPTY' parse.c >>parse.h

CLEANFILES+=	parse.c parse.h cfns.h

cfns.h:
	gperf -o -C -E -k '1-6,$$' -j1 -D -N 'libc_name_p' \
	    ${GCCDIR}/cp/cfns.gperf > ${.TARGET}

.include <bsd.prog.mk>
