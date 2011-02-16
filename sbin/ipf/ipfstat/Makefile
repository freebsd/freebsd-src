#	$FreeBSD: src/sbin/ipf/ipfstat/Makefile,v 1.1.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

NOGCCERROR=	# defined

PROG=		ipfstat
SRCS=		ipfstat.c
MAN=		ipfstat.8
DPADD+=  	${LIBCURSES}
LDADD+=  	-lcurses

.include <bsd.prog.mk>
