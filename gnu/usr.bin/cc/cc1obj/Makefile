# $FreeBSD$

.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR}/objc ${GCCDIR}

PROG=	cc1obj
SRCS=	objc-parse.c objc-act.c objc-lang.c main.c c-decl.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED?=yes

CFLAGS+= -I${GCCDIR}/objc -I.

DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

#-----------------------------------------------------------------------
# objc parser
.ORDER: objc-parse.c
objc-parse.c: c-parse.in
	sed -e "/^ifc$$/,/^end ifc$$/d" \
	    -e "/^ifobjc$$/d" \
	    -e "/^end ifobjc$$/d" \
	    ${GCCDIR}/c-parse.in > objc-parse.y
	${YACC} -o objc-parse.c.in objc-parse.y
	sed -e "s/malloc/xmalloc/g" \
	    -e "s/realloc/xrealloc/g" \
	    objc-parse.c.in >objc-parse.c

CLEANFILES+=	objc-parse.c           objc-parse.y		# insurance

#-----------------------------------------------------------------------

.include <bsd.prog.mk>
