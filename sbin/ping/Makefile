#	@(#)Makefile	8.1 (Berkeley) 6/5/93
#	$Id: Makefile,v 1.2.8.2 1998/03/06 13:07:12 jkh Exp $

PROG=	ping
MAN8=	ping.8
BINOWN=	root
BINMODE=4555
COPTS+=	-Wall -Wmissing-prototypes
DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>
