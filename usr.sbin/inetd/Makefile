#	from: @(#)Makefile	5.5 (Berkeley) 6/29/90
#	$Id: Makefile,v 1.5 1993/09/25 12:14:32 rgrimes Exp $

PROG=	inetd
MAN8=	inetd.8
MLINKS=	inetd.8 inetd.5

DPADD=	${LIBUTIL} ${LIBRPC}
LDADD=	-lutil -lrpc

.include <bsd.prog.mk>
