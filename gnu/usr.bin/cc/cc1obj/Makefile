# $FreeBSD$

.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR}/objc ${GCCDIR}

PROG=	cc1obj
SRCS=	objc-parse.c c-parse.h objc-act.c
# Ugh, compiled twice...
SRCS+=	c-decl.c c-lex.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED?=yes

CFLAGS+= -I${GCCDIR}/objc -I.

DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

#-----------------------------------------------------------------------
# objc parser
.ORDER: objc-parse.c c-parse.h
objc-parse.c c-parse.h: c-parse.in
	sed -e "/^ifc$$/,/^end ifc$$/d" \
	    -e "/^ifobjc$$/d" \
	    -e "/^end ifobjc$$/d" \
	    ${.ALLSRC} > objc-parse.y
	${YACC} -d -o objc-parse.c objc-parse.y
	mv objc-parse.h c-parse.h

CLEANFILES+=	objc-parse.c c-parse.h objc-parse.y 	# insurance

#-----------------------------------------------------------------------

.include <bsd.prog.mk>
