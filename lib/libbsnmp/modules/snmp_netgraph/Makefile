# $FreeBSD$
#
# Author: Harti Brandt <harti@freebsd.org>

MOD=	netgraph
SRCS=	snmp_netgraph.c
MAN=	snmp_netgraph.3

MIBS=	BEGEMOT-NETGRAPH.txt
DEFS=	${MOD}_tree.def
INCS=	snmp_${MOD}.h

CFLAGS+= -I${INCLUDEDIR}/bsnmp -I${.CURDIR}

.include <bsd.lib.mk>
