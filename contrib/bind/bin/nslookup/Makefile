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

# $Id: Makefile,v 8.18 1997/12/03 22:55:16 halley Exp $

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
STRIP=-s

LDFLAGS=
DEFS=	-D_PATH_HELPFILE=\"${DESTDIR}${DESTHELP}/nslookup.help\"
CFLAGS=	${CDEBUG} -I${PORTINCL} -I${INCL} ${DEFS}

PROG= nslookup
CSRCS=	main.c getinfo.c debug.c send.c skip.c list.c subr.c
SRCS=	${CSRCS} commands.l
OBJS=	main.${O} getinfo.${O} debug.${O} send.${O} skip.${O} list.${O} \
	subr.${O} commands.${O}

all: ${PROG}

${PROG}: ${OBJS} ${LIBBIND} Makefile
	${CC} ${CDEBUG} ${LDFLAGS} -o ${PROG} ${OBJS} \
		${LIBBIND} ${SYSLIBS}

distclean: clean
	rm -f commands.c

clean: FRC
	rm -f ${PROG} ${OBJS} core .depend
	rm -f *.BAK *.CKP *~ *.orig
	rm -f lex.yy.c lex.yy.o

depend: ${SRCS}
	mkdep ${CPPFLAGS} -I${INCL} -I${PORTINCL} ${DEFS} ${CSRCS}

${DESTDIR}${DESTHELP}:
	mkdir -p ${DESTDIR}${DESTHELP}

${DESTDIR}${DESTBIN}:
	mkdir -p ${DESTDIR}${DESTBIN}

install: ${DESTDIR}${DESTBIN} ${DESTDIR}${DESTHELP} ${PROG}
	${INSTALL} ${STRIP} -c -m 755 ${PROG} ${DESTDIR}${DESTBIN}/${PROG}
	${INSTALL} -c -o bin -g bin -m 444 nslookup.help ${DESTDIR}${DESTHELP}/

links: FRC
	@set -e; ln -s SRC/*.[chl] SRC/nslookup.help .

tags: FRC
	ctags ${CSRCS} *.h

commands.c: commands.l
	${LEX} -t $< > $@ || rm $@

FRC:

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
