# $FreeBSD: src/gnu/lib/libreadline/readline/Makefile,v 1.13.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

SUBDIR= doc

LIB=    readline
MAN=    doc/readline.3
SHLIBDIR?=	/lib

TILDESRC= tilde.c
SRCS=     readline.c vi_mode.c funmap.c keymaps.c parens.c search.c \
	  rltty.c complete.c bind.c isearch.c display.c signals.c \
	  util.c kill.c undo.c macro.c input.c callback.c terminal.c \
	  text.c nls.c misc.c compat.c xmalloc.c $(HISTSRC) $(TILDESRC)

INSTALLED_HEADERS= readline.h chardefs.h keymaps.h history.h tilde.h \
		   rlstdc.h rlconf.h rltypedefs.h

DPADD=	${LIBTERMCAP}
LDADD=	-ltermcap

INCSDIR=${INCLUDEDIR}/readline

.for hdr in ${INSTALLED_HEADERS}
INCS+=	${SRCDIR}/${hdr}
.endfor

.include <bsd.lib.mk>
