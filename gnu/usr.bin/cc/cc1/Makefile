# $FreeBSD$

.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR}
 
PROG=	cc1
SRCS=	c-parse.c c-parse.h c-lang.c
# Ugh, compiled twice...
SRCS+=	c-decl.c c-lex.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED=yes

CFLAGS+= -I.

DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

#-----------------------------------------------------------------------
# C parser
.ORDER: c-parse.c c-parse.h
c-parse.c c-parse.h: c-parse.in
	sed -e "/^ifobjc$$/,/^end ifobjc$$/d" \
	    -e "/^ifc$$/d" -e "/^end ifc$$/d" \
	    ${GCCDIR}/c-parse.in > c-parse.y
	${YACC} -d -o c-parse.c c-parse.y
	mv c-parse.y c-parse.y.out

CLEANFILES+=	c-parse.c c-parse.h \
		c-parse.y c-parse.y.out		# insurance

#-----------------------------------------------------------------------

.include <bsd.prog.mk>
