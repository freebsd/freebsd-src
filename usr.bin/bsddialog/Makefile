BSDDIALOG=	${SRCTOP}/contrib/bsddialog
.PATH:	${BSDDIALOG}

PROG=	bsddialog
SRCS=	bsddialog.c util_theme.c
MAN=	bsddialog.1
CFLAGS+=	-I${BSDDIALOG}/lib
LIBADD=	bsddialog tinfow

.include <bsd.prog.mk>
