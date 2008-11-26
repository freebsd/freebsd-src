# $FreeBSD: src/usr.sbin/bsnmpd/modules/Makefile,v 1.8.2.2.4.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../contrib/bsnmp/snmpd

.if !defined(NO_ATM)
_snmp_atm= snmp_atm
.endif

SUBDIR=	${_snmp_atm} \
	snmp_bridge \
	snmp_hostres \
	snmp_mibII \
	snmp_netgraph \
	snmp_pf

INCS=	snmpmod.h
INCSDIR= ${INCLUDEDIR}/bsnmp

.include <bsd.prog.mk>
