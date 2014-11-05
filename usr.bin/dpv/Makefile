# $FreeBSD$

PROG=		dpv

CFLAGS+=	-I${.CURDIR}

DPADD=		${LIBDPV} ${LIBDIALOG} ${LIBFIGPAR} ${LIBNCURSES} ${LIBUTIL}
LDADD=		-ldpv -ldialog -lfigpar -lncurses -lutil

WARNS?=		6

.include <bsd.prog.mk>
