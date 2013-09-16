# $FreeBSD$

BINDIR= /usr/libexec/bsdinstall
PROG=	distfetch
DPADD=	${LIBFETCH} ${LIBNCURSESW} ${LIBDIALOG} ${LIBM}
LDADD=	-lfetch -lncursesw -ldialog -lm

WARNS?=	6
NO_MAN=	true

.include <bsd.prog.mk>
