#
# Makefile for building a standalone send-pr.
#

BINDIR= /usr/bin
NOOBJ=noobj
MAN1= send-pr.1
DATADIR=/etc
SUBMITTERS=current-users
RELEASE=`uname -rsm`

all: ${.CURDIR}/send-pr.sh ${.CURDIR}/send-pr-el.in ${.CURDIR}/install-sid.sh
	sed -e 's,@DATADIR@,$(DATADIR),g' \
	    -e 's/@DEFAULT_RELEASE@/$(RELEASE)/g' ${.CURDIR}/send-pr.sh > send-pr
	sed -e 's,@DATADIR@,$(DATADIR),g' \
		 -e 's/@DEFAULT_RELEASE@/$(RELEASE)/g' ${.CURDIR}/send-pr-el.in > send-pr.el
	sed -e 's,@BINDIR@,$(BINDIR),g' ${.CURDIR}/install-sid.sh > install-sid

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		send-pr ${DESTDIR}/usr/bin/send-pr
	install -c -o ${BINOWN} -g ${BINGRP} -m 0644 \
		${.CURDIR}/categories  ${DESTDIR}/etc/gnats/freefall
	sh install-sid ${SUBMITTERS}

clean:
	rm -f send-pr send-pr.el install-sid

.include <bsd.prog.mk>
