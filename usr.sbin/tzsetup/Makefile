#	$FreeBSD$

PROG=	tzsetup
MAN8=	tzsetup.8
CFLAGS+=	-I${.CURDIR}
LDADD+=	-ldialog -lncurses -lmytinfo
DPADD+=	${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}

.include <bsd.prog.mk>
