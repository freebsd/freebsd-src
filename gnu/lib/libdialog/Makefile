# Makefile for libdialog
# $Id: Makefile,v 1.9 1995/04/21 10:15:31 jkh Exp $

LIB=    dialog
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
