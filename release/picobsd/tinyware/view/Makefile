# $FreeBSD: src/release/picobsd/tinyware/view/Makefile,v 1.5.24.1 2010/02/10 00:26:20 kensmith Exp $

PROG=view
SRCS=view.c
CFLAGS+=-I/usr/local/include
LDADD+=-L/usr/local/lib -lpng -lvgl -lz -lm
NO_MAN=

.include <bsd.prog.mk>
