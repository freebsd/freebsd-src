# $FreeBSD$

PROG=	kbdmap
CFLAGS+=	-I${SRCTOP}/contrib/bsddialog/lib
LIBADD=	bsddialog
LINKS=	${BINDIR}/kbdmap ${BINDIR}/vidfont
MLINKS=	kbdmap.1 vidfont.1

.include <bsd.prog.mk>
