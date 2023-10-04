BSDDIALOG=	${SRCTOP}/contrib/bsddialog/utility
.PATH:	${BSDDIALOG}

PROG=	bsddialog
SRCS=	bsddialog.c \
	util_builders.c \
	util_cli.c \
	util_theme.c
MAN=	bsddialog.1
CFLAGS+=	-I${BSDDIALOG}/../lib
LIBADD=	bsddialog tinfow

.include <bsd.prog.mk>
