#       $Id: Makefile,v 1.30 1998/04/28 07:16:24 ache Exp $

SUBDIR= doc

LIB=    readline
MAN3=   readline.3

TILDESRC= tilde.c
SRCS=     readline.c vi_mode.c funmap.c keymaps.c parens.c search.c \
	  rltty.c complete.c bind.c isearch.c display.c signals.c \
	  util.c kill.c undo.c macro.c input.c callback.c terminal.c \
	  nls.c xmalloc.c \
	  $(HISTSRC) $(TILDESRC)

DPADD+= $(LIBTERMCAP)
LDADD+= -ltermcap

.include "../Makefile.inc"
.include <bsd.lib.mk>
