# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}
 
PROG=	cc1
SRCS=	main.c c-parse+%DIKED.c c-lang.c c-decl.c c-opts.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED?=yes

CFLAGS+= -I.

DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

#-----------------------------------------------------------------------
# C parser
c-parse+%DIKED.c: c-parse.c
	sed -e "s/malloc/xmalloc/g" \
	    -e "s/realloc/xrealloc/g" \
	    ${.ALLSRC} > ${.TARGET}

c-parse.y: c-parse.in
	sed -e "/^ifobjc$$/,/^end ifobjc$$/d" \
	    -e "/^ifc$$/d" \
	    -e "/^end ifc$$/d" \
	    ${.ALLSRC} > ${.TARGET}

CLEANFILES=	c-parse+%DIKED.c c-parse.c c-parse.y
CLEANFILES+=	y.tab.h # we don't use it, but the system YACC rules are naive

.include <bsd.prog.mk>
