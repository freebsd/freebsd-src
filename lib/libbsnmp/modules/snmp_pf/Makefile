# $FreeBSD: src/lib/libbsnmp/modules/snmp_pf/Makefile,v 1.1.2.1 2005/04/20 08:45:13 philip Exp $
# 
# Author: Philip Paeps <philip@freebsd.org>

CONTRIB=${.CURDIR}/../../../../contrib/bsnmp

MOD=	pf
SRCS=	pf_snmp.c
WARNS?=	6

XSYM=	begemotPf
DEFS=	${MOD}_tree.def
BMIBS=	BEGEMOT-PF-MIB.txt

.include <bsd.lib.mk>
