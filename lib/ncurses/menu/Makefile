# Makefile for libmenu
# $FreeBSD$

NCURSES=${.CURDIR}/../../contrib/ncurses

.PATH:	${NCURSES}/menu

LIB=    menu

SRCS=	m_attribs.c m_cursor.c m_driver.c m_format.c m_global.c m_hook.c \
	m_item_cur.c m_item_nam.c m_item_new.c m_item_opt.c m_item_use.c \
	m_item_val.c m_item_vis.c m_items.c m_new.c m_opts.c m_post.c \
	m_req_name.c m_spacing.c m_userptr.c m_win.c

CFLAGS+= -I${.CURDIR}/../libncurses -I${NCURSES}/menu -I${NCURSES}/include \
	-Wall -DNDEBUG -DHAVE_CONFIG_H

beforeinstall:
.for i in menu.h eti.h
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${NCURSES}/menu/$i \
		${DESTDIR}/usr/include
.endfor

.include <bsd.lib.mk>
