#	@(#)Makefile	8.1 (Berkeley) 6/5/93
#	$Id$

PROG=	ping
MAN8=	ping.8
BINOWN=	root
BINMODE=4555
COPTS+=	-Wall -Wmissing-prototypes
DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>
