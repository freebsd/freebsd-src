# $FreeBSD$

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../../contrib/bsnmp/snmpd

.if ${MK_ATM} != "no"
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
