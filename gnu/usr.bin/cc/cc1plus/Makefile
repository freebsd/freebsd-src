# $FreeBSD: src/gnu/usr.bin/cc/cc1plus/Makefile,v 1.22.2.2 2003/09/09 20:17:28 ru Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR}/cp

PROG=	cc1plus
SRCS=	parse.y y.tab.h parse.h
SRCS+=	call.c class.c cvt.c decl.c decl2.c errfn.c error.c except.c expr.c \
	friend.c init.c lex.c method.c pt.c ptree.c repo.c rtti.c \
	search.c semantics.c sig.c spew.c tree.c typeck.c typeck2.c xref.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED=yes

CFLAGS+= -I${GCCDIR}/cp -I.

DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

parse.h: y.tab.h parse.c
	cp -p y.tab.h ${.TARGET}
	grep '^#define[   ]*YYEMPTY' parse.c >>${.TARGET}

CLEANFILES+=	parse.h

CPPHDRS=	exception new new.h typeinfo
INCS=		${CPPHDRS:S;^;${GCCDIR}/cp/inc/;}
INCSDIR=	${INCLUDEDIR}/g++

.include <bsd.prog.mk>
