#	$FreeBSD: src/sbin/ipf/ipfstat/Makefile,v 1.1.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

NOGCCERROR=	# defined

PROG=		ipfstat
SRCS=		ipfstat.c
MAN=		ipfstat.8
DPADD+=  	${LIBCURSES}
LDADD+=  	-lcurses

.include <bsd.prog.mk>
