# $FreeBSD$

.include <src.opts.mk>

.PATH: ${.CURDIR}/../../../contrib/bsnmp/snmpd

.if ${MK_ATM} != "no"
_snmp_atm= snmp_atm
.endif

SUBDIR=	${_snmp_atm} \
	snmp_bridge \
	snmp_hast \
	snmp_hostres \
	snmp_lm75 \
	snmp_mibII \
	snmp_target \
	snmp_usm \
	snmp_vacm \
	snmp_wlan

.if ${MK_PF} != "no"
SUBDIR+=snmp_pf
.endif

.if ${MK_NETGRAPH_SUPPORT} != "no"
SUBDIR+=snmp_netgraph
.endif

INCS=	snmpmod.h
INCSDIR= ${INCLUDEDIR}/bsnmp

.include <bsd.prog.mk>
