# Makefile for libmenu
# $FreeBSD$

NCURSES=${.CURDIR}/../../contrib/ncurses

.PATH:	${NCURSES}/menu ${NCURSES}/include

LIB=    menu
AWK?=	awk

SRCS=	ncurses_def.h \
	m_attribs.c m_cursor.c m_driver.c m_format.c m_global.c m_hook.c \
	m_item_cur.c m_item_nam.c m_item_new.c m_item_opt.c m_item_top.c \
	m_item_use.c m_item_val.c m_item_vis.c m_items.c m_new.c m_opts.c \
	m_pad.c m_pattern.c m_post.c m_req_name.c m_scale.c m_spacing.c \
	m_sub.c m_userptr.c m_win.c
INCS=	${NCURSES}/menu/menu.h ${NCURSES}/menu/eti.h

CLEANFILES+=	ncurses_def.h
CFLAGS+= -I. -I${.CURDIR}/../libncurses -I${NCURSES}/menu -I${NCURSES}/include \
	-Wall -DNDEBUG -DHAVE_CONFIG_H

ncurses_def.h:	MKncurses_def.sh ncurses_defs
	AWK=${AWK} sh ${NCURSES}/include/MKncurses_def.sh \
	    ${NCURSES}/include/ncurses_defs > ncurses_def.h

.include <bsd.lib.mk>
