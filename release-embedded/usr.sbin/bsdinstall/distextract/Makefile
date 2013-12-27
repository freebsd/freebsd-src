# $FreeBSD$

BINDIR= /usr/libexec/bsdinstall
PROG=	distextract
DPADD=	${LIBARCHIVE} ${LIBNCURSESW} ${LIBDIALOG} ${LIBM}
LDADD=	-larchive -lncursesw -ldialog -lm

WARNS?=	6
NO_MAN=	true

.include <bsd.prog.mk>
