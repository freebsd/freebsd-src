## Copyright (c) 1999 by Internet Software Consortium
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

# $Id: Makefile,v 1.2 2000/07/11 06:41:33 vixie Exp $

DESTDIR=
CC= cc
SHELL= /bin/sh

CDEBUG= -g

#(net2 and its descendents)
SYSTYPE = bsdos
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
STRIP=-s
INSTALL_EXEC=
INSTALL_LIB=-o bin -g bin

LDFLAGS=
CFLAGS=	${CDEBUG}
CPPFLAGS= -I${PORTINCL} -I${INCL}

PROG= named-bootconf

all: ${PROG}

${PROG}: ${PROG}.sh Makefile
	cp ${PROG}.sh ${PROG}
	chmod +x ${PROG}

distclean: clean

clean: FRC
	rm -f ${PROG}
	rm -f *.BAK *.CKP *~ *.orig

depend:

${DESTDIR}${DESTSBIN}:
	mkdir -p ${DESTDIR}${DESTSBIN}

install: ${DESTDIR}${DESTSBIN} ${PROG}
	${INSTALL} -c -m 755 ${PROG} ${DESTDIR}${DESTSBIN}/${PROG}

links: FRC
	@set -e; ln -s SRC/*.sh .

tags:

FRC:

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
