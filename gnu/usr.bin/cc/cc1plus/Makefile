# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}/cp ${GCCDIR}

PROG=	cc1plus
SRCS=	parse.c cfns.h
SRCS+=	main.c cp-lang.c
SRCS+=	call.c class.c cvt.c decl.c decl2.c         error.c except.c expr.c \
	friend.c init.c lex.c mangle.c method.c pt.c ptree.c repo.c rtti.c \
	search.c semantics.c spew.c tree.c typeck.c typeck2.c        dump.c optimize.c

BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED?=yes

CFLAGS+= -I${GCCDIR}/cp -I.

DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

.ORDER: parse.c
parse.c: parse.y
	${YACC} -o parse.c.in ${.ALLSRC}
	sed -e "s/malloc/xmalloc/g" \
	    -e "s/realloc/xrealloc/g" \
	    parse.c.in >parse.c

CLEANFILES+=	parse.c         cfns.h

cfns.h:
	gperf -o -C -E -k '1-6,$$' -j1 -D -N 'libc_name_p' \
	    ${GCCDIR}/cp/cfns.gperf > ${.TARGET}

.include <bsd.prog.mk>
