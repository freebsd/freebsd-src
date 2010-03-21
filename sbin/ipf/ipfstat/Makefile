#	$FreeBSD: src/sbin/ipf/ipfstat/Makefile,v 1.1.24.1 2010/02/10 00:26:20 kensmith Exp $

NOGCCERROR=	# defined

PROG=		ipfstat
SRCS=		ipfstat.c
MAN=		ipfstat.8
DPADD+=  	${LIBCURSES}
LDADD+=  	-lcurses

.include <bsd.prog.mk>
