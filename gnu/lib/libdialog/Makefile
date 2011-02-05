# $FreeBSD$

DIALOG=		${.CURDIR}/../../../contrib/dialog

LIB=		dialog
SHLIB_MAJOR=	7
SRCS=		arrows.c buttons.c calendar.c checklist.c columns.c dlg_keys.c \
		editbox.c fselect.c formbox.c guage.c inputbox.c inputstr.c \
		menubox.c mixedform.c mixedgauge.c mouse.c mousewget.c \
		msgbox.c pause.c progressbox.c rc.c tailbox.c textbox.c \
		timebox.c trace.c ui_getc.c util.c version.c yesno.c
INCS=		dialog.h dlg_colors.h dlg_config.h dlg_keys.h
MAN=		dialog.3

CFLAGS+= 	-I${.CURDIR} -I${DIALOG} -D_XOPEN_SOURCE_EXTENDED
.PATH:		${DIALOG}
WARNS?=		3

.include <bsd.lib.mk>
