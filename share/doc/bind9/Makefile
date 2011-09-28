# $FreeBSD$

BIND_DIR=	${.CURDIR}/../../../contrib/bind9
SRCDIR=		${BIND_DIR}/doc

.PATH: ${BIND_DIR} ${SRCDIR}/arm ${SRCDIR}/misc

NO_OBJ=

FILESGROUPS=	TOP ARM MISC
TOP=		CHANGES COPYRIGHT FAQ HISTORY README \
		RELEASE-NOTES-BIND-9.8.1.pdf RELEASE-NOTES-BIND-9.8.1.txt \
		RELEASE-NOTES-BIND-9.8.1.html release-notes.css
TOPDIR=		${DOCDIR}/bind9
ARM=		Bv9ARM.ch01.html Bv9ARM.ch02.html Bv9ARM.ch03.html \
		Bv9ARM.ch04.html Bv9ARM.ch05.html Bv9ARM.ch06.html \
		Bv9ARM.ch07.html Bv9ARM.ch08.html Bv9ARM.ch09.html \
		Bv9ARM.ch10.html Bv9ARM.html \
		man.arpaname.html man.ddns-confgen.html man.dig.html \
		man.dnssec-dsfromkey.html man.dnssec-keyfromlabel.html \
		man.dnssec-keygen.html man.dnssec-revoke.html \
		man.dnssec-settime.html man.dnssec-signzone.html \
		man.genrandom.html man.host.html man.isc-hmac-fixup.html \
		man.named-checkconf.html man.named-checkzone.html \
		man.named-journalprint.html man.named.html \
		man.nsec3hash.html man.nsupdate.html \
		man.rndc-confgen.html man.rndc.conf.html man.rndc.html
ARMDIR=		${TOPDIR}/arm
MISC=		dnssec format-options.pl ipv6 migration migration-4to9 \
		options rfc-compliance roadmap sdb sort-options.pl
MISCDIR=	${TOPDIR}/misc

.include <bsd.prog.mk>
