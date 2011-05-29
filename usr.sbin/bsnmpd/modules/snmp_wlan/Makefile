#
# $FreeBSD$
#

MOD=	wlan
SRCS=	wlan_snmp.c wlan_sys.c
CFLAGS+= -DSNMPTREE_TYPES

WARNS=            6

XSYM=	begemotWlan

BMIBS=	BEGEMOT-WIRELESS-MIB.txt
MAN=	snmp_${MOD}.3
DEFS=	${MOD}_tree.def

.include <bsd.snmpmod.mk>
