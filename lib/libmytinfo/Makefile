# Makefile for libmytinfo
# Use 'make update_term_h' manually after changing internal
# mytinfo structures
# $Id: Makefile,v 1.12 1997/02/22 15:07:29 peter Exp $

LIB=    mytinfo
SRCS=   addstr.c binorder.c buildpath.c caplist.c capsort.c compar.c\
	fillterm.c findcap.c findterm.c getother.c gettbin.c\
	gettcap.c gettinfo.c tcapconv.c tcapvars.c termcap.c\
	terminfo.c tgoto.c tiget.c tmatch.c tparm.c tputs.c tty.c version.c
CLEANFILES+= binorder.c caplist.c capsort.c mkbinorder mkcaplist \
	mkcapsort mktermhead mkversion readcaps.o version.c

CFLAGS+=-Wall -I${.CURDIR}

CAPS=   1000

# ${.CURDIR}/term.h: ${.CURDIR}/term.head ${.CURDIR}/term.tail
update_term_h: ${.CURDIR}/term.head ${.CURDIR}/term.tail
	cat ${.CURDIR}/term.head ${.CURDIR}/term.tail > ${.CURDIR}/term.h

beforeinstall:
	$(INSTALL) -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/term.h \
		${DESTDIR}/usr/include
	ln -sf term.h ${DESTDIR}/usr/include/nterm.h

build-tools: mkbinorder mkcaplist mkcapsort mkversion

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

${.CURDIR}/term.head: mktermhead ${.CURDIR}/cap_list
	./mktermhead -n $(CAPS) ${.CURDIR}/cap_list > ${.CURDIR}/term.head

caplist.c: mkcaplist ${.CURDIR}/cap_list
	./mkcaplist -n $(CAPS) ${.CURDIR}/cap_list > caplist.c

mktermhead: mktermhead.c readcaps.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o mktermhead ${.CURDIR}/mktermhead.c readcaps.o

mkcaplist: mkcaplist.c readcaps.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o mkcaplist ${.CURDIR}/mkcaplist.c readcaps.o

.include <bsd.lib.mk>
