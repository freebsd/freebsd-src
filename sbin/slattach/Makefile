#	@(#)Makefile	5.4 (Berkeley) 5/11/90
#
#	$Header: /pub/FreeBSD/FreeBSD-CVS/src/sbin/slattach/Makefile,v 1.4.6.1 1995/10/25 00:50:32 jkh Exp $

PROG=	slattach
SRCS=   slattach.c uucplock.c
MAN8=	slattach.8
MLINKS=	slattach.8 slip.8
LDADD=	-lutil
DPADD=	${LIBUTIL}

.PATH: ${.CURDIR}/../startslip

.include <bsd.prog.mk>
