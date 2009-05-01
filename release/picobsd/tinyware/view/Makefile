# $FreeBSD: src/release/picobsd/tinyware/view/Makefile,v 1.5.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=view
SRCS=view.c
CFLAGS+=-I/usr/local/include
LDADD+=-L/usr/local/lib -lpng -lvgl -lz -lm
NO_MAN=

.include <bsd.prog.mk>
