#	@(#)Makefile	5.4 (Berkeley) 5/11/90
#
#	$Header: /home/ncvs/src/sbin/slattach/Makefile,v 1.5 1995/09/19 03:27:23 ache Exp $

PROG=	slattach
SRCS=   slattach.c uucplock.c
MAN8=	slattach.8
MLINKS=	slattach.8 slip.8
LDADD=	-lutil
DPADD=	${LIBUTIL}

.PATH: ${.CURDIR}/../startslip

.include <bsd.prog.mk>
