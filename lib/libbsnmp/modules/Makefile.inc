# $FreeBSD$

SHLIB_NAME=	snmp_${MOD}.so.${SHLIB_MAJOR}
SRCS+=		${MOD}_tree.c
CFLAGS+=	-I${CONTRIB}/lib -I${CONTRIB}/snmpd -I${.CURDIR}

.if defined(DEFS)
FILESGROUPS+=	DEFS
DEFSDIR=	${SHAREDIR}/bsnmp/defs
.endif

.if defined(MIBS)
FILESGROUPS+=	MIBS
MIBSDIR=	${SHAREDIR}/bsnmp/mibs
.endif

.include "../Makefile.inc"
