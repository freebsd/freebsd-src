# Makefile for libmenu
# $FreeBSD: src/lib/libmenu/Makefile,v 1.2.2.1 2000/05/25 09:54:28 tg Exp $

NCURSES=${.CURDIR}/../../contrib/ncurses

.PATH:	${NCURSES}/menu

LIB=    menu

SRCS=	m_attribs.c m_cursor.c m_driver.c m_format.c m_global.c m_hook.c \
	m_item_cur.c m_item_nam.c m_item_new.c m_item_opt.c m_item_top.c \
	m_item_use.c m_item_val.c m_item_vis.c m_items.c m_new.c m_opts.c \
	m_pad.c m_pattern.c m_post.c m_req_name.c m_scale.c m_spacing.c \
	m_sub.c m_userptr.c m_win.c
INCS=	${NCURSES}/menu/menu.h ${NCURSES}/menu/eti.h

CFLAGS+= -I${.CURDIR}/../libncurses -I${NCURSES}/menu -I${NCURSES}/include \
	-Wall -DNDEBUG -DHAVE_CONFIG_H

.include <bsd.lib.mk>
