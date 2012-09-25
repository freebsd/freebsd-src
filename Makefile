#	$NetBSD: Makefile,v 1.51 2012/08/10 12:20:10 joerg Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/4/93

USE_SHLIBDIR=	yes

WIDECHAR ?= yes
WARNS?=	5
LIB=	edit

LIBDPLIBS+=     terminfo ${.CURDIR}/../libterminfo

.include "bsd.own.mk"

COPTS+=	-Wunused-parameter
CWARNFLAGS.gcc+=	-Wconversion

OSRCS=	chared.c common.c el.c emacs.c fcns.c filecomplete.c help.c \
	hist.c keymacro.c map.c chartype.c \
	parse.c prompt.c read.c refresh.c search.c sig.c terminal.c tty.c vi.c

MAN=	editline.3 editrc.5

MLINKS=	editline.3 el_init.3 editline.3 el_end.3 editline.3 el_reset.3 \
	editline.3 el_gets.3 editline.3 el_getc.3 editline.3 el_push.3 \
	editline.3 el_parse.3 editline.3 el_set.3 editline.3 el_get.3 \
	editline.3 el_source.3 editline.3 el_resize.3 editline.3 el_line.3 \
	editline.3 el_insertstr.3 editline.3 el_deletestr.3 \
	editline.3 history_init.3 editline.3 history_end.3 \
	editline.3 history.3 \
	editline.3 tok_init.3 editline.3 tok_end.3 editline.3 tok_reset.3 \
	editline.3 tok_line.3 editline.3 tok_str.3

# For speed and debugging
#SRCS=   ${OSRCS} readline.c tokenizer.c history.c
# For protection
SRCS=	editline.c readline.c tokenizer.c history.c

.if ${WIDECHAR} == "yes"
OSRCS += eln.c
SRCS += tokenizern.c historyn.c
CLEANFILES+=tokenizern.c.tmp tokenizern.c historyn.c.tmp historyn.c
CPPFLAGS+=-DWIDECHAR
.endif

LIBEDITDIR?=${.CURDIR}

INCS= histedit.h
INCSDIR=/usr/include

CLEANFILES+=editline.c
CLEANFILES+=common.h.tmp editline.c.tmp emacs.h.tmp fcns.c.tmp fcns.h.tmp
CLEANFILES+=help.c.tmp help.h.tmp vi.h.tmp tc1.o tc1
CLEANFILES+=tokenizern.c.tmp tokenizern.c tokenizerw.c.tmp tokenizerw.c
CPPFLAGS+=-I. -I${LIBEDITDIR} 
CPPFLAGS+=-I. -I${.CURDIR}
CPPFLAGS+=#-DDEBUG_TTY -DDEBUG_KEY -DDEBUG_READ -DDEBUG -DDEBUG_REFRESH
CPPFLAGS+=#-DDEBUG_PASTE -DDEBUG_EDIT

AHDR=vi.h emacs.h common.h 
ASRC=${LIBEDITDIR}/vi.c ${LIBEDITDIR}/emacs.c ${LIBEDITDIR}/common.c

DPSRCS+=	${AHDR} fcns.h help.h fcns.c help.c
CLEANFILES+=	${AHDR} fcns.h help.h fcns.c help.c

SUBDIR=	readline

vi.h: vi.c makelist Makefile
	${_MKTARGET_CREATE}
	${HOST_SH} ${LIBEDITDIR}/makelist -h ${LIBEDITDIR}/vi.c \
	    > ${.TARGET}.tmp && \
	    mv ${.TARGET}.tmp ${.TARGET}

emacs.h: emacs.c makelist Makefile
	${_MKTARGET_CREATE}
	${HOST_SH} ${LIBEDITDIR}/makelist -h ${LIBEDITDIR}/emacs.c \
	    > ${.TARGET}.tmp && \
	    mv ${.TARGET}.tmp ${.TARGET}

common.h: common.c makelist Makefile
	${_MKTARGET_CREATE}
	${HOST_SH} ${LIBEDITDIR}/makelist -h ${LIBEDITDIR}/common.c \
	    > ${.TARGET}.tmp && \
	    mv ${.TARGET}.tmp ${.TARGET}

fcns.h: ${AHDR} makelist Makefile
	${_MKTARGET_CREATE}
	${HOST_SH} ${LIBEDITDIR}/makelist -fh ${AHDR} > ${.TARGET}.tmp && \
	    mv ${.TARGET}.tmp ${.TARGET}

fcns.c: ${AHDR} fcns.h help.h makelist Makefile
	${_MKTARGET_CREATE}
	${HOST_SH} ${LIBEDITDIR}/makelist -fc ${AHDR} > ${.TARGET}.tmp && \
	    mv ${.TARGET}.tmp ${.TARGET}

help.c: ${ASRC} makelist Makefile
	${_MKTARGET_CREATE}
	${HOST_SH} ${LIBEDITDIR}/makelist -bc ${ASRC} > ${.TARGET}.tmp && \
	    mv ${.TARGET}.tmp ${.TARGET}

help.h: ${ASRC} makelist Makefile
	${_MKTARGET_CREATE}
	${HOST_SH} ${LIBEDITDIR}/makelist -bh ${ASRC} > ${.TARGET}.tmp && \
	    mv ${.TARGET}.tmp ${.TARGET}

editline.c: ${OSRCS} makelist Makefile
	${_MKTARGET_CREATE}
	${HOST_SH} ${LIBEDITDIR}/makelist -e ${OSRCS:T} > ${.TARGET}.tmp && \
	    mv ${.TARGET}.tmp ${.TARGET}

tokenizern.c: makelist Makefile
	${_MKTARGET_CREATE}
	${HOST_SH} ${LIBEDITDIR}/makelist -n tokenizer.c > ${.TARGET}.tmp && \
	    mv ${.TARGET}.tmp ${.TARGET}

historyn.c: makelist Makefile
	${_MKTARGET_CREATE}
	${HOST_SH} ${LIBEDITDIR}/makelist -n history.c > ${.TARGET}.tmp && \
	    mv ${.TARGET}.tmp ${.TARGET}

tc1.o:	${LIBEDITDIR}/TEST/tc1.c

tc1:	libedit.a tc1.o 
	${_MKTARGET_LINK}
	${CC} ${LDFLAGS} ${.ALLSRC} -o ${.TARGET} libedit.a ${LDADD} -ltermlib

.include <bsd.lib.mk>
.include <bsd.subdir.mk>

# XXX
.if defined(HAVE_GCC) && ${HAVE_GCC} >= 45
COPTS.editline.c+=	-Wno-cast-qual
COPTS.tokenizer.c+=	-Wno-cast-qual
COPTS.tokenizern.c+=	-Wno-cast-qual
.endif
