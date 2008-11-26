# $FreeBSD: src/share/doc/bind9/Makefile,v 1.4.12.1 2008/10/02 02:57:24 kensmith Exp $

BIND_DIR=	${.CURDIR}/../../../contrib/bind9
SRCDIR=		${BIND_DIR}/doc

.PATH: ${BIND_DIR} ${SRCDIR}/arm ${SRCDIR}/misc

NO_OBJ=

FILESGROUPS=	TOP ARM MISC
TOP=		CHANGES COPYRIGHT FAQ README
TOPDIR=		${DOCDIR}/bind9
ARM=		Bv9ARM.ch01.html Bv9ARM.ch02.html Bv9ARM.ch03.html \
		Bv9ARM.ch04.html Bv9ARM.ch05.html Bv9ARM.ch06.html \
		Bv9ARM.ch07.html Bv9ARM.ch08.html Bv9ARM.ch09.html \
		Bv9ARM.html
ARMDIR=		${TOPDIR}/arm
MISC=		dnssec format-options.pl ipv6 migration migration-4to9 \
		options rfc-compliance roadmap sdb
MISCDIR=	${TOPDIR}/misc

.include <bsd.prog.mk>
