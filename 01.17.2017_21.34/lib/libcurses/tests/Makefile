# $NetBSD: Makefile,v 1.2 2011/09/08 18:44:38 jmmv Exp $

NOMAN=		# defined

.include <bsd.own.mk>

ATFFILE=	no

TESTSDIR=	${TESTSBASE}/lib/libcurses

FILESDIR=	${TESTSDIR}/tests
FILES=		start
FILES+=		std_defines
FILES+=		attributes
FILES+=		addch
FILES+=		addchstr
FILES+=		addchnstr
FILES+=		addstr
FILES+=		addnstr
FILES+=		beep
FILES+=		background
FILES+=		can_change_color
FILES+=		cbreak
FILES+=		color_set
FILES+=		copywin
FILES+=		curs_set
FILES+=		getch
FILES+=		start_color
FILES+=		assume_default_colors
FILES+=		termattrs
FILES+=		timeout
FILES+=		window
FILES+=		window_create
FILES+=		wborder
FILES+=		box
FILES+=		wprintw
FILES+=		wscrl
FILES+=		mvwin
FILES+=		getstr
FILES+=		chgat
FILES+=		clear
FILES+=		color_content
FILES+=		fill_screen

.include <bsd.test.mk>
.include <bsd.files.mk>
.include <bsd.prog.mk>
