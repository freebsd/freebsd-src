# $FreeBSD$

PROG=		dpv

CFLAGS+=	-I${.CURDIR}

DPADD=		${LIBDPV} ${LIBDIALOG} ${LIBFIGPAR} ${LIBNCURSESW} ${LIBUTIL}
LDADD=		-ldpv -ldialog -lfigpar -lncursesw -lutil

WARNS?=		6

.include <bsd.prog.mk>
