# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	tradcpp0
SRCS=	tradcpp.c tradcif+%DIKED.c
BINDIR=	/usr/libexec
NOMAN=	1

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

tradcif+%DIKED.c: tradcif.c
	sed -e "s/malloc/xmalloc/g" \
	    -e "s/realloc/xrealloc/g" \
	    ${.ALLSRC} >${.TARGET}

CLEANFILES=	tradcif+%DIKED.c tradcif.c
CLEANFILES+=	y.tab.h # we don't use it, but the system YACC rules are naive

.include <bsd.prog.mk>
