#	@(#)Makefile	8.1 (Berkeley) 5/31/93
#	$Id: Makefile,v 1.2 1994/09/24 02:54:33 davidg Exp $

PROG=	date
SRCS=	date.c netdate.c vary.c
CFLAGS+=-Wall
DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
