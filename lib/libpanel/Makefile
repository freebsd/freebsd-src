# Makefile for libpanel
# $FreeBSD$

NCURSES=${.CURDIR}/../../contrib/ncurses

.PATH:	${NCURSES}/panel ${NCURSES}/include
.PATH:  ${NCURSES}/man

LIB=    panel
AWK?=	awk

SRCS=	ncurses_def.h \
	p_above.c p_below.c p_bottom.c p_delete.c p_hidden.c \
	p_hide.c p_move.c p_new.c p_replace.c p_show.c p_top.c \
	p_update.c p_user.c p_win.c panel.c
INCS=	${NCURSES}/panel/panel.h

CLEANFILES+=	ncurses_def.h
CFLAGS+= -I. -I${.CURDIR}/../libncurses -I${NCURSES}/panel -I${NCURSES}/include \
	-Wall -DNDEBUG -DHAVE_CONFIG_H

ncurses_def.h:	MKncurses_def.sh ncurses_defs
	AWK=${AWK} sh ${NCURSES}/include/MKncurses_def.sh \
	    ${NCURSES}/include/ncurses_defs > ncurses_def.h

# generate MAN
CLEANFILES+=	panel.3
MAN=		panel.3
all-man: panel.3
panel.3: panel.3x
	ln -s ${.ALLSRC} ${.TARGET}

MLINKS+=panel.3 bottom_panel.3 panel.3 del_panel.3 panel.3 hide_panel.3 \
	panel.3 move_panel.3 panel.3 new_panel.3 panel.3 panel_above.3 \
	panel.3 panel_below.3 panel.3 panel_hidden.3 \
	panel.3 panel_userptr.3 panel.3 panel_window.3 \
	panel.3 replace_panel.3 panel.3 set_panel_userptr.3 \
	panel.3 show_panel.3 panel.3 top_panel.3 panel.3 update_panels.3

.include <bsd.lib.mk>

.SUFFIXES: .3x .3
