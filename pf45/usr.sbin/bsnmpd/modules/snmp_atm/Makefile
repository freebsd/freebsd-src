# $FreeBSD$
#
# Author: Harti Brandt <harti@freebsd.org>

CONTRIB= ${.CURDIR}/../../../../contrib/ngatm
.PATH: ${CONTRIB}/snmp_atm

MOD=	atm
SRCS=	snmp_atm.c atm_sys.c
XSYM=	begemotAtm
MAN=	snmp_atm.3

BMIBS=	BEGEMOT-ATM.txt BEGEMOT-ATM-FREEBSD-MIB.txt
DEFS=	${MOD}_tree.def atm_freebsd.def
INCS=	snmp_${MOD}.h

EXTRAMIBDEFS= atm_freebsd.def

CFLAGS+= -I${CONTRIB}/snmp_atm

.include <bsd.snmpmod.mk>
