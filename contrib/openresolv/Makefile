PKG=		openresolv

# Nasty hack so that make clean works without configure being run
_CONFIG_MK!=	test -e config.mk && echo config.mk || echo config-null.mk
CONFIG_MK?=	${_CONFIG_MK}
include		${CONFIG_MK}

SBINDIR?=	/sbin
SYSCONFDIR?=	/etc
LIBEXECDIR?=	/libexec/resolvconf
VARDIR?=	/var/run/resolvconf

ECHO?=		echo
INSTALL?=	install
SED?=		sed

VERSION!=	${SED} -n 's/OPENRESOLV_VERSION="\(.*\)".*/\1/p' resolvconf.in

BINMODE?=	0755
DOCMODE?=	0644
MANMODE?=	0444

RESOLVCONF=	resolvconf resolvconf.8 resolvconf.conf.5
SUBSCRIBERS=	libc dnsmasq named pdnsd pdns_recursor unbound
TARGET=		${RESOLVCONF} ${SUBSCRIBERS}
SRCS=		${TARGET:C,$,.in,} # pmake
SRCS:=		${TARGET:=.in} # gmake

SED_SBINDIR=		-e 's:@SBINDIR@:${SBINDIR}:g'
SED_SYSCONFDIR=		-e 's:@SYSCONFDIR@:${SYSCONFDIR}:g'
SED_LIBEXECDIR=		-e 's:@LIBEXECDIR@:${LIBEXECDIR}:g'
SED_VARDIR=		-e 's:@VARDIR@:${VARDIR}:g'
SED_RCDIR=		-e 's:@RCDIR@:${RCDIR}:g'
SED_RESTARTCMD=		-e 's:@RESTARTCMD@:${RESTARTCMD}:g'
SED_RCDIR=		-e 's:@RCDIR@:${RCDIR}:g'
SED_STATUSARG=		-e 's:@STATUSARG@:${STATUSARG}:g'

DISTPREFIX?=	${PKG}-${VERSION}
DISTFILEGZ?=	${DISTPREFIX}.tar.gz
DISTFILE?=	${DISTPREFIX}.tar.xz
DISTINFO=	${DISTFILE}.distinfo
DISTINFOSIGN=	${DISTINFO}.asc
CKSUM?=		cksum -a SHA256
PGP?=		netpgp

GITREF?=	HEAD

.SUFFIXES: .in

all: ${TARGET}

.in: Makefile ${CONFIG_MK}
	${SED}	${SED_SBINDIR} ${SED_SYSCONFDIR} ${SED_LIBEXECDIR} \
		${SED_VARDIR} \
		${SED_RCDIR} ${SED_RESTARTCMD} ${SED_RCDIR} ${SED_STATUSARG} \
		$< > $@

clean:
	rm -f ${TARGET}

distclean: clean
	rm -f config.mk ${DISTFILE} ${DISTINFO} ${DISTINFOSIGN}

installdirs:

proginstall: ${TARGET}
	${INSTALL} -d ${DESTDIR}${SBINDIR}
	${INSTALL} -m ${BINMODE} resolvconf ${DESTDIR}${SBINDIR}
	${INSTALL} -d ${DESTDIR}${SYSCONFDIR}
	test -e ${DESTDIR}${SYSCONFDIR}/resolvconf.conf || \
	${INSTALL} -m ${DOCMODE} resolvconf.conf ${DESTDIR}${SYSCONFDIR}
	${INSTALL} -d ${DESTDIR}${LIBEXECDIR}
	${INSTALL} -m ${DOCMODE} ${SUBSCRIBERS} ${DESTDIR}${LIBEXECDIR}

maninstall:
	${INSTALL} -d ${DESTDIR}${MANDIR}/man8
	${INSTALL} -m ${MANMODE} resolvconf.8 ${DESTDIR}${MANDIR}/man8
	${INSTALL} -d ${DESTDIR}${MANDIR}/man5
	${INSTALL} -m ${MANMODE} resolvconf.conf.5 ${DESTDIR}${MANDIR}/man5

install: proginstall maninstall

dist-git:
	git archive --prefix=${DISTPREFIX}/ ${GITREF} | xz >${DISTFILE}

dist-inst:
	mkdir /tmp/${DISTPREFIX}
	cp -RPp * /tmp/${DISTPREFIX}
	(cd /tmp/${DISTPREFIX}; make clean)
	tar -cvjpf ${DISTFILE} -C /tmp ${DISTPREFIX}
	rm -rf /tmp/${DISTPREFIX}

dist: dist-git

distinfo: dist
	rm -f ${DISTINFO} ${DISTINFOSIGN}
	${CKSUM} ${DISTFILE} >${DISTINFO}
	#printf "SIZE (${DISTFILE}) = %s\n" $$(wc -c <${DISTFILE}) >>${DISTINFO}
	${PGP} --clearsign --output=${DISTINFOSIGN} ${DISTINFO}
	chmod 644 ${DISTINFOSIGN}
	ls -l ${DISTFILE} ${DISTINFO} ${DISTINFOSIGN}

import: dist
	rm -rf /tmp/${DISTPREFIX}
	${INSTALL} -d /tmp/${DISTPREFIX}
	tar xvJpf ${DISTFILE} -C /tmp

_import-src:
	rm -rf ${DESTDIR}/*
	${INSTALL} -d ${DESTDIR}
	cp LICENSE README.md ${SRCS} resolvconf.conf ${DESTDIR};
	cp resolvconf.8.in resolvconf.conf.5.in ${DESTDIR};
	@${ECHO}
	@${ECHO} "============================================================="
	@${ECHO} "openresolv-${VERSION} imported to ${DESTDIR}"

import-src:
	${MAKE} _import-src DESTDIR=`if [ -n "${DESTDIR}" ]; then echo "${DESTDIR}"; else  echo /tmp/${DISTPREFIX}; fi`
