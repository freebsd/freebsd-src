# Makefile for libdialog
# $Id: Makefile,v 1.11 1995/05/03 18:53:19 ache Exp $

LIB=    dialog
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
	      install -c -o ${BINOWN} -g ${BINGRP} -m 444 dialog.h \
	      ${DESTDIR}/usr/include

.include <bsd.lib.mk>
