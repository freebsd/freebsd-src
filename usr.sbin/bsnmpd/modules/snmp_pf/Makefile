# $FreeBSD: src/usr.sbin/bsnmpd/modules/snmp_pf/Makefile,v 1.1.2.1 2006/01/25 13:22:59 harti Exp $
# 
# Author: Philip Paeps <philip@freebsd.org>

MOD=	pf
SRCS=	pf_snmp.c
WARNS?=	6

XSYM=	begemotPf
DEFS=	${MOD}_tree.def
BMIBS=	BEGEMOT-PF-MIB.txt

.include <bsd.snmpmod.mk>
