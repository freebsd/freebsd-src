#
# Makefile for building a standalone send-pr.
#

MAN1=		send-pr.1
SUBMITTERS=	current-users
RELEASE!=	uname -rsm
CLEANFILES+=	send-pr send-pr.el

SUBDIR+= doc

all: send-pr _PROGSUBDIR

send-pr:	send-pr.sh Makefile
	sed -e 's,@DATADIR@,/etc,g' \
	    -e 's/@DEFAULT_RELEASE@/$(RELEASE)/g' \
	    -e 's/^SUBMITTER=.*/SUBMITTER=$(SUBMITTERS)/' \
	    ${.ALLSRC:N*Makefile} > ${.TARGET}

send-pr.el:	send-pr-el.in Makefile
	sed -e 's,@DATADIR@,/etc,g' \
	    -e 's/@DEFAULT_RELEASE@/$(RELEASE)/g' \
	    -e 's/"unknown"/"$(SUBMITTERS)"/g' \
	    ${.ALLSRC:N*Makefile} > ${.TARGET}

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		send-pr ${DESTDIR}${BINDIR}/send-pr
	install -c -o ${BINOWN} -g ${BINGRP} -m 0644 \
		${.CURDIR}/categories  ${DESTDIR}/etc/gnats/freefall

.include <bsd.prog.mk>
