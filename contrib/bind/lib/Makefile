# Copyright (c) 1996 by Internet Software Consortium
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
# ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
# CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
# PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
# ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
# SOFTWARE.

# $Id: Makefile,v 8.10 1997/09/26 17:56:00 halley Exp $

SUBDIRS = resolv irs isc bsd inet nameser

# these are only appropriate for BSD 4.4 or derivatives, and are used in
# development.  normal builds will be done in the top level directory and
# this Makefile will be invoked with a lot of overrides for the following:
SYSTYPE= bsdos
SHELL= /bin/sh
O=o
A=a
DESTDIR=
DESTINC= /usr/local/bind/include
DESTLIB= /usr/local/bind/lib
TOP= ..
INCL= ${TOP}/include
PORTINCL= ${TOP}/port/${SYSTYPE}/include
LIBBIND= ${TOP}/lib/libbind.${A}
LIBPORT= ${TOP}/port/libport.${A}
RANLIB= ranlib
AR= ar cruv
INSTALL= install
CDEBUG= -g

# Warning: this MARGS has RANLIB=: to prevent submakes from running ranlib
MARGS =	"SYSTYPE=${SYSTYPE}" "SHELL=${SHELL}" "A=${A}" "O=${O}" \
	"CC=${CC}" "LEX=${LEX}" "YACC=${YACC}" "CDEBUG=${CDEBUG}" \
	"SYSLIBS=${SYSLIBS}" "LDFLAGS=${LDFLAGS}" \
	"DESTDIR=${DESTDIR}" "PIDDIR=${PIDDIR}" "DESTMAN=${DESTMAN}" \
	"DESTBIN=${DESTBIN}" "DESTSBIN=${DESTSBIN}" "DESTEXEC=${DESTEXEC}" \
	"DESTLIB=${DESTLIB}" "DESTINC=${DESTINC}" "DESTHELP=${DESTHELP}" \
	"RANLIB=:" "AR=${AR}" "ARPREF=${ARPREF}" "ARSUFF=${ARSUFF}" \
	"INCL=../${INCL}" "PORTINCL=../${PORTINCL}" \
	"LIBBIND=../${LIBBIND}" "LIBPORT=../${LIBPORT}" \
	"INSTALL=${INSTALL}" "CPPFLAGS=${CPPFLAGS}" "TOP=../${TOP}"

LIB = libbind.${A}
LIBTS = ${TOP}/lib/libbind.ts

all depend clean install distclean::
	@for x in ${SUBDIRS}; do \
		(cd $$x; pwd; ${MAKE} ${MARGS} $@); \
	done

all:: ${LIBTS}

${LIBTS}: ${LIBBIND}
	${RANLIB} ${LIBBIND}
	touch ${LIBTS}

distclean:: clean

clean:: FRC
	rm -f *~ *.BAK *.CKP *.orig
	rm -f ${LIBBIND} ${LIBTS}

install:: ${DESTDIR}${DESTLIB} ${DESTDIR}${DESTLIB}/${LIB}

${DESTDIR}${DESTLIB}:
	mkdir -p ${DESTDIR}${DESTLIB}

${DESTDIR}${DESTLIB}/${LIB}: ${LIBBIND}
	${INSTALL} -c -o bin -g bin -m 644 ${LIBBIND} \
		${DESTDIR}${DESTLIB}/${LIB}
	( cd ${DESTDIR}${DESTLIB} ; ${RANLIB} ${LIB} )

links:: FRC
	@set -e; for x in ${SUBDIRS}; do \
		( mkdir $$x; cd $$x; pwd; ln -s ../SRC/$$x SRC; \
		  cp SRC/Makefile Makefile; chmod +w Makefile; \
		  ${MAKE} ${MARGS} links; \
		); \
	done

FRC:
