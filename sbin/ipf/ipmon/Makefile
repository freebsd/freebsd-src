#	$FreeBSD$

PACKAGE=	ipf
PROG=		ipmon
SRCS=		${GENHDRS} ipmon.c ipmon_y.c ipmon_l.c
MAN=		ipmon.5 ipmon.8
MLINKS=		ipmon.5 ipmon.conf.5

CFLAGS+=	-DLOGFAC=LOG_LOCAL0 -I.
CFLAGS+=	-Wno-error=unused-but-set-variable

GENHDRS+=	ipmon_l.h ipmon_y.h
CLEANFILES+=	${GENHDRS} ipmon_y.c ipmon_l.c

ipmon_y.c: ipmon_y.y
	${YACC} -d ${.ALLSRC}
	sed -e 's/yy/ipmon_yy/g' \
	    y.tab.c > ${.TARGET}
	sed -e 's/yy/ipmon_yy/g' \
	    y.tab.h > ${.TARGET:.c=.h}

ipmon_y.h: ipmon_y.c

ipmon_l.c: lexer.c
	sed -e 's/yy/ipmon_yy/g' \
	    -e 's/y.tab.h/ipmon_y.h/' \
	    -e 's/lexer.h/ipmon_l.h/' \
	    ${.ALLSRC} > ${.TARGET}

ipmon_l.h: lexer.h
	sed -e 's/yy/ipmon_yy/g' \
	    ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
