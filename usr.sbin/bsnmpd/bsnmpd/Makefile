# $FreeBSD$
#
# Author: Harti Brandt <harti@freebsd.org>

CONTRIB=${.CURDIR}/../../../contrib/bsnmp
.PATH:	${CONTRIB}/snmpd

PROG=	bsnmpd
SRCS=	oid.h tree.h tree.c main.c action.c config.c export.c trap.c
MAN=	bsnmpd.1 snmpmod.3
WARNS=	6

FILESGROUPS += MIBS DEFS

MIBS=	FOKUS-MIB.txt BEGEMOT-MIB.txt BEGEMOT-SNMPD.txt
MIBSDIR= ${SHAREDIR}/bsnmp/mibs
DEFS=	tree.def
DEFSDIR= ${SHAREDIR}/bsnmp/defs
INCSDIR= ${INCLUDEDIR}/bsnmp
# snmpdmod is installed in lib/libbsnmp/modules
# INCS=	snmpmod.h

CFLAGS+= -I. -I${.CURDIR} -I${.CURDIR}/../lib -I${.OBJDIR} \
	 -I${CONTRIB}/lib -I${CONTRIB}/snmpd
DPADD+=	${LIBISC} ${LIBBSNMP}
LDADD=	-lisc -lbsnmp

LDFLAGS+= -export-dynamic

.include <bsd.prog.mk>
