# $FreeBSD: src/lib/libbsnmp/modules/snmp_netgraph/Makefile,v 1.6 2004/01/24 20:12:30 harti Exp $
#
# Author: Harti Brandt <harti@freebsd.org>

CONTRIB=${.CURDIR}/../../../../contrib/bsnmp

MOD=	netgraph
SRCS=	snmp_netgraph.c
XSYM=	begemotNg
MAN=	snmp_netgraph.3

BMIBS=	BEGEMOT-NETGRAPH.txt
DEFS=	${MOD}_tree.def
INCS=	snmp_${MOD}.h

DPADD=	${LIBNETGRAPH}
LDADD=	-lnetgraph

.include <bsd.lib.mk>
