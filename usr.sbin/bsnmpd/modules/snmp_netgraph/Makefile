# $FreeBSD$
#
# Author: Harti Brandt <harti@freebsd.org>

CONTRIB=${.CURDIR}/../../../../contrib/bsnmp

MOD=	netgraph
SRCS=	snmp_netgraph.c
MAN=	snmp_netgraph.3

MIBS=	BEGEMOT-NETGRAPH.txt
DEFS=	${MOD}_tree.def
INCS=	snmp_${MOD}.h

DPADD=	${LIBNETGRAPH}
LDADD=	-lnetgraph

.include <bsd.lib.mk>
