#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	inetd
MAN8=	inetd.8
MLINKS=	inetd.8 inetd.conf.5

COPTS+=	-Wall
#COPTS+=	-DSANITY_CHECK

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

.include <bsd.prog.mk>
