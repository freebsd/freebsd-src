## Copyright (c) 1996,1999 by Internet Software Consortium, Inc.
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

# $Id: Makefile,v 8.52 2000/11/21 12:54:37 marka Exp $

# This is just for making distributions.  For the real Makefile, cd src.

all clean depend: FRC
	@echo go to the ./src directory, you cannot make '"'$@'"' here.
	@false

tar: bind-doc.tar.gz bind-src.tar.gz bind-contrib.tar.gz

pgp: bind-doc.tar.gz.asc bind-src.tar.gz.asc bind-contrib.tar.gz.asc

bind-doc.tar.gz: Makefile
	cd doc/bog; make clean file.psf file.lst
	cd doc/man; make clean all
	cd doc/man; make MANROFF="groff -t" OUT_EXT=psf clean all
	tar cf - Makefile doc | gzip > bind-doc.tar.gz
	cd doc/man; make clean
	cd doc/man; make MANROFF="groff -t" OUT_EXT=psf clean

bind-src.tar.gz: Makefile
	cd src; make distclean
	cd src/bin/nslookup; make commands.c
	cd src/bin/named; make ns_parser.c
	tar cf - Makefile src | gzip > bind-src.tar.gz

bind-contrib.tar.gz: Makefile
	tar cf - Makefile contrib | gzip > bind-contrib.tar.gz

bind-doc.tar.gz.asc: bind-doc.tar.gz
	rm -f bind-doc.tar.gz.asc
	pgp -u pgpkey@isc.org -sba bind-doc.tar.gz
	chmod o+r bind-doc.tar.gz.asc

bind-src.tar.gz.asc: bind-src.tar.gz
	rm -f bind-src.tar.gz.asc
	pgp -u pgpkey@isc.org -sba bind-src.tar.gz
	chmod o+r bind-src.tar.gz.asc

bind-contrib.tar.gz.asc: bind-contrib.tar.gz
	rm -f bind-contrib.tar.gz.asc
	pgp -u pgpkey@isc.org -sba bind-contrib.tar.gz
	chmod o+r bind-contrib.tar.gz.asc

noesw: src/Version src/lib/Makefile src/lib/dst/Makefile \
	src/lib/cylink/. src/lib/dnssafe/.
	perl -pi.BAK -e 's/$$/-NOESW/' src/Version
	perl -pi.BAK -e 's/ cylink dnssafe//' src/lib/Makefile
	perl -pi.BAK -e 's:-I../cylink::' src/lib/dst/Makefile
	perl -pi.BAK -e 's:-I../dnssafe::' src/lib/dst/Makefile
	perl -pi.BAK -e 's/-DCYLINK_DSS//' src/lib/dst/Makefile
	perl -pi.BAK -e 's/-DDNSSAFE//' src/lib/dst/Makefile
	rm -rf src/lib/cylink src/lib/dnssafe

default: all

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
			) || exit 1; \
		done \
	fi

clean distclean::
	rm -f .systype .settings

links: FRC
	@set -e; mkdir ${DST}; cd ${DST}; pwd; ln -s ${SRC} SRC; \
	ln -s SRC/Version .; cp SRC/Makefile .; chmod +w Makefile; \
	systype=`${SH} SRC/port/systype`; \
	for x in ${SUBDIRS} ; do \
		( mkdir $$x; cd $$x; pwd; ln -s ../SRC/$$x SRC; \
		  cp SRC/Makefile Makefile; chmod +w Makefile; \
		  ${MAKE} ${MARGS} SYSTYPE=$$systype links; \
		) || exit 1; \
	done

stdlinks: FRC
	if [ ! -d /var/obj ]; then \
		mkdir /var/obj; \
	fi
	${MAKE} ${MARGS} SRC=`pwd` DST=/var/obj/bind links

uplinks: FRC
	@set -e; systype=`${SH} ${TOP}port/systype`; pwd=`pwd`; \
	${MAKE} ${MARGS} SRC=../`basename $$pwd` "DST=../$$systype" links

FRC:
