# $FreeBSD$
#
# Author: Harti Brandt <harti@freebsd.org>

CONTRIB=${.CURDIR}/../../../../contrib/bsnmp/snmp_mibII
.PATH: ${CONTRIB}

MOD=	mibII
SRCS=	mibII.c mibII_ifmib.c mibII_ip.c mibII_interfaces.c		\
	mibII_ipaddr.c mibII_ifstack.c mibII_rcvaddr.c			\
	mibII_nettomedia.c mibII_tcp.c mibII_udp.c mibII_route.c
MAN=	snmp_mibII.3

DEFS=	${MOD}_tree.def
INCS=	snmp_${MOD}.h

CFLAGS+= -I${INCLUDEDIR}/bsnmp -I${.CURDIR}

.include <bsd.lib.mk>
