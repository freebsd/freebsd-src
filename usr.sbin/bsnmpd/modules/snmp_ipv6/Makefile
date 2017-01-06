# $FreeBSD$
#

MOD=	ipv6MIB

SRCS+=	ipv6.c
SRCS+=	ipv6_addrPrefixTable.c
SRCS+=	ipv6_addrTable.c
SRCS+=	ipv6_ifStatsTable.c
SRCS+=	ipv6_ifTable.c
SRCS+=	ipv6_netToMediaTable.c
SRCS+=	ipv6_routeTable.c

XSYM=	${MOD}

BMIBS=	IPV6-MIB.txt
MAN=	snmp_${MOD}.3
DEFS=	${MOD}_tree.def

CFLAGS+=	-DSNMPTREE_TYPES

# XXX (ngie): unused variables
NO_WERROR=

.include <bsd.snmpmod.mk>
