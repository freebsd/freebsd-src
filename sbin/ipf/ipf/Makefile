#	$FreeBSD$

.include <bsd.own.mk>		# for MKDYNAMICROOT definition

PROG=		ipf
SRCS=		ipf.c ipfcomp.c ipf_y.c ipf_l.c
MAN=		ipf.8 ipf.4 ipf.5 ipl.4
MLINKS=		ipl.4 ipfilter.4 ipf.5 ipf.conf.5 ipf.5 ipf6.conf.5
CFLAGS+=	-I.

DPSRCS+=	ipf_l.h ipf_y.h

CLEANFILES+=	ipf_y.c ipf_y.h
CLEANFILES+=	ipf_l.c ipf_l.h

ipf_y.c: ipf_y.y
	${_MKTARGET_CREATE}
	${YACC} -d ${.ALLSRC}
	sed -e 's/yy/ipf_yy/g' \
	    -e 's/"ipf_y.y"/"..\/tools\/ipf_y.y"/' \
	    y.tab.c > ${.TARGET}
	sed -e 's/yy/ipf_yy/g' \
	    y.tab.h > ${.TARGET:.c=.h}

ipf_y.h: ipf_y.c

ipf_l.c: lexer.c
	${_MKTARGET_CREATE}
	sed -e 's/yy/ipf_yy/g' \
	    -e 's/y.tab.h/ipf_y.h/' \
	    -e 's/lexer.h/ipf_l.h/' \
	    ${.ALLSRC} > ${.TARGET}

ipf_l.h: lexer.h
	${_MKTARGET_CREATE}
	sed -e 's/yy/ipf_yy/g' \
	    ${.ALLSRC} > ${.TARGET}

BINDIR=		/sbin
.if defined(NO_DYNAMICROOT)
LDSTATIC?=	-static
.endif

.include <bsd.prog.mk>
