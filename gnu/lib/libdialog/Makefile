# Makefile for libdialog
# $Id: Makefile,v 1.8 1995/04/20 17:39:25 ache Exp $

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
	-cd ${.CURDIR}; cmp -s ui_objects.h \
	    ${DESTDIR}/usr/include/ui_objects.h || \
	      install -c -o ${BINOWN} -g ${BINGRP} -m 444 ui_objects.h \
	      ${DESTDIR}/usr/include

.include <bsd.lib.mk>
