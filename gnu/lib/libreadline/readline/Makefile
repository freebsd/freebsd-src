# $FreeBSD: src/gnu/lib/libreadline/readline/Makefile,v 1.5 2000/01/30 20:59:53 obrien Exp $

SUBDIR= doc

LIB=    readline
MAN3=   readline.3

TILDESRC= tilde.c
SRCS=     readline.c vi_mode.c funmap.c keymaps.c parens.c search.c \
	  rltty.c complete.c bind.c isearch.c display.c signals.c \
	  util.c kill.c undo.c macro.c input.c callback.c terminal.c \
	  nls.c xmalloc.c \
	  $(HISTSRC) $(TILDESRC)

INSTALLED_HEADERS= readline.h chardefs.h keymaps.h history.h tilde.h \
		   rlstdc.h rlconf.h

DPADD+= $(LIBTERMCAP)
LDADD+= -ltermcap

INCDIR=	/usr/include/readline

.for hdr in ${INSTALLED_HEADERS}
INCS+=	${SRCDIR}/${hdr}
.endfor

.include <bsd.lib.mk>
