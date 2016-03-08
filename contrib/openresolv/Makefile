PKG=		openresolv
VERSION=	3.7.3

# Nasty hack so that make clean works without configure being run
_CONFIG_MK!=	test -e config.mk && echo config.mk || echo config-null.mk
CONFIG_MK?=	${_CONFIG_MK}
include		${CONFIG_MK}

SBINDIR?=	/sbin
SYSCONFDIR?=	/etc
LIBEXECDIR?=	/libexec/resolvconf
VARDIR?=	/var/run/resolvconf
RCDIR?=		/etc/rc.d
RESTARTCMD?=	if ${RCDIR}/\1 status >/dev/null 2>\&1; then \
			${RCDIR}/\1 restart; \
		fi

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

SED_SBINDIR=		-e 's:@SBINDIR@:${SBINDIR}:g'
SED_SYSCONFDIR=		-e 's:@SYSCONFDIR@:${SYSCONFDIR}:g'
SED_LIBEXECDIR=		-e 's:@LIBEXECDIR@:${LIBEXECDIR}:g'
SED_VARDIR=		-e 's:@VARDIR@:${VARDIR}:g'
SED_RCDIR=		-e 's:@RCDIR@:${RCDIR}:g'
SED_RESTARTCMD=		-e 's:@RESTARTCMD \(.*\)@:${RESTARTCMD}:g'

DISTPREFIX?=	${PKG}-${VERSION}
DISTFILEGZ?=	${DISTPREFIX}.tar.gz
DISTFILE?=	${DISTPREFIX}.tar.xz
FOSSILID?=	current

.SUFFIXES: .in

all: ${TARGET}

.in:
	${SED}	${SED_SBINDIR} ${SED_SYSCONFDIR} ${SED_LIBEXECDIR} \
		${SED_VARDIR} ${SED_RCDIR} ${SED_RESTARTCMD} \
		$< > $@

clean:
	rm -f ${TARGET}

distclean: clean
	rm -f config.mk ${DISTFILE}

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

import:
	rm -rf /tmp/${DISTPREFIX}
	${INSTALL} -d /tmp/${DISTPREFIX}
	cp README ${SRCS} /tmp/${DISTPREFIX}

dist:
	fossil tarball --name ${DISTPREFIX} ${FOSSILID} ${DISTFILEGZ}
	gunzip -c ${DISTFILEGZ} | xz >${DISTFILE}
	rm ${DISTFILEGZ}
