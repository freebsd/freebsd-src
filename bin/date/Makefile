#	@(#)Makefile	8.1 (Berkeley) 5/31/93
#	$Id: Makefile,v 1.5 1997/08/04 03:37:05 brian Exp $

PROG=	date
SRCS=	date.c netdate.c vary.c
CFLAGS+=-Wall
DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
