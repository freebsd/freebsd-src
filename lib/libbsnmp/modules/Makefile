# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/bsnmp/snmpd

.if !defined(NOATM) && defined(SNMP_NOT_YET)
_snmp_atm= snmp_atm
.endif

SUBDIR=	${_snmp_atm} \
	snmp_mibII \
	snmp_netgraph

INCS=	snmpmod.h
INCSDIR= ${INCLUDEDIR}/bsnmp

.include <bsd.prog.mk>
