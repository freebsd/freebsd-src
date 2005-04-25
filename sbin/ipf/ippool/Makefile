#	$FreeBSD$

PROG=		ippool
SRCS=		ippool_y.c ippool_l.c kmem.c ippool.c
MAN=		ippool.5 ippool.8
CFLAGS+=	-I.

DPSRCS+=	ippool_l.h ippool_y.h

CLEANFILES+=	ippool_y.c ippool_y.h
CLEANFILES+=	ippool_l.c ippool_l.h

ippool_y.c: ippool_y.y
	${_MKTARGET_CREATE}
	${YACC} -d ${.ALLSRC}
	sed -e 's/yy/ippool_yy/g' \
	    -e 's/"ippool_y.y"/"..\/tools\/ippool_y.y"/' \
	    y.tab.c > ${.TARGET}
	sed -e 's/yy/ippool_yy/g' \
	    y.tab.h > ${.TARGET:.c=.h}

ippool_y.h: ippool_y.c

ippool_l.c: lexer.c
	${_MKTARGET_CREATE}
	sed -e 's/yy/ippool_yy/g' \
	    -e 's/y.tab.h/ippool_y.h/' \
	    -e 's/lexer.h/ippool_l.h/' \
	    ${.ALLSRC} > ${.TARGET}

ippool_l.h: lexer.h
	${_MKTARGET_CREATE}
	sed -e 's/yy/ippool_yy/g' \
	    ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
