# Makefile for libform
# $FreeBSD$

NCURSES=${.CURDIR}/../../contrib/ncurses

.PATH:	${NCURSES}/form
.PATH:	${NCURSES}/menu

LIB=    form

SRCS=	fty_regex.c fty_num.c fty_int.c fty_ipv4.c fty_enum.c fty_alpha.c \
	fty_alnum.c \
	frm_data.c frm_win.c frm_user.c frm_opts.c frm_hook.c frm_req_name.c \
	fld_user.c fld_type.c fld_stat.c fld_def.c frm_def.c frm_driver.c

CFLAGS+=-I${.CURDIR}/../libncurses -I${NCURSES}/form -I${NCURSES}/menu \
	-I${NCURSES}/include -Wall -DNDEBUG -DHAVE_CONFIG_H

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${NCURSES}/form/form.h ${DESTDIR}/usr/include

.include <bsd.lib.mk>
