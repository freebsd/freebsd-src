# Makefile for libdialog
# $Id: Makefile,v 1.1.1.1 1994/10/11 23:30:50 ache Exp $

LIB=    dialog
SRCS=   kernel.c rc.c checklist.c inputbox.c menubox.c msgbox.c \
	radiolist.c textbox.c yesno.c

CFLAGS+= -Wall -Wstrict-prototypes -DHAVE_NCURSES -DLOCALE

LDADD+= -lncurses -lmytinfo

beforeinstall:
	-cd ${.CURDIR}; cmp -s dialog.h ${DESTDIR}/usr/include/dialog.h || \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 dialog.h \
	    ${DESTDIR}/usr/include

.include <bsd.lib.mk>
