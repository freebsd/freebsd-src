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

# $Id: Makefile,v 8.31 2001/08/14 05:57:57 marka Exp $

DESTDIR=
CC= cc
SHELL= /bin/sh

CDEBUG= -g -Wall

#(net2 and its descendents)
SYSTYPE = freebsd
TOP = ..
O=o
A=a
INCL = ${TOP}/include
LIBBIND = ${TOP}/lib/libbind.${A}
LIBPORT = ${TOP}/port/libport.${A}
PORTINCL = ${TOP}/port/${SYSTYPE}/include
LEX = lex -I
YACC = yacc
SYSLIBS = -ll -lutil
DESTBIN = /usr/local/bin
DESTSBIN = /usr/local/sbin
DESTEXEC = /usr/local/libexec
DESTMAN = /usr/share/man
DESTHELP= /usr/share/misc
AR= ar cru
INSTALL_EXEC=
INSTALL_LIB=-o bin -g bin

LDFLAGS=

MARGS =	"SYSTYPE=${SYSTYPE}" "SHELL=${SHELL}" "A=${A}" "O=${O}" \
	"CC=${CC}" "LEX=${LEX}" "YACC=${YACC}" "CDEBUG=${CDEBUG}" \
	"SYSLIBS=${SYSLIBS}" "LDFLAGS=${LDFLAGS}" \
	"DESTDIR=${DESTDIR}" "DESTMAN=${DESTMAN}" \
	"DESTBIN=${DESTBIN}" "DESTSBIN=${DESTSBIN}" "DESTEXEC=${DESTEXEC}" \
	"DESTLIB=${DESTLIB}" "DESTINC=${DESTINC}" "DESTETC=${DESTETC}" \
	"DESTRUN=${DESTRUN}" "DESTHELP=${DESTHELP}" \
	"RANLIB=${RANLIB}" "AR=${AR}" "ARPREF=${ARPREF}" "ARSUFF=${ARSUFF}" \
	"INCL=../${INCL}" "PORTINCL=../${PORTINCL}" "EXE=${EXE}" \
	"LIBBIND=../${LIBBIND}" "LIBPORT=../${LIBPORT}" \
	"INSTALL=${INSTALL}" "TOP=../${TOP}" \
	"VER=${VER}" "STRIP=${STRIP}" "PS=${PS}" "INSTALL_LIB=${INSTALL_LIB}" \
	"INSTALL_EXEC=${INSTALL_EXEC}" "BOUNDS=${BOUNDS}"

CFLAGS=	${CDEBUG}
CPPFLAGS= -I${PORTINCL} -I${INCL}

SUBDIRS = addr nslookup dig dnsquery host named named-xfer ndc nsupdate \
	mkservdb irpd dnskeygen named-bootconf

all: 
	@for x in ${SUBDIRS}; do \
		(cd $$x; pwd; ${MAKE} ${MARGS} $@) || exit 1; \
	done

install depend tags clean distclean::
	@for x in ${SUBDIRS}; do \
		(cd $$x; pwd; ${MAKE} ${MARGS} $@) || exit 1; \
	done

distclean:: clean

clean::
	rm -f *.BAK *.CKP *~ *.orig

links: FRC
	@set -e; \
	for x in $(SUBDIRS); do \
		( mkdir $$x; cd $$x; pwd; ln -s ../SRC/$$x SRC; \
		  cp SRC/Makefile Makefile; chmod +w Makefile; \
		  $(MAKE) $(MARGS) links; \
		) || exit 1; \
	done

FRC:

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
