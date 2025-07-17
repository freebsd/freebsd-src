#	$NetBSD: Makefile,v 1.70 2023/08/03 14:56:36 rin Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/4/93

USE_SHLIBDIR=	yes

WARNS?=	5
LIB=	edit

LIBDPLIBS+=     terminfo ${.CURDIR}/../libterminfo

.include "bsd.own.mk"

COPTS+=	-Wunused-parameter
CWARNFLAGS.gcc+=	-Wconversion
CWARNFLAGS.clang+=	-Wno-cast-qual

SRCS =	chared.c chartype.c common.c el.c eln.c emacs.c filecomplete.c \
	hist.c history.c historyn.c keymacro.c literal.c map.c \
	parse.c prompt.c read.c readline.c refresh.c search.c sig.c \
	terminal.c tokenizer.c tokenizern.c tty.c vi.c

MAN=	editline.3 editrc.5 editline.7

FILES+=			libedit.pc
FILESOWN_libedit.pc=	${BINOWN}
FILESGRP_libedit.pc=	${BINGRP}
FILESMODE_libedit.pc=	${NONBINMODE}
FILESDIR_libedit.pc=	/usr/lib/pkgconfig

MLINKS= \
editline.3 el_deletestr.3 \
editline.3 el_end.3 \
editline.3 el_get.3 \
editline.3 el_getc.3 \
editline.3 el_gets.3 \
editline.3 el_init.3 \
editline.3 el_init_fd.3 \
editline.3 el_insertstr.3 \
editline.3 el_line.3 \
editline.3 el_parse.3 \
editline.3 el_push.3 \
editline.3 el_reset.3 \
editline.3 el_resize.3 \
editline.3 el_set.3 \
editline.3 el_source.3 \
editline.3 history.3 \
editline.3 history_end.3 \
editline.3 history_init.3 \
editline.3 tok_end.3 \
editline.3 tok_init.3 \
editline.3 tok_line.3 \
editline.3 tok_reset.3 \
editline.3 tok_str.3

MLINKS+= \
editline.3 el_wdeletestr.3 \
editline.3 el_wget.3 \
editline.3 el_wgetc.3 \
editline.3 el_wgets.3 \
editline.3 el_winsertstr.3 \
editline.3 el_wline.3 \
editline.3 el_wparse.3 \
editline.3 el_wpush.3 \
editline.3 el_wset.3 \
editline.3 history_w.3 \
editline.3 history_wend.3 \
editline.3 history_winit.3 \
editline.3 tok_wend.3 \
editline.3 tok_winit.3 \
editline.3 tok_wline.3 \
editline.3 tok_wreset.3 \
editline.3 tok_wstr.3

LIBEDITDIR?=${.CURDIR}

INCS= histedit.h
INCSDIR=/usr/include

CLEANFILES+=common.h.tmp emacs.h.tmp fcns.h.tmp func.h.tmp
CLEANFILES+=help.h.tmp vi.h.tmp tc1.o tc1 .depend

CPPFLAGS+=-I. -I${LIBEDITDIR}
CPPFLAGS+=-I. -I${.CURDIR}
#CPPFLAGS+=-DDEBUG_TTY -DDEBUG_KEY -DDEBUG -DDEBUG_REFRESH
#CPPFLAGS+=-DDEBUG_PASTE -DDEBUG_EDIT

AHDR=vi.h emacs.h common.h
ASRC=${LIBEDITDIR}/vi.c ${LIBEDITDIR}/emacs.c ${LIBEDITDIR}/common.c

DPSRCS+=	${AHDR} fcns.h func.h help.h
CLEANFILES+=	${AHDR} fcns.h func.h help.h

SUBDIR=	readline

.depend: ${AHDR} fcns.h func.h help.h

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

func.h: ${AHDR} makelist Makefile
	${_MKTARGET_CREATE}
	${HOST_SH} ${LIBEDITDIR}/makelist -fc ${AHDR} > ${.TARGET}.tmp && \
	    mv ${.TARGET}.tmp ${.TARGET}

help.h: ${ASRC} makelist Makefile
	${_MKTARGET_CREATE}
	${HOST_SH} ${LIBEDITDIR}/makelist -bh ${ASRC} > ${.TARGET}.tmp && \
	    mv ${.TARGET}.tmp ${.TARGET}

tc1.o:	${LIBEDITDIR}/TEST/tc1.c

tc1:	libedit.a tc1.o
	${_MKTARGET_LINK}
	${CC} ${LDFLAGS} ${.ALLSRC} -o ${.TARGET} libedit.a ${LDADD} -ltermlib

.include <bsd.lib.mk>
.include <bsd.subdir.mk>

# XXX
.if defined(HAVE_GCC)
COPTS.editline.c+=	-Wno-cast-qual
COPTS.literal.c+=	-Wno-sign-conversion
COPTS.tokenizer.c+=	-Wno-cast-qual
COPTS.tokenizern.c+=	-Wno-cast-qual
.endif

COPTS.history.c+=	${CC_WNO_STRINGOP_OVERFLOW}
COPTS.historyn.c+=	${CC_WNO_STRINGOP_OVERFLOW}
COPTS.readline.c+=	${CC_WNO_STRINGOP_TRUNCATION} ${CC_WNO_STRINGOP_OVERFLOW}
