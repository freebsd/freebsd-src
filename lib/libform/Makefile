# Makefile for libform
# $FreeBSD: src/lib/libform/Makefile,v 1.2.2.1 2000/05/25 09:54:27 tg Exp $

NCURSES=${.CURDIR}/../../contrib/ncurses

.PATH:	${NCURSES}/form
.PATH:	${NCURSES}/menu

LIB=    form

SRCS=	fld_arg.c fld_attr.c fld_current.c fld_def.c fld_dup.c fld_ftchoice.c \
	fld_ftlink.c fld_info.c fld_just.c fld_link.c fld_max.c fld_move.c \
	fld_newftyp.c fld_opts.c fld_pad.c fld_page.c fld_stat.c fld_type.c \
	fld_user.c frm_cursor.c frm_data.c frm_def.c frm_driver.c frm_hook.c \
	frm_opts.c frm_page.c frm_post.c frm_req_name.c frm_scale.c frm_sub.c \
	frm_user.c frm_win.c fty_alnum.c fty_alpha.c fty_enum.c fty_int.c \
	fty_ipv4.c fty_num.c fty_regex.c
INCS=	${NCURSES}/form/form.h

CFLAGS+=-I${.CURDIR}/../libncurses -I${NCURSES}/form -I${NCURSES}/menu \
	-I${NCURSES}/include -Wall -DNDEBUG -DHAVE_CONFIG_H

.include <bsd.lib.mk>
