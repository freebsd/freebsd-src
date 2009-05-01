# $FreeBSD: src/usr.sbin/bsnmpd/modules/snmp_pf/Makefile,v 1.2.8.1 2009/04/15 03:14:26 kensmith Exp $
# 
# Author: Philip Paeps <philip@freebsd.org>

MOD=	pf
SRCS=	pf_snmp.c
WARNS?=	6

XSYM=	begemotPf
DEFS=	${MOD}_tree.def
BMIBS=	BEGEMOT-PF-MIB.txt

.include <bsd.snmpmod.mk>
