#	$Id: Makefile,v 1.7 1997/02/22 16:14:04 peter Exp $

PROG=	tzsetup
MAN8=	tzsetup.8
CFLAGS+=	-I${.CURDIR}
LDADD+=	-ldialog -lncurses -lmytinfo
DPADD+=	${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}

.include <bsd.prog.mk>
