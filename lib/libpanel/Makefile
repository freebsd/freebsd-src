# Makefile for libpanel
# $FreeBSD: src/lib/libpanel/Makefile,v 1.2.2.1 2000/05/25 09:54:28 tg Exp $

NCURSES=${.CURDIR}/../../contrib/ncurses

.PATH:	${NCURSES}/panel

LIB=    panel

SRCS=	p_above.c p_below.c p_bottom.c p_delete.c p_hidden.c \
	p_hide.c p_move.c p_new.c p_replace.c p_show.c p_top.c \
	p_update.c p_user.c p_win.c panel.c
INCS=	${NCURSES}/panel/panel.h

CFLAGS+= -I${.CURDIR}/../libncurses -I${NCURSES}/panel -I${NCURSES}/include \
	-Wall -DNDEBUG -DHAVE_CONFIG_H

.include <bsd.lib.mk>
