# $FreeBSD: src/usr.sbin/bsnmpd/modules/Makefile,v 1.8.2.1 2006/01/27 16:27:34 harti Exp $

.PATH: ${.CURDIR}/../../../contrib/bsnmp/snmpd

.if !defined(NO_ATM)
_snmp_atm= snmp_atm
.endif

SUBDIR=	${_snmp_atm} \
	snmp_hostres \
	snmp_mibII \
	snmp_netgraph \
	snmp_pf

INCS=	snmpmod.h
INCSDIR= ${INCLUDEDIR}/bsnmp

.include <bsd.prog.mk>
