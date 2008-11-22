# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}/objc ${GCCDIR}

PROG=	cc1obj
SRCS=	main.c objc-parse+DIKED.c objc-act.c objc-lang.c c-decl.c
BINDIR=	/usr/libexec
NO_MAN=
NO_SHARED?=yes

CFLAGS+= -I${GCCDIR}/objc -I.

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

#-----------------------------------------------------------------------
# objc parser
objc-parse+DIKED.c: objc-parse.c
	sed -e "s/malloc/xmalloc/g" \
	    -e "s/realloc/xrealloc/g" \
	    ${.ALLSRC} > ${.TARGET}

objc-parse.y: c-parse.in
	sed -e "/^@@ifc.*/,/^@@end_ifc.*/d" \
	    -e "/^@@ifobjc.*/d" -e "/^@@end_ifobjc.*/d" \
	    ${.ALLSRC} > ${.TARGET}

CLEANFILES=	objc-parse+DIKED.c objc-parse.c objc-parse.y
CLEANFILES+=	y.tab.h # we don't use it, but the system YACC rules are naive

#-----------------------------------------------------------------------

.include <bsd.prog.mk>
