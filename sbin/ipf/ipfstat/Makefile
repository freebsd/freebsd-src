#	$FreeBSD: src/sbin/ipf/ipfstat/Makefile,v 1.1.16.1 2008/10/02 02:57:24 kensmith Exp $

NOGCCERROR=	# defined

PROG=		ipfstat
SRCS=		ipfstat.c
MAN=		ipfstat.8
DPADD+=  	${LIBCURSES}
LDADD+=  	-lcurses

.include <bsd.prog.mk>
