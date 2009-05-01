#	$FreeBSD: src/sbin/ipf/ipfstat/Makefile,v 1.1.20.1 2009/04/15 03:14:26 kensmith Exp $

NOGCCERROR=	# defined

PROG=		ipfstat
SRCS=		ipfstat.c
MAN=		ipfstat.8
DPADD+=  	${LIBCURSES}
LDADD+=  	-lcurses

.include <bsd.prog.mk>
