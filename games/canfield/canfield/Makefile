#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	canfield
MAN6=	canfield.6
DPADD=	${LIBCURSES} ${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-lcurses -ltermcap -lcompat
HIDEGAME=hidegame
MLINKS=	canfield.6 cfscores.6


.include "../../Makefile.inc"
.include <bsd.prog.mk>
