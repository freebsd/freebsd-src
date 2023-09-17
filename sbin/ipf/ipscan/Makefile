
PACKAGE=	ipf
PROG=		ipscan
SRCS=		${GENHDRS} ipscan_y.c
MAN=		ipscan.5 ipscan.8
MLINKS=		ipscan.5 ipscan.conf.5

GENHDRS+=	ipscan_y.h
CLEANFILES+=	${GENHDRS} ipscan_y.c

ipscan_y.c: ipscan_y.y
	${YACC} -d ${.ALLSRC}

ipscan_y.h: ipscan_y.c


.include <bsd.prog.mk>
