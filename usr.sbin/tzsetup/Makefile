#	$Id: Makefile,v 1.4.2.1 1996/11/28 22:07:05 phk Exp $

PROG=	tzsetup
MAN8=	tzsetup.8
CFLAGS+=	-I${.CURDIR}
LDADD+=	-ldialog -lncurses -lmytinfo
DPADD+=	${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}

.include <bsd.prog.mk>
