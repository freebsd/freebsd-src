# Makefile for libpanel
# $FreeBSD$

NCURSES=${.CURDIR}/../../contrib/ncurses

.PATH:	${NCURSES}/panel ${NCURSES}/include

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

.include <bsd.lib.mk>
