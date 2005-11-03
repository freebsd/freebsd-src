# $FreeBSD: src/usr.sbin/bsnmpd/modules/Makefile,v 1.8 2005/03/14 22:16:39 philip Exp $

.PATH: ${.CURDIR}/../../../contrib/bsnmp/snmpd

.if !defined(NO_ATM)
_snmp_atm= snmp_atm
.endif

SUBDIR=	${_snmp_atm} \
	snmp_mibII \
	snmp_netgraph \
	snmp_pf

INCS=	snmpmod.h
INCSDIR= ${INCLUDEDIR}/bsnmp

.include <bsd.prog.mk>
