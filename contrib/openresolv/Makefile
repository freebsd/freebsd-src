include config.mk

NAME=		openresolv
VERSION=	3.4.1
PKG=		${NAME}-${VERSION}

INSTALL?=	install
SED?=		sed

BINMODE?=	0755
DOCMODE?=	0644
MANMODE?=	0444

RESOLVCONF=	resolvconf resolvconf.8 resolvconf.conf.5
SUBSCRIBERS=	libc dnsmasq named pdnsd unbound
TARGET=		${RESOLVCONF} ${SUBSCRIBERS}
SRCS=		${TARGET:C,$,.in,} # pmake
SRCS:=		${TARGET:=.in} # gmake

SED_PREFIX=		-e 's:@PREFIX@:${PREFIX}:g'
SED_SYSCONFDIR=		-e 's:@SYSCONFDIR@:${SYSCONFDIR}:g'
SED_LIBEXECDIR=		-e 's:@LIBEXECDIR@:${LIBEXECDIR}:g'
SED_VARDIR=		-e 's:@VARDIR@:${VARDIR}:g'
SED_RCDIR=		-e 's:@RCDIR@:${RCDIR}:g'
SED_RESTARTCMD=		-e 's:@RESTARTCMD \(.*\)@:${RESTARTCMD}:g'

.SUFFIXES: .in

all: ${TARGET}

.in:
	${SED}	${SED_PREFIX} ${SED_SYSCONFDIR} ${SED_LIBEXECDIR} \
		${SED_VARDIR} ${SED_RCDIR} ${SED_RESTARTCMD} \
		$< > $@

clean:
	rm -f ${TARGET} openresolv-${VERSION}.tar.bz2

distclean: clean
	rm -f config.mk

installdirs:

install: ${TARGET}
	${INSTALL} -d ${DESTDIR}${SBINDIR}
	${INSTALL} -m ${BINMODE} resolvconf ${DESTDIR}${SBINDIR}
	${INSTALL} -d ${DESTDIR}${SYSCONFDIR}
	test -e ${DESTDIR}${SYSCONFDIR}/resolvconf.conf || \
	${INSTALL} -m ${DOCMODE} resolvconf.conf ${DESTDIR}${SYSCONFDIR}
	${INSTALL} -d ${DESTDIR}${LIBEXECDIR}
	${INSTALL} -m ${DOCMODE} ${SUBSCRIBERS} ${DESTDIR}${LIBEXECDIR}
	${INSTALL} -d ${DESTDIR}${MANDIR}/man8
	${INSTALL} -m ${MANMODE} resolvconf.8 ${DESTDIR}${MANDIR}/man8
	${INSTALL} -d ${DESTDIR}${MANDIR}/man5
	${INSTALL} -m ${MANMODE} resolvconf.conf.5 ${DESTDIR}${MANDIR}/man5

import:
	rm -rf /tmp/${PKG}
	${INSTALL} -d /tmp/${PKG}
	cp README ${SRCS} /tmp/${PKG}

dist: import
	cp configure Makefile resolvconf.conf /tmp/${PKG}
	tar cvjpf ${PKG}.tar.bz2 -C /tmp ${PKG} 
	rm -rf /tmp/${PKG} 
	ls -l ${PKG}.tar.bz2
