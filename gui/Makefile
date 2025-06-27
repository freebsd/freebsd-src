# Simple Makefile for ls_gui

PROG=ls_gui
SRCS=ls_gui.c
MAN=

# Add GTK4 flags
CFLAGS+= `pkg-config --cflags gtk4`
LDADD+= `pkg-config --libs gtk4`

.include <bsd.prog.mk>
