#	@(#)Makefile	8.1 (Berkeley) 5/31/93
#	$Id: Makefile,v 1.2.8.1 1997/09/14 13:07:02 jkh Exp $

PROG=	date
SRCS=	date.c netdate.c vary.c
CFLAGS+=-Wall
DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
