# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	cpp0
SRCS=	cppmain.c c-parse.c c-lang.c c-decl.c

BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED?=yes

DPADD+=	${LIBCC_INT}
LDADD+=	${LIBCC_INT}

#-----------------------------------------------------------------------
# C parser
.ORDER: c-parse.c
c-parse.c: c-parse.in
	sed -e "/^ifobjc$$/,/^end ifobjc$$/d" \
	    -e "/^ifc$$/d" \
	    -e "/^end ifc$$/d" \
	    ${GCCDIR}/c-parse.in > c-parse.y
	${YACC} -o c-parse.c.in c-parse.y
	sed -e "s/malloc/xmalloc/g" \
	    -e "s/realloc/xrealloc/g" \
	    c-parse.c.in >c-parse.c

CLEANFILES+=	c-parse.c c-parse.y		# insurance

#-----------------------------------------------------------------------

.include <bsd.prog.mk>
