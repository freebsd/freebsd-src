# $FreeBSD$
#
# Author: Harti Brandt <harti@freebsd.org>

CONTRIB=${.CURDIR}/../../../../contrib/bsnmp
SNMP_ATM=${.CURDIR}/../../../../contrib/ngatm/snmp_atm
.PATH: ${SNMP_ATM}

MOD=	atm
SRCS=	snmp_atm.c atm_sys.c
XSYM=	begemotAtm
MAN=	snmp_atm.3

BMIBS=	BEGEMOT-ATM.txt BEGEMOT-ATM-FREEBSD-MIB.txt
DEFS=	${MOD}_tree.def atm_freebsd.def
INCS=	snmp_${MOD}.h

EXTRAMIBDEFS= atm_freebsd.def

CFLAGS+= -I${SNMP_ATM} -I${CONTRIB}/snmpd -I${CONTRIB}/snmp_mibII

.include <bsd.lib.mk>
