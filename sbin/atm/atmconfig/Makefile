# Copyright (c) 2001-2003
#	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
#	All rights reserved.
# Author: Harti Brandt <brandt@fokus.gmd.de>
#
# $FreeBSD$

PROG=	atmconfig
.ifndef RESCUE
SRCS=	${.OBJDIR}/oid.h
.endif
SRCS+=	main.c diag.c natm.c
.ifndef RESCUE
SRCS+=	atmconfig_device.c
.endif
MAN=	atmconfig.8
# CFLAGS+= -DPATH_HELP='".:/usr/share/doc/atm:/usr/local/share/doc/atm"'

CFLAGS+= -I${.OBJDIR}

.ifndef RESCUE
DPADD=	${LIBBSNMP}
LDADD=	-lbsnmp
.endif

.ifndef RESCUE
CLEANFILES+= oid.h
.endif

.if ${MACHINE_ARCH} == "arm"
WARNS?= 3
.else
WARNS?=	9
.endif

FILES=	atmconfig.help atmconfig_device.help
FILESDIR= /usr/share/doc/atm

SNMP_ATM_DEF= ${.CURDIR}/../../../contrib/ngatm/snmp_atm/atm_tree.def	\
	${.CURDIR}/../../../lib/libbsnmp/modules/snmp_atm/atm_freebsd.def

${.OBJDIR}/oid.h: atm_oid.list ${SNMP_ATM_DEF}
	cat ${SNMP_ATM_DEF} | gensnmptree -e `tail -n +2 ${.CURDIR}/atm_oid.list` \
		> ${.OBJDIR}/oid.h

.include <bsd.prog.mk>
