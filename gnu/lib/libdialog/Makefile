# Makefile for libdialog
# $FreeBSD$

LIB=    dialog
INCS=	dialog.h
MAN=	dialog.3

SHLIB_MAJOR= 4
SRCS=   kernel.c rc.c checklist.c inputbox.c menubox.c msgbox.c \
	lineedit.c radiolist.c textbox.c yesno.c prgbox.c raw_popen.c \
	fselect.c ui_objects.c dir.c notify.c help.c gauge.c tree.c

EXAMPLES= Makefile check1.c check2.c check3.c dselect.c fselect.c \
	ftree1.c ftree1.test ftree2.c ftree2.test gauge.c input1.c \
	input2.c menu1.c menu2.c menu3.c msg.c prgbox.c radio1.c \
	radio2.c radio3.c text.c tree.c yesno.c

CFLAGS+= -I${.CURDIR} -Wall -Wstrict-prototypes -DLOCALE

LDADD+= -lncurses
DPADD+= ${LIBNCURSES}

.if !defined(NOHTML)
afterinstall:
.for file in ${EXAMPLES}
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 444 \
	      ${.CURDIR}/TESTS/${file} ${DESTDIR}/usr/share/examples/libdialog
.endfor
.endif

MLINKS+=dialog.3 draw_shadow.3 dialog.3 draw_box.3 \
	dialog.3 line_edit.3 dialog.3 strheight.3 \
	dialog.3 strwidth.3 dialog.3 dialog_create_rc.3 \
	dialog.3 dialog_yesno.3 dialog.3 dialog_noyes.3 \
	dialog.3 dialog_prgbox.3 \
	dialog.3 dialog_textbox.3 dialog.3 dialog_menu.3 \
	dialog.3 dialog_checklist.3 dialog.3 dialog_radiolist.3 \
	dialog.3 dialog_inputbox.3 dialog.3 dialog_clear_norefresh.3 \
	dialog.3 dialog_clear.3 dialog.3 dialog_update.3 \
	dialog.3 dialog_fselect.3 dialog.3 dialog_notify.3 \
	dialog.3 dialog_mesgbox.3 dialog.3 dialog_gauge.3 \
	dialog.3 init_dialog.3 dialog.3 end_dialog.3 \
	dialog.3 use_helpfile.3 dialog.3 use_helpline.3 \
	dialog.3 get_helpline.3 dialog.3 restore_helpline.3 \
	dialog.3 dialog_msgbox.3 dialog.3 dialog_ftree.3 \
	dialog.3 dialog_tree.3

.include <bsd.lib.mk>
