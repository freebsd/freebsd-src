## Copyright (c) 1996,1999 by Internet Software Consortium
##
## Permission to use, copy, modify, and distribute this software for any
## purpose with or without fee is hereby granted, provided that the above
## copyright notice and this permission notice appear in all copies.
##
## THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
## ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
## OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
## CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
## DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
## PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
## ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
## SOFTWARE.

# $Id: Makefile,v 1.17 2000/12/23 08:02:54 vixie Exp $

DESTDIR=
CC= cc
SHELL= /bin/sh

CDEBUG= -g -Wall

#(net2 and its descendents)
SYSTYPE = freebsd
TOP = ../..
INCL =  ${TOP}/include
PORTINCL = ${TOP}/port/${SYSTYPE}/include
LIBBIND = ${TOP}/lib/libbind.a
A=a
O=o
LEX = lex -I
SYSLIBS = -ll -lutil
DESTBIN = /usr/local/bin
DESTSBIN = /usr/local/sbin
DESTEXEC = /usr/local/libexec
DESTMAN = /usr/share/man
DESTHELP= /usr/share/misc
AR= ar cru
INSTALL= install
STRIP=-s
INSTALL_EXEC=
INSTALL_LIB=-o bin -g bin
PS=ps

LDFLAGS=
CFLAGS=	${CDEBUG}
CPPFLAGS= -I${PORTINCL} -I${INCL} ${DEFS}

PROG= ndc
SRCS= ${PROG}.c
OBJS= ${PROG}.${O}
HDRS= pathnames.h

all: ${PROG}${EXE}

${PROG}${EXE}: ${HDRS} ${OBJS} ${LIBBIND} Makefile
	${CC} ${CDEBUG} ${LDFLAGS} ${BOUNDS} -o ${PROG}${EXE} ${OBJS} ${LIBBIND} ${SYSLIBS}

.c.${O}:
	${CC} ${CPPFLAGS} ${CFLAGS} ${BOUNDS} -c $*.c

pathnames.h: ${TOP}/.settings Makefile ../named/pathtemplate.h
	rm -f pathnames.h
	sed -e "s|%DESTSBIN%|${DESTSBIN}|" \
	    -e "s|%DESTEXEC%|${DESTEXEC}|" \
	    -e "s|%DESTETC%|${DESTETC}|" \
	    -e "s|%DESTRUN%|${DESTRUN}|" \
	    < ../named/pathtemplate.h > pathnames.h

distclean: clean

clean: FRC
	rm -f ${PROG}${EXE} ${OBJS} core .depend
	rm -f *.BAK *.CKP *~ *.orig
	rm -f pathnames.h

depend: ${SRCS} pathnames.h
	mkdep ${CPPFLAGS} -I${INCL} -I${PORTINCL} ${SRCS}

${DESTDIR}${DESTSBIN}:
	mkdir -p ${DESTDIR}${DESTSBIN}

install: ${DESTDIR}${DESTSBIN} ${PROG}${EXE}
	${INSTALL}  ${INSTALL_EXEC} ${STRIP} -c -m 755 ${PROG}${EXE} ${DESTDIR}${DESTSBIN}/${PROG}${EXE}

links: FRC
	@set -e; ln -s SRC/*.[ch] .

tags: FRC
	ctags ${SRCS} *.h

FRC:

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
