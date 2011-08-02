# $FreeBSD$

BIND_DIR=	${.CURDIR}/../../../contrib/bind9
SRCDIR=		${BIND_DIR}/doc

.PATH: ${BIND_DIR} ${SRCDIR}/arm ${SRCDIR}/misc

NO_OBJ=

FILESGROUPS=	TOP ARM MISC
TOP=		CHANGES COPYRIGHT FAQ README README.idnkit \
		RELEASE-NOTES-BIND-9.4-ESV.pdf RELEASE-NOTES-BIND-9.4-ESV.txt \
		RELEASE-NOTES-BIND-9.4-ESV.html release-notes.css
TOPDIR=		${DOCDIR}/bind9
ARM=		Bv9ARM.ch01.html Bv9ARM.ch02.html Bv9ARM.ch03.html \
		Bv9ARM.ch04.html Bv9ARM.ch05.html Bv9ARM.ch06.html \
		Bv9ARM.ch07.html Bv9ARM.ch08.html Bv9ARM.ch09.html \
		Bv9ARM.ch10.html Bv9ARM.html man.dig.html \
		man.dnssec-keygen.html man.dnssec-signzone.html man.host.html \
		man.named-checkconf.html man.named-checkzone.html \
		man.named.html man.rndc-confgen.html man.rndc.conf.html \
		man.rndc.html
ARMDIR=		${TOPDIR}/arm
MISC=		dnssec format-options.pl ipv6 migration migration-4to9 \
		options rfc-compliance roadmap sdb sort-options.pl
MISCDIR=	${TOPDIR}/misc

.include <bsd.prog.mk>
