# Makefile for libpanel
# $FreeBSD$

NCURSES=${.CURDIR}/../../contrib/ncurses

.PATH:	${NCURSES}/panel

LIB=    panel

SRCS=	panel.c

CFLAGS+= -I${.CURDIR}/../libncurses -I${NCURSES}/panel -I${NCURSES}/include \
	-Wall -DNDEBUG -DHAVE_CONFIG_H

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${NCURSES}/panel/panel.h ${DESTDIR}/usr/include

.include <bsd.lib.mk>
