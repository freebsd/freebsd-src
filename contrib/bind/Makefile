## Copyright (c) 1996 by Internet Software Consortium.
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

# $Id: Makefile,v 8.46 1998/01/28 23:42:06 halley Exp $

TOP=
SUBDIRS= include port lib bin

SH=sh

all clean depend install distclean:: FRC
	@set -e; \
	version=`cat ${TOP}Version`; \
	systype=`${SH} ${TOP}port/systype ${TOP}.systype`; \
	if [ $$systype = "unknown" ]; then \
		echo "There is no BIND port for this system in this kit."; \
	else \
		settings=`${SH} ${TOP}port/settings ${TOP}.settings \
			< ${TOP}port/$$systype/Makefile.set`; \
		PATH=`pwd`/port/$$systype/bin:$$PATH; export PATH; \
		for x in ${SUBDIRS}; do \
			( cd $$x; pwd; \
			  eval "${MAKE} $$settings ${MARGS} SYSTYPE=$$systype \
				VER=$$version \
				$@"; \
			); \
		done \
	fi

clean distclean::
	rm -f .systype .settings

links: FRC
	@set -e; mkdir ${DST}; cd ${DST}; pwd; ln -s ${SRC} SRC; \
	ln -s SRC/Version .; cp SRC/Makefile .; chmod +w Makefile; \
	systype=`${SH} SRC/port/systype`; \
	for x in ${SUBDIRS}; do \
		( mkdir $$x; cd $$x; pwd; ln -s ../SRC/$$x SRC; \
		  cp SRC/Makefile Makefile; chmod +w Makefile; \
		  ${MAKE} ${MARGS} SYSTYPE=$$systype links; \
		); \
	done

stdlinks: FRC
	if [ ! -d /var/obj ]; then \
		mkdir /var/obj; \
	fi
	${MAKE} ${MARGS} SRC=`pwd` DST=/var/obj/bind links

uplinks: FRC
	@set -e; systype=`${SH} ${TOP}port/systype`; pwd=`pwd`; \
	${MAKE} ${MARGS} SRC=$$pwd "DST=`dirname $$pwd`/$$systype" links

FRC:
