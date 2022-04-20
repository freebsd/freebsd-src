BSDDIALOG=	${SRCTOP}/contrib/bsddialog

.PATH:	${BSDDIALOG}/lib

LIB=	bsddialog
PRIVATELIB=	yes
SHLIB_MAJOR=	0
SRCS=	barbox.c \
	formbox.c \
	infobox.c \
	lib_util.c \
	lib_util.h \
	libbsddialog.c \
	menubox.c \
	messagebox.c \
	textbox.c \
	theme.c \
	timebox.c
INCS=	bsddialog.h \
	bsddialog_theme.h \
	bsddialog_progressview.h
MAN=	bsddialog.3

LIBADD=	ncursesw tinfow formw
CFLAGS+=	-D_XOPEN_SOURCE_EXTENDED

.include <bsd.lib.mk>
