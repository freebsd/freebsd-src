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

SUBDIRS = arpa isc
HFILES = netdb.h resolv.h res_update.h irs.h irp.h hesiod.h

DESTDIR=
DESTINC= /usr/local/bind/include
INSTALL= install
INSTALL_INC=

MARGS= DESTDIR="${DESTDIR}" DESTINC="${DESTINC}" INSTALL="${INSTALL}" \
	MAKE="${MAKE}" INSTALL_INC=${INSTALL_INC}

all depend clean distclean install::
	@for x in ${SUBDIRS}; do \
		(cd $$x; pwd; ${MAKE} ${MARGS} $@) || exit 1; \
	done

distclean:: clean

clean:: FRC
	rm -f *~ *.BAK *.CKP *.orig

install:: ${DESTDIR}${DESTINC}
	@set -x; for x in ${HFILES}; do \
		${INSTALL} -c ${INSTALL_INC} -m 444 $$x ${DESTDIR}${DESTINC}/$$x; \
	done

${DESTDIR}${DESTINC}:
	mkdir -p ${DESTDIR}${DESTINC}; 

links:: FRC
	@set -e; ln -s SRC/*.h .; \
	for x in $(SUBDIRS); do \
		( mkdir $$x; cd $$x; pwd; ln -s ../SRC/$$x SRC; \
		  cp SRC/Makefile Makefile; chmod +w Makefile; \
		  $(MAKE) $(MARGS) links; \
		) || exit 1; \
	done

FRC:
