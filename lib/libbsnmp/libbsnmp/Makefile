# $FreeBSD: src/lib/libbsnmp/libbsnmp/Makefile,v 1.1 2003/11/10 09:17:33 harti Exp $
#
# Author: Harti Brandt <harti@freebsd.org>

CONTRIB= ${.CURDIR}/../../../contrib/bsnmp/lib
.PATH: ${CONTRIB}

LIB	= bsnmp

WARNS=	6
CFLAGS= -I${CONTRIB}

SRCS=	asn1.c snmp.c snmpagent.c snmpclient.c
INCS=	asn1.h snmp.h snmpagent.h snmpclient.h
MAN=	asn1.3 bsnmplib.3 bsnmpclient.3 bsnmpagent.3

.include <bsd.lib.mk>
