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

# $Id: Makefile,v 8.16 2000/11/21 12:54:42 marka Exp $

# these are only appropriate for BSD 4.4 or derivatives, and are used in
# development.  normal builds will be done in the top level directory and
# this Makefile will be invoked with a lot of overrides for the following:
SYSTYPE= bsdos
SHELL= /bin/sh
O=o
A=a
DESTDIR=
DESTINC= /usr/local/bind/include
DESTLIB= /usr/local/lib
TOP= ..
INCL= ${TOP}/include
PORTINCL= ${TOP}/port/${SYSTYPE}/include
LIBBIND= ${TOP}/lib/libbind.${A}
RANLIB= ranlib
AR= ar cru
INSTALL= install
CDEBUG= -g

SUBDIRS = bsdos decunix linux netbsd sunos ultrix

MARGS =	"SYSTYPE=${SYSTYPE}" "SHELL=${SHELL}" "A=${A}" "O=${O}" \
	"CC=${CC}" "LEX=${LEX}" "CDEBUG=${CDEBUG}" \
	"SYSLIBS=${SYSLIBS}" "LDFLAGS=${LDFLAGS}" \
	"DESTDIR=${DESTDIR}" "DESTMAN=${DESTMAN}" \
	"DESTBIN=${DESTBIN}" "DESTSBIN=${DESTSBIN}" "DESTEXEC=${DESTEXEC}" \
	"DESTLIB=${DESTLIB}" "DESTINC=${DESTINC}" "DESTHELP=${DESTHELP}" \
	"RANLIB=${RANLIB}" "AR=${AR}" "ARPREF=${ARPREF}" "ARSUFF=${ARSUFF}" \
	"INCL=../${INCL}" "PORTINCL=../${PORTINCL}" \
	"LIBBIND=../${LIBBIND}" \
	"INSTALL=${INSTALL}" "CPPFLAGS=${CPPFLAGS}" "TOP=../${TOP}"

all::

all depend install::
	@(cd ${SYSTYPE}; pwd; ${MAKE} ${MARGS} $@)

distclean:: clean
	@set -e; for x in ${SUBDIRS}; do \
		(cd $$x; pwd; ${MAKE} ${MARGS} $@) || exit 1; \
	done

clean::
	@set -e; cd ${SYSTYPE}; pwd; ${MAKE} ${MARGS} clean

clean:: FRC
	rm -f *~ *.BAK *.CKP *.orig .systype

install::

links:: FRC
	@set -e; \
	  ln -s SRC/systype SRC/settings .; \
	  mkdir ${SYSTYPE}; cd ${SYSTYPE}; pwd; ln -s ../SRC/${SYSTYPE} SRC; \
	  cp SRC/Makefile Makefile; chmod +w Makefile; \
	  ${MAKE} ${MARGS} links

FRC:
