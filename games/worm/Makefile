#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/worm/Makefile,v 1.2.14.1 2001/04/25 09:29:04 ru Exp $

PROG=	worm
MAN=	worm.6
DPADD=	${LIBCURSES} ${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-lcurses -ltermcap -lcompat
HIDEGAME=hidegame

.include <bsd.prog.mk>
