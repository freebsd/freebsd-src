# $FreeBSD: src/usr.sbin/bsnmpd/modules/snmp_netgraph/Makefile,v 1.7 2005/11/10 10:14:56 harti Exp $
#
# Author: Harti Brandt <harti@freebsd.org>

MOD=	netgraph
SRCS=	snmp_netgraph.c
XSYM=	begemotNg
MAN=	snmp_netgraph.3

BMIBS=	BEGEMOT-NETGRAPH.txt
DEFS=	${MOD}_tree.def
INCS=	snmp_${MOD}.h

DPADD=	${LIBNETGRAPH}
LDADD=	-lnetgraph

.include <bsd.snmpmod.mk>
