# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/bsnmp/snmpd

.if !defined(NOATM)
_snmp_atm= snmp_atm
.endif

SUBDIR=	${_snmp_atm} \
	snmp_mibII \
	snmp_netgraph \
	snmp_pf

INCS=	snmpmod.h
INCSDIR= ${INCLUDEDIR}/bsnmp

.include <bsd.prog.mk>
