# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}/cp ${GCCDIR}

PROG=	cc1plus
SRCS=	cfns.h
SRCS+=	main.c cp-lang.c c-opts.c call.c class.c cvt.c cxx-pretty-print.c \
	decl.c decl2.c error.c except.c expr.c dump.c friend.c  init.c lex.c \
	mangle.c method.c name-lookup.c parser.c pt.c ptree.c repo.c rtti.c \
	search.c semantics.c tree.c typeck.c typeck2.c optimize.c 

BINDIR=	/usr/libexec
NO_MAN=
NO_SHARED?=yes

CFLAGS+= -I${GCCDIR}/cp -I.

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

#-----------------------------------------------------------------------
# C++ parser
cfns.h: cfns.gperf
	gperf -o -C -E -k '1-6,$$' -j1 -D -N 'libc_name_p' -L ANSI-C \
	    ${.ALLSRC} > ${.TARGET}

CLEANFILES= cfns.h

.include <bsd.prog.mk>
