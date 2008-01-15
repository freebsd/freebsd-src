#	$FreeBSD: src/sbin/ipf/ipfstat/Makefile,v 1.1 2005/04/25 18:55:50 darrenr Exp $

NOGCCERROR=	# defined

PROG=		ipfstat
SRCS=		ipfstat.c
MAN=		ipfstat.8
DPADD+=  	${LIBCURSES}
LDADD+=  	-lcurses

.include <bsd.prog.mk>
