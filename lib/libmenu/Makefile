# Makefile for libmenu
# $FreeBSD$

NCURSES=${.CURDIR}/../../contrib/ncurses

.PATH:	${NCURSES}/menu ${NCURSES}/include
.PATH:  ${NCURSES}/man

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

MANx=	menu.3x menu_attribs.3x menu_attributes.3x menu_cursor.3x \
	menu_driver.3x menu_format.3x menu_hook.3x menu_items.3x \
	menu_mark.3x menu_new.3x menu_opts.3x menu_pattern.3x \
	menu_post.3x menu_requestname.3x menu_spacing.3x \
	menu_userptr.3x menu_win.3x mitem_current.3x mitem_name.3x \
	mitem_new.3x mitem_opts.3x mitem_userptr.3x mitem_value.3x \
	mitem_visible.3x

# Generate the MAN list from MANx
.for page in ${MANx}
CLEANFILES+=${page:T:S/x$//g}
MAN+=${page:T:S/x$//g}
${page:T:S/x$//g}: ${page}
	ln -s ${.ALLSRC} ${.TARGET}
.endfor

MLINKS+=menu_attributes.3 menu_back.3 menu_attributes.3 menu_fore.3 \
	menu_attributes.3 menu_grey.3 menu_attributes.3 menu_pad.3 \
	menu_attributes.3 set_menu_back.3 menu_attributes.3 set_menu_fore.3 \
	menu_attributes.3 set_menu_grey.3 menu_attributes.3 set_menu_pad.3
MLINKS+=menu_cursor.3 pos_menu_cursor.3
MLINKS+=menu_format.3 set_menu_format.3
MLINKS+=menu_hook.3 item_init.3 menu_hook.3 item_term.3 \
	menu_hook.3 menu_init.3 menu_hook.3 menu_term.3 \
	menu_hook.3 set_item_init.3 menu_hook.3 set_item_term.3 \
	menu_hook.3 set_menu_init.3 menu_hook.3 set_menu_term.3
MLINKS+=menu_items.3 item_count.3 menu_items.3 set_menu_items.3
MLINKS+=menu_mark.3 set_menu_mark.3
MLINKS+=menu_new.3 free_menu.3 menu_new.3 new_menu.3
MLINKS+=menu_opts.3 menu_opts_off.3 menu_opts.3 menu_opts_on.3
MLINKS+=menu_pattern.3 set_menu_pattern.3
MLINKS+=menu_post.3 post_menu.3 menu_post.3 unpost_menu.3
MLINKS+=menu_requestname.3 menu_request_by_name.3 \
	menu_requestname.3 menu_request_name.3
MLINKS+=menu_spacing.3 set_menu_spacing.3
MLINKS+=menu_userptr.3 set_menu_userptr.3
MLINKS+=menu_win.3 menu_sub.3 menu_win.3 scale_menu.3 \
	menu_win.3 set_menu_sub.3 menu_win.3 set_menu_win.3
MLINKS+=mitem_current.3 current_item.3 mitem_current.3 item_index.3 \
	mitem_current.3 set_top_row.3 mitem_current.3 top_row.3 \
	mitem_current.3 set_current_item.3
MLINKS+=mitem_name.3 item_description.3 mitem_name.3 item_name.3
MLINKS+=mitem_new.3 free_item.3 mitem_new.3 new_item.3
MLINKS+=mitem_opts.3 item_opts.3 mitem_opts.3 item_opts_off.3 \
	mitem_opts.3 item_opts_on.3 mitem_opts.3 set_item_opts.3 \
	mitem_opts.3 set_menu_opts.3
MLINKS+=mitem_userptr.3 item_userptr.3 mitem_userptr.3 set_item_userptr.3
MLINKS+=mitem_value.3 item_value.3 mitem_value.3 set_item_value.3
MLINKS+=mitem_visible.3 item_visible.3

.include <bsd.lib.mk>
