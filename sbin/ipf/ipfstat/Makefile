#	$FreeBSD: src/sbin/ipf/ipfstat/Makefile,v 1.1.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

NOGCCERROR=	# defined

PROG=		ipfstat
SRCS=		ipfstat.c
MAN=		ipfstat.8
DPADD+=  	${LIBCURSES}
LDADD+=  	-lcurses

.include <bsd.prog.mk>
