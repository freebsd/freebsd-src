## Copyright (c) 1996, 1997 by Internet Software Consortium
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

# $Id: Makefile,v 1.6 2000/07/17 07:15:08 vixie Exp $

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
EXE=
LEX = lex -I
YACC = yacc -d
SYSLIBS = -ll -lutil
DESTBIN = /usr/local/bin
DESTSBIN = /usr/local/sbin
DESTEXEC = /usr/local/libexec
DESTMAN = /usr/share/man
DESTHELP= /usr/share/misc
DESTETC= /etc
DESTRUN= /var/run
AR= ar cru
INSTALL= install
STRIP=-s

PS=ps
LDFLAGS=
CFLAGS=	${CDEBUG}
CPPFLAGS= -I${PORTINCL} -I${INCL} -I${TOP}/lib/irs ${DEFS}

VER= LOCAL-`date +%y%m%d.%H%M%S`
HOSTNAMECMD= hostname || uname -n

PROG=	irpd
HDRS=	
SRCS=	irpd.c
OBJS=	irpd.${O}

all: ${PROG}${EXE}

${PROG}${EXE}: irpd.${O} tmp_version.${O} ${LIBBIND}
	${CC} ${CDEBUG} ${LDFLAGS} -o ${PROG}${EXE} ${OBJS} tmp_version.${O} \
		${LIBBIND} ${SYSLIBS}
.c.${O}:
	${CC} ${CPPFLAGS} ${CFLAGS} ${BOUNDS} -c $*.c

tmp_version.${O}: tmp_version.c

tmp_version.c: version.c Makefile ../Makefile ${SRCS} ${HDRS}
	(u=$${USER-root} d=`pwd` h=`${HOSTNAMECMD}` t=`date`; \
	sed -e "s|%WHEN%|$${t}|" -e "s|%VERSION%|"${VER}"|" \
	    -e "s|%WHOANDWHERE%|$${u}@$${h}:$${d}|" \
	    < version.c > tmp_version.c); sleep 1

distclean: clean

clean: FRC
	rm -f ${PROG}${EXE} ${OBJS} core .depend
	rm -f *.BAK *.CKP *~ *.orig
	rm -f tmp_version.c tmp_version.${O}

depend: ${SRCS}
	mkdep ${CPPFLAGS} -I${INCL} -I${PORTINCL} -I${TOP}/lib/irs ${DEFS} ${SRCS}

${DESTDIR}${DESTSBIN}:
	mkdir -p ${DESTDIR}${DESTSBIN}

install: ${DESTDIR}${DESTSBIN} ${PROG}${EXE}
	${INSTALL} ${STRIP} -c -m 755 ${PROG}${EXE} ${DESTDIR}${DESTSBIN}/${PROG}${EXE}

links: FRC
	@ln -s SRC/*.[chy] SRC/test .; rm -f ns_parser.[ch]

tags: FRC
	ctags ${SRCS} *.h

FRC:

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
