# $FreeBSD$

PROG=		dpv

CFLAGS+=	-I${.CURDIR}

DPADD=		${LIBDPV} ${LIBDIALOG} ${LIBFIGPAR} ${LIBNCURSESW} ${LIBUTIL} ${LIBM}
LDADD=		-ldpv -ldialog -lfigpar -lncursesw -lutil -lm

WARNS?=		6

.include <bsd.prog.mk>
