# Makefile for libdialog
# $Id: Makefile,v 1.16 1996/02/09 16:19:16 mpp Exp $

LIB=    dialog
MAN3=	dialog.3

SHLIB_MAJOR= 3
SHLIB_MINOR= 0
SRCS=   kernel.c rc.c checklist.c inputbox.c menubox.c msgbox.c \
	lineedit.c radiolist.c textbox.c yesno.c prgbox.c raw_popen.c \
	fselect.c ui_objects.c dir.c notify.c help.c gauge.c

CFLAGS+= -I${.CURDIR} -Wall -Wstrict-prototypes -DLOCALE

LDADD+= -lncurses -lmytinfo
DPADD+= ${LIBNCURSES} ${LIBMYTINFO}

beforeinstall:
	-cd ${.CURDIR}; cmp -s dialog.h ${DESTDIR}/usr/include/dialog.h || \
	      ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 dialog.h \
	      ${DESTDIR}/usr/include

MLINKS+=dialog.3 draw_shadow.3 dialog.3 draw_box.3 \
	dialog.3 line_edit.3 dialog.3 strheight.3 \
	dialog.3 strwidth.3 dialog.3 dialog_create_rc.3 \
	dialog.3 dialog_yesno.3 dialog.3 dialog_prgbox.3 \
	dialog.3 dialog_textbox.3 dialog.3 dialog_menu.3 \
	dialog.3 dialog_checklist.3 dialog.3 dialog_radiolist.3 \
	dialog.3 dialog_inputbox.3 dialog.3 dialog_clear_norefresh.3 \
	dialog.3 dialog_clear.3 dialog.3 dialog_update.3 \
	dialog.3 dialog_fselect.3 dialog.3 dialog_notify.3 \
	dialog.3 dialog_mesgbox.3 dialog.3 dialog_gauge.3 \
	dialog.3 init_dialog.3 dialog.3 end_dialog.3 \
	dialog.3 use_helpfile.3 dialog.3 use_helpline.3 \
	dialog.3 get_helpline.3 dialog.3 restore_helpline.3 \
	dialog.3 dialog_msgbox.3

.include <bsd.lib.mk>
