#	$FreeBSD: src/sbin/ipf/ipfstat/Makefile,v 1.1.26.1 2010/12/21 17:10:29 kensmith Exp $

NOGCCERROR=	# defined

PROG=		ipfstat
SRCS=		ipfstat.c
MAN=		ipfstat.8
DPADD+=  	${LIBCURSES}
LDADD+=  	-lcurses

.include <bsd.prog.mk>
