#	From: @(#)Makefile	8.2 (Berkeley) 1/2/94
#	$FreeBSD$

CFLAGS+=#-DTFILE=\"/dev/ttyp0\"
CFLAGS+=-D_CURSES_PRIVATE -I${.CURDIR}
DPADD=	${LIBTERMCAP}
LDADD=	-ltermcap
LIB=	curses
SRCS=	addbytes.c addch.c addnstr.c box.c clear.c clrtobot.c clrtoeol.c \
	cr_put.c ctrace.c cur_hash.c curses.c delch.c deleteln.c delwin.c \
	erase.c fullname.c getch.c getstr.c id_subwins.c idlok.c initscr.c \
	insch.c insertln.c longname.c move.c mvwin.c newwin.c overlay.c \
	overwrite.c printw.c _putchar.c putchar.c refresh.c scanw.c scroll.c \
	setterm.c standout.c toucholap.c touchwin.c tscroll.c tstp.c tty.c \
	unctrl.c
MAN3=	curses.3

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/curses.h \
	    ${DESTDIR}/usr/include

.include <bsd.lib.mk>
