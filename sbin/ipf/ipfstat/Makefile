#	$FreeBSD: src/sbin/ipf/ipfstat/Makefile,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $

NOGCCERROR=	# defined

PROG=		ipfstat
SRCS=		ipfstat.c
MAN=		ipfstat.8
DPADD+=  	${LIBCURSES}
LDADD+=  	-lcurses

.include <bsd.prog.mk>
