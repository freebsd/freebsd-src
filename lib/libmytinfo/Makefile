# Makefile for libmytinfo
# $Id$

LIB=    mytinfo
BINDIR= /usr/bin
SRCS=   addstr.c binorder.c buildpath.c caplist.c capsort.c compar.c\
	fillterm.c findcap.c findterm.c getother.c gettbin.c\
	gettcap.c gettinfo.c tcapconv.c tcapvars.c termcap.c\
	terminfo.c tgoto.c tiget.c tmatch.c tparm.c tputs.c tty.c version.c
MAN1=   tconv.1
MLINKS= tconv.1 tic.1  tconv.1 captoinfo.1
CLEANFILES+= binorder.c caplist.c capsort.c mkbinorder mkcaplist \
		mkcapsort mktermhead mkversion readcaps.o term.h* \
		version.c quit.o tconv

CFLAGS+=-Wall

CAPS=   1000

all: $${_LIBS} tconv

beforeinstall: term.h
	-cmp -s term.h ${DESTDIR}/usr/include/term.h > \
	    /dev/null 2>&1 || \
	    $(INSTALL) $(COPY) -o ${BINOWN} -g ${BINGRP} -m 444 term.h \
	    ${DESTDIR}/usr/include

afterinstall:
	$(INSTALL) $(COPY) $(STRIP) -o $(BINOWN) -g $(BINGRP) -m $(BINMODE) \
		tconv ${DESTDIR}${BINDIR}
	rm -f ${DESTDIR}${BINDIR}/tic
	ln ${DESTDIR}${BINDIR}/tconv ${DESTDIR}${BINDIR}/tic
	rm -f ${DESTDIR}${BINDIR}/captoinfo
	ln ${DESTDIR}${BINDIR}/tconv ${DESTDIR}${BINDIR}/captoinfo

capsort.c: mkcapsort
	./mkcapsort > capsort.c

mkcapsort: mkcapsort.c caplist.o compar.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o mkcapsort ${.CURDIR}/mkcapsort.c caplist.o compar.o

binorder.c: mkbinorder ${.CURDIR}/bin_order
	./mkbinorder ${.CURDIR}/bin_order > binorder.c

mkbinorder: mkbinorder.c capsort.o caplist.o compar.o findcap.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o mkbinorder ${.CURDIR}/mkbinorder.c capsort.o \
		caplist.o compar.o findcap.o

version.c: mkversion
	./mkversion > version.c

mkversion: mkversion.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o mkversion ${.CURDIR}/mkversion.c

term.h: term.head ${.CURDIR}/term.tail
	cat term.head ${.CURDIR}/term.tail > term.h

term.head: mktermhead ${.CURDIR}/cap_list
	./mktermhead -n $(CAPS) ${.CURDIR}/cap_list > term.head

caplist.c: mkcaplist ${.CURDIR}/cap_list
	./mkcaplist -n $(CAPS) ${.CURDIR}/cap_list > caplist.c

mktermhead: mktermhead.c readcaps.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o mktermhead ${.CURDIR}/mktermhead.c readcaps.o

mkcaplist: mkcaplist.c readcaps.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o mkcaplist ${.CURDIR}/mkcaplist.c readcaps.o

tconv: tconv.c quit.o $${_LIBS}
	$(CC) ${CFLAGS} $(LDFLAGS) -L. -o tconv ${.CURDIR}/tconv.c quit.o -l$(LIB)

.include <bsd.lib.mk>

