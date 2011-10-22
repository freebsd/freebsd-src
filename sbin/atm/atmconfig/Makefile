# Copyright (c) 2001-2003
#	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
#	All rights reserved.
# Author: Harti Brandt <brandt@fokus.gmd.de>
#
# $FreeBSD$

.include <bsd.own.mk>

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
. if ${MK_DYNAMICROOT} == "no" && ${MK_OPENSSL} != "no"
LDADD+= -lcrypto
. endif
.endif

.ifndef RESCUE
CLEANFILES+= oid.h
.endif

# XXX - this is verboten
.if ${MACHINE_CPUARCH} == "arm"
WARNS?= 3
.endif

FILES=	atmconfig.help atmconfig_device.help
FILESDIR= /usr/share/doc/atm

SNMP_ATM_DEF= ${.CURDIR}/../../../contrib/ngatm/snmp_atm/atm_tree.def	\
	${.CURDIR}/../../../usr.sbin/bsnmpd/modules/snmp_atm/atm_freebsd.def

${.OBJDIR}/oid.h: atm_oid.list ${SNMP_ATM_DEF}
	cat ${SNMP_ATM_DEF} | gensnmptree -e `tail -n +2 ${.CURDIR}/atm_oid.list` \
		> ${.OBJDIR}/oid.h

.include <bsd.prog.mk>
