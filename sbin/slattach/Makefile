#	@(#)Makefile	5.4 (Berkeley) 5/11/90
#
#	$Header: /home/ncvs/src/sbin/slattach/Makefile,v 1.4 1994/08/23 08:28:30 rich Exp $

PROG=	slattach
SRCS=   slattach.c uucplock.c
MAN8=	slattach.8
MLINKS=	slattach.8 slip.8
LDADD=	-lutil
DPADD=	${LIBUTIL}

.include <bsd.prog.mk>
