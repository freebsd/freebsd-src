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

HFILES = inet.h nameser.h nameser_compat.h

DESTDIR =
DESTINC = /usr/local/bind/include

all depend:

distclean: clean

clean: FRC
	rm -f *~ *.BAK *.CKP *.orig

install: ${DESTDIR}${DESTINC}/arpa
	set -x; for x in ${HFILES}; do \
		${INSTALL} -c ${INSTALL_INC} -m 444 $$x ${DESTDIR}${DESTINC}/arpa/$$x; \
	done

${DESTDIR}${DESTINC}/arpa:
	mkdir -p ${DESTDIR}${DESTINC}/arpa

links: FRC
	@set -e; ln -s SRC/*.h .

FRC:
