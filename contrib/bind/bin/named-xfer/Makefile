## Copyright (c) 1996 by Internet Software Consortium
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

# $Id: Makefile,v 8.20 1997/06/19 03:22:09 halley Exp $

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
PIDDIR = /var/run
DESTBIN = /usr/local/bin
DESTSBIN = /usr/local/sbin
DESTEXEC = /usr/local/libexec
DESTMAN = /usr/share/man
DESTHELP= /usr/share/misc
AR= ar cruv
INSTALL= install
STRIP=-s

PS=ps
LDFLAGS=
CFLAGS=	${CDEBUG} -I${PORTINCL} -I${INCL}

NAMED_OBJS= \
	../named/db_glue.${O} ../named/ns_glue.${O} ../named/tmp_version.${O}

PROG= named-xfer
SRCS= ${PROG}.c
OBJS= ${PROG}.${O}

all: ${PROG}

${PROG}: ${OBJS} ${NAMED_OBJS} ${LIBBIND} Makefile
	${CC} ${CDEBUG} ${LDFLAGS} -o ${PROG} ${OBJS} ${NAMED_OBJS} \
		${LIBBIND} ${SYSLIBS}

distclean: clean

clean: FRC
	rm -f ${PROG} core .depend
	rm -f *.BAK *.CKP *~ *.${O} *.orig

depend: ${SRCS}
	mkdep -p ${CPPFLAGS} -I${INCL} -I${PORTINCL} ${SRCS}

${DESTDIR}${DESTEXEC}:
	mkdir -p ${DESTDIR}${DESTEXEC}

install: ${DESTDIR}${DESTSBIN} ${PROG}
	${INSTALL} ${STRIP} -c -m 755 ${PROG} ${DESTDIR}${DESTEXEC}/${PROG}

links: FRC
	@set -e; ln -s SRC/*.[ch] .

tags: FRC
	ctags ${SRCS} *.h

commands.c: commands.l
	${LEX} -t $< > $@ || rm $@

FRC:

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
