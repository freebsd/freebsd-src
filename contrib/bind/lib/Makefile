# Copyright (c) 1996,1999 by Internet Software Consortium
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

# $Id: Makefile,v 8.25 2000/12/23 08:02:56 vixie Exp $

SUBDIRS = resolv irs isc bsd inet nameser dst cylink dnssafe

# these are only appropriate for FreeBSD or similar, and are used in
# development.  normal builds will be done in the top level directory and
# this Makefile will be invoked with a lot of overrides for the following:
SYSTYPE= freebsd
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
LIBBINDR= ${TOP}/lib/libbind_r.${A}
LIBPORT= ${TOP}/port/libport.${A}
RANLIB= ranlib
AR= ar cru
INSTALL= install
CDEBUG= -g
REENTRANT=-D_REENTRANT
INSTALL_EXEC=
INSTALL_LIB=-o bin -g bin

# Warning: this MARGS has RANLIB=: to prevent submakes from running ranlib
MARGS =	"SYSTYPE=${SYSTYPE}" "SHELL=${SHELL}" "A=${A}" "O=${O}" \
	"CC=${CC}" "LEX=${LEX}" "YACC=${YACC}" "CDEBUG=${CDEBUG}" \
	"SYSLIBS=${SYSLIBS}" "LDFLAGS=${LDFLAGS}" \
	"DESTDIR=${DESTDIR}" "DESTMAN=${DESTMAN}" \
	"DESTBIN=${DESTBIN}" "DESTSBIN=${DESTSBIN}" "DESTEXEC=${DESTEXEC}" \
	"DESTLIB=${DESTLIB}" "DESTINC=${DESTINC}" "DESTHELP=${DESTHELP}" \
	"RANLIB=:" "AR=${AR}" "ARPREF=${ARPREF}" "ARSUFF=${ARSUFF}" \
	"INCL=../${INCL}" "PORTINCL=../${PORTINCL}" "EXE=${EXE}" \
	"LIBBIND=../${LIBBIND}" "LIBPORT=../${LIBPORT}" \
	"INSTALL=${INSTALL}" "TOP=../${TOP}" \
	"REENTRANT=${REENTRANT}" "INSTALL_LIB=${INSTALL_LIB}" \
	"INSTALL_EXEC=${INSTALL_EXEC}" "BOUNDS=${BOUNDS}"

LIB = libbind.${A}
LIBTS = ${TOP}/lib/libbind.ts
LIBR = libbind_r.${A}
LIBRTS = ${TOP}/lib/libbindr.ts

all depend clean install distclean::
	@for x in ${SUBDIRS}; do \
		(cd $$x; pwd; ${MAKE} ${MARGS} $@) || exit 1; \
	done

all:: ${LIBTS} ${LIBRTS}

${LIBRTS}: ${LIBBINDR}
	${RANLIB} ${LIBBINDR}
	sleep 1 && touch ${LIBRTS}

${LIBTS}: ${LIBBIND}
	${RANLIB} ${LIBBIND}
	sleep 1 && touch ${LIBTS}

distclean:: clean

clean:: FRC
	rm -f *~ *.BAK *.CKP *.orig
	rm -f ${LIBBIND} ${LIBTS}
	rm -f ${LIBBINDR} ${LIBRTS}

install:: ${DESTDIR}${DESTLIB} ${DESTDIR}${DESTLIB}/${LIB}
install:: ${DESTDIR}${DESTLIB} ${DESTDIR}${DESTLIB}/${LIBR}

${DESTDIR}${DESTLIB}:
	mkdir -p ${DESTDIR}${DESTLIB}

${DESTDIR}${DESTLIB}/${LIBR}: ${LIBBINDR}
	${INSTALL} -c ${INSTALL_LIB} -m 644 ${LIBBINDR} \
		${DESTDIR}${DESTLIB}/${LIBR}
	( cd ${DESTDIR}${DESTLIB} ; ${RANLIB} ${LIBR} )

${DESTDIR}${DESTLIB}/${LIB}: ${LIBBIND}
	${INSTALL} -c ${INSTALL_LIB} -m 644 ${LIBBIND} \
		${DESTDIR}${DESTLIB}/${LIB}
	( cd ${DESTDIR}${DESTLIB} ; ${RANLIB} ${LIB} )

links:: FRC
	@set -e; for x in ${SUBDIRS}; do \
		( mkdir $$x; cd $$x; pwd; ln -s ../SRC/$$x SRC; \
		  cp SRC/Makefile Makefile; chmod +w Makefile; \
		  ${MAKE} ${MARGS} links; \
		) || exit 1; \
	done

FRC:
