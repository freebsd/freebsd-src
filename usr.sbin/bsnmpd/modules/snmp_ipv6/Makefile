# $FreeBSD$
#

MOD=	ipv6mib

SRCS+=	ipv6.c
SRCS+=	ipv6_addrPrefixTable.c
SRCS+=	ipv6_addrTable.c
SRCS+=	ipv6_ifStatsTable.c
SRCS+=	ipv6_ifTable.c
SRCS+=	ipv6_netToMediaTable.c
SRCS+=	ipv6_routeTable.c
SRCS+=	ipv6_sys.c

XSYM=	ipv6MIB

MAN=	snmp_${MOD}.3

DEFS=	${MOD}_tree.def
#INCS=	snmp_${MOD}.h
BMIBS=	IPV6-MIB.txt

CFLAGS+=	-DSNMPTREE_TYPES

.include <bsd.snmpmod.mk>
