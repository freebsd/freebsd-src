# $FreeBSD: src/lib/libbsnmp/modules/snmp_netgraph/Makefile,v 1.3 2003/11/12 17:09:13 harti Exp $
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

CFLAGS+= -I${CONTRIB}/lib -I${CONTRIB}/snmpd -I${.CURDIR}

.include <bsd.lib.mk>
