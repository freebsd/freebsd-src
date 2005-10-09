# $FreeBSD$
#
# Author: Harti Brandt <harti@freebsd.org>

CONTRIB= ${.CURDIR}/../../../contrib/bsnmp/lib
.PATH: ${CONTRIB}

LIB	= bsnmp
SHLIBDIR ?= /lib

CFLAGS+= -I${CONTRIB} -DHAVE_ERR_H -DHAVE_GETADDRINFO -DHAVE_STRLCPY

SRCS=	asn1.c snmp.c snmpagent.c snmpclient.c support.c
INCS=	asn1.h snmp.h snmpagent.h snmpclient.h
MAN=	asn1.3 bsnmplib.3 bsnmpclient.3 bsnmpagent.3

.include <bsd.lib.mk>
