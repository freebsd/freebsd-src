# $FreeBSD: src/lib/libbsnmp/modules/Makefile,v 1.6 2004/08/11 16:47:12 harti Exp $

.PATH: ${.CURDIR}/../../../contrib/bsnmp/snmpd

.if !defined(NOATM)
_snmp_atm= snmp_atm
.endif

SUBDIR=	${_snmp_atm} \
	snmp_mibII \
	snmp_netgraph

INCS=	snmpmod.h
INCSDIR= ${INCLUDEDIR}/bsnmp

.include <bsd.prog.mk>
