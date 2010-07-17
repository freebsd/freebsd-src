# $FreeBSD: src/usr.sbin/bsnmpd/modules/snmp_pf/Makefile,v 1.2.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $
# 
# Author: Philip Paeps <philip@freebsd.org>

MOD=	pf
SRCS=	pf_snmp.c
WARNS?=	6

XSYM=	begemotPf
DEFS=	${MOD}_tree.def
BMIBS=	BEGEMOT-PF-MIB.txt

.include <bsd.snmpmod.mk>
