#	$NetBSD: Makefile,v 1.34 2005/05/28 12:02:53 lukem Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

LIB=	edit
SHLIB_MAJOR=	5
SHLIBDIR?= /lib

OSRCS=	chared.c common.c el.c emacs.c fcns.c help.c hist.c key.c map.c \
	parse.c prompt.c read.c refresh.c search.c sig.c term.c tty.c vi.c

DPADD=	${LIBNCURSES}
LDADD=	-lncurses

MAN=	editline.3 editrc.5

MLINKS=	editline.3 el_deletestr.3 editline.3 el_end.3 editline.3 el_init.3 \
	editline.3 el_get.3 editline.3 el_getc.3 editline.3 el_gets.3 \
	editline.3 history.3 editline.3 history_end.3 \
	editline.3 history_init.3 editline.3 el_insertstr.3 \
	editline.3 el_line.3 editline.3 el_parse.3 editline.3 el_push.3 \
	editline.3 el_reset.3 editline.3 el_resize.3 editline.3 el_set.3 \
	editline.3 el_source.3 \
	editline.3 tok_init.3 editline.3 tok_end.3 editline.3 tok_reset.3 \
	editline.3 tok_line.3 editline.3 tok_str.3

# For speed and debugging
#SRCS=   ${OSRCS} tokenizer.c history.c
# For protection
SRCS=	editline.c tokenizer.c history.c
SRCS+=	common.h emacs.h fcns.h help.h vi.h
CLEANFILES+= common.h editline.c emacs.h fcns.c fcns.h help.c help.h vi.h

CFLAGS+= -I. -I${.CURDIR}
CFLAGS+= #-DDEBUG_TTY -DDEBUG_KEY -DDEBUG_READ -DDEBUG -DDEBUG_REFRESH
CFLAGS+= #-DDEBUG_PASTE -DDEBUG_EDIT

AHDR=	vi.h emacs.h common.h
ASRC=	${.CURDIR}/vi.c ${.CURDIR}/emacs.c ${.CURDIR}/common.c

.for hdr in vi emacs common
${hdr}.h: ${hdr}.c makelist
	sh ${.CURDIR}/makelist -h ${.CURDIR}/${hdr}.c > ${.TARGET}
.endfor

fcns.h: ${AHDR} makelist
	sh ${.CURDIR}/makelist -fh ${AHDR} > ${.TARGET}

fcns.c: ${AHDR} fcns.h makelist
	sh ${.CURDIR}/makelist -fc ${AHDR} > ${.TARGET}

help.c: ${ASRC} makelist
	sh ${.CURDIR}/makelist -bc ${ASRC} > ${.TARGET}

help.h: ${ASRC} makelist
	sh ${.CURDIR}/makelist -bh ${ASRC} > ${.TARGET}

editline.c: ${OSRCS}
	sh ${.CURDIR}/makelist -e ${.ALLSRC:T} > ${.TARGET}

# minimal dependency to make "make depend" optional
editline.o editline.po editline.So editline.ln:	\
	common.h emacs.h fcns.c fcns.h help.c help.h vi.h

test.o: ${.CURDIR}/TEST/test.c

test: test.o libedit.a ${DPADD} ${LIBTERMCAP}
	${CC} ${CFLAGS} ${.ALLSRC} -o ${.TARGET} libedit.a ${LDADD}

.include <bsd.lib.mk>
