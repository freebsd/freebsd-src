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

CFLAGS+=	-I${.OBJDIR}/..
SRCDIR=	${.CURDIR}/../../../../contrib/libreadline

beforebuild: ${INSTALLED_HEADERS}
CLEANFILES+= ${INSTALLED_HEADERS}
DPSRCS+= ${INSTALLED_HEADERS}

.for _h in ${INSTALLED_HEADERS}
${_h}:	${SRCDIR}/${_h}
	ln -sf ${.ALLSRC} ${.TARGET}
.endfor

LIBADD=	ncursesw

.include <bsd.lib.mk>
