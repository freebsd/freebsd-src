# $FreeBSD: src/gnu/usr.bin/cc/cc1obj/Makefile,v 1.11.2.1 2000/07/04 05:41:39 obrien Exp $

.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR}/objc ${GCCDIR}

PROG=	cc1obj
SRCS=	objc-parse.c objc-parse.h objc-parse.h objc-act.c
# Ugh, compiled twice...
SRCS+=	c-decl.c c-lex.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED=yes

CFLAGS+= -I${GCCDIR}/objc -I.

DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

#-----------------------------------------------------------------------
# objc parser
.ORDER: objc-parse.c objc-parse.h
objc-parse.c objc-parse.h: c-parse.in
	sed -e "/^ifc$$/,/^end ifc$$/d" \
	    -e "/^ifobjc$$/d" -e "/^end ifobjc$$/d" \
	    ${GCCDIR}/c-parse.in > objc-parse.y
	${YACC} -d -o objc-parse.c objc-parse.y
	ln -sf objc-parse.h c-parse.h
	mv objc-parse.y objc-parse.y.out

CLEANFILES+=	objc-parse.c objc-parse.h c-parse.h \
		objc-parse.y objc-parse.y.out	# insurance

#-----------------------------------------------------------------------

.include <bsd.prog.mk>
