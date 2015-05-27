# $FreeBSD$

LIB=    readline
INTERNALLIB=	yes
NO_MAN=	yes

TILDESRC= tilde.c
SRCS=     readline.c vi_mode.c funmap.c keymaps.c parens.c search.c \
	  rltty.c complete.c bind.c isearch.c display.c signals.c \
	  util.c kill.c undo.c macro.c input.c callback.c terminal.c \
	  text.c nls.c misc.c compat.c xmalloc.c $(HISTSRC) $(TILDESRC)

INSTALLED_HEADERS= readline.h chardefs.h keymaps.h history.h tilde.h \
		   rlstdc.h rlconf.h rltypedefs.h

CFLAGS+=	-DREADLINE_LIBRARY
CFLAGS+=	-I${.OBJDIR}/..
SRCDIR=	${.CURDIR}/../../../../contrib/libreadline

CLEANFILES+= ${INSTALLED_HEADERS}
beforebuild: ${INSTALLED_HEADERS}
.for h in ${INSTALLED_HEADERS}
$h: ${SRCDIR}/$h
	ln -sf ${SRCDIR}/$h $h
.endfor

LIBADD=	ncursesw

.include <bsd.lib.mk>
