# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	tradcpp0
SRCS=	tradcpp.c tradcif.c
BINDIR=	/usr/libexec
NOMAN=	1

DPADD+=	${LIBCC_INT}
LDADD+=	${LIBCC_INT}

#	${YACC} ${GCCDIR}/tradcif.y
tradcif.c: tradcif.y
	${YACC} ${.ALLSRC}
	sed -e "s/malloc/xmalloc/g" \
	    -e "s/realloc/xrealloc/g" \
	    y.tab.c >${.TARGET}

.include <bsd.prog.mk>
