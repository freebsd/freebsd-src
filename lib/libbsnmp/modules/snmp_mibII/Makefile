# $FreeBSD$
#
# Author: Harti Brandt <harti@freebsd.org>

CONTRIB=${.CURDIR}/../../../../contrib/bsnmp
.PATH: ${CONTRIB}/snmp_mibII

MOD=	mibII
SRCS=	mibII.c mibII_ifmib.c mibII_ip.c mibII_interfaces.c		\
	mibII_ipaddr.c mibII_ifstack.c mibII_rcvaddr.c			\
	mibII_nettomedia.c mibII_tcp.c mibII_udp.c mibII_route.c
MAN=	snmp_mibII.3

DEFS=	${MOD}_tree.def
INCS=	snmp_${MOD}.h

CFLAGS+= -I${CONTRIB}/lib -I${CONTRIB}/snmpd -I${.CURDIR}

.include <bsd.lib.mk>
