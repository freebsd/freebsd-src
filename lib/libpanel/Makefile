# Makefile for libpanel
# $FreeBSD: src/lib/libpanel/Makefile,v 1.2 2000/01/14 07:57:39 rgrimes Exp $

NCURSES=${.CURDIR}/../../contrib/ncurses

.PATH:	${NCURSES}/panel

LIB=    panel

SRCS=	panel.c
INCS=	${NCURSES}/panel/panel.h

CFLAGS+= -I${.CURDIR}/../libncurses -I${NCURSES}/panel -I${NCURSES}/include \
	-Wall -DNDEBUG -DHAVE_CONFIG_H

.include <bsd.lib.mk>
