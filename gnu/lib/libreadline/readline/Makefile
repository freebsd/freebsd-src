# $FreeBSD$

SUBDIR= doc

LIB=    readline
MAN=    doc/readline.3

TILDESRC= tilde.c
SRCS=     readline.c vi_mode.c funmap.c keymaps.c parens.c search.c \
	  rltty.c complete.c bind.c isearch.c display.c signals.c \
	  util.c kill.c undo.c macro.c input.c callback.c terminal.c \
	  nls.c compat.c xmalloc.c $(HISTSRC) $(TILDESRC)

INSTALLED_HEADERS= readline.h chardefs.h keymaps.h history.h tilde.h \
		   rlstdc.h rlconf.h rltypedefs.h

DPADD=	${LIBNCURSES}
LDADD=	-lncurses

INCDIR=	/usr/include/readline

.for hdr in ${INSTALLED_HEADERS}
INCS+=	${SRCDIR}/${hdr}
.endfor

.include <bsd.lib.mk>
