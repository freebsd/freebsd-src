# $FreeBSD$
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
