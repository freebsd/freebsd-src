# $FreeBSD$
#
# Makefile for building a standalone send-pr.
#

MAN=		send-pr.1
SUBMITTERS=	current-users
RELEASE=	`uname -rsm`
RELEASE_EV!=	uname -rsm
CLEANFILES+=	send-pr send-pr.el

SUBDIR+= doc

LINKS=	${BINDIR}/send-pr ${BINDIR}/sendbug
MLINKS=	send-pr.1 sendbug.1

all: send-pr _SUBDIR

send-pr:	send-pr.sh Makefile
	sed -e 's,@DATADIR@,/etc,g' \
	    -e 's/@DEFAULT_RELEASE@/$(RELEASE)/g' \
	    -e 's/^SUBMITTER=.*/SUBMITTER=$(SUBMITTERS)/' \
	    ${.ALLSRC:N*Makefile} > ${.TARGET}

send-pr.el:	send-pr-el.in Makefile
	sed -e 's,@DATADIR@,/etc,g' \
	    -e 's/@DEFAULT_RELEASE@/$(RELEASE_EV)/g' \
	    -e 's/"unknown"/"$(SUBMITTERS)"/g' \
	    ${.ALLSRC:N*Makefile} > ${.TARGET}

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		send-pr ${DESTDIR}${BINDIR}/send-pr

# called from /usr/src/etc/Makefile 
etc-gnats-freefall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 0644 \
		${.CURDIR}/categories  ${DESTDIR}/etc/gnats/freefall

.include <bsd.prog.mk>
