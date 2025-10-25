#	$NetBSD: Makefile,v 1.56 2016/03/02 19:24:20 christos Exp $

EDITDIR=	${SRCTOP}/contrib/libedit
.PATH: ${EDITDIR}

SHLIB_MAJOR=	8
SHLIBDIR?= /lib

LIB=	edit

LIBADD=	tinfow

SRCS=	chared.c chartype.c common.c el.c eln.c emacs.c filecomplete.c \
	hist.c history.c historyn.c keymacro.c literal.c map.c \
	parse.c prompt.c read.c readline.c refresh.c search.c sig.c \
	terminal.c tokenizer.c tokenizern.c tty.c vi.c

MAN=		editline.3
MANNODEV=	editrc.5 editline.7

MLINKS=\
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
	editline.3 tok_str.3 \
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

INCS=	histedit.h filecomplete.h

SRCS+=	common.h emacs.h fcns.h func.h help.h vi.h
CLEANFILES+= common.h emacs.h fcns.h func.h help.h vi.h

CFLAGS+= -I. -I${EDITDIR} -I${EDITDIR}/readline

AHDR=	vi.h emacs.h common.h
ASRC=	${EDITDIR}/vi.c ${EDITDIR}/emacs.c ${EDITDIR}/common.c

SUBDIR= readline

.for hdr in vi emacs common
${hdr}.h: ${hdr}.c makelist
	sh ${EDITDIR}/makelist -h ${EDITDIR}/${hdr}.c > ${.TARGET}
.endfor

fcns.h: ${AHDR} makelist
	sh ${EDITDIR}/makelist -fh ${AHDR} > ${.TARGET}

func.h: ${AHDR} makelist
	sh ${EDITDIR}/makelist -fc ${AHDR} > ${.TARGET}

help.h: ${ASRC} makelist
	sh ${EDITDIR}/makelist -bh ${ASRC} > ${.TARGET}

tc1.o: ${EDITDIR}/TEST/tc1.c

test: tc1.o libedit.a ${DPADD} ${LIBTERMCAP}
	${CC} ${CFLAGS} ${.ALLSRC} -o ${.TARGET} libedit.a ${LDADD}

.include "Makefile.inc"
.include <bsd.lib.mk>

CWARNFLAGS.chartype.c=	${NO_WUSE_AFTER_FREE}
