#	$FreeBSD: src/sbin/ipf/ipfstat/Makefile,v 1.1.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

NOGCCERROR=	# defined

PROG=		ipfstat
SRCS=		ipfstat.c
MAN=		ipfstat.8
DPADD+=  	${LIBCURSES}
LDADD+=  	-lcurses

.include <bsd.prog.mk>
