# Makefile for libpanel
# $FreeBSD$

NCURSES=${.CURDIR}/../../contrib/ncurses

.PATH:	${NCURSES}/panel

LIB=    panel

SRCS=	panel.c
INCS=	${NCURSES}/panel/panel.h

CFLAGS+= -I${.CURDIR}/../libncurses -I${NCURSES}/panel -I${NCURSES}/include \
	-Wall -DNDEBUG -DHAVE_CONFIG_H

.include <bsd.lib.mk>
