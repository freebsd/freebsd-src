# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/bsnmp/snmpd

.if !defined(NO_ATM)
_snmp_atm= snmp_atm
.endif

SUBDIR=	${_snmp_atm} \
	${_snmp_hostres} \
	snmp_mibII \
	snmp_netgraph \
	snmp_pf

INCS=	snmpmod.h
INCSDIR= ${INCLUDEDIR}/bsnmp

.if ${MACHINE_ARCH} != "amd64"
_snmp_hostres=	snmp_hostres
.endif

.include <bsd.prog.mk>
