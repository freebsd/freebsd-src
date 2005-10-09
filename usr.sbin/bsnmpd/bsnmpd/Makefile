# $FreeBSD$
#
# Author: Harti Brandt <harti@freebsd.org>

CONTRIB=${.CURDIR}/../../../contrib/bsnmp
.PATH: ${CONTRIB}/snmpd

PROG=	bsnmpd
SRCS=	main.c action.c config.c export.c trap.c trans_udp.c trans_lsock.c
SRCS+=	oid.h tree.c tree.h
XSYM=	snmpMIB begemotSnmpdModuleTable begemotSnmpd begemotTrapSinkTable \
	sysUpTime snmpTrapOID coldStart authenticationFailure \
	begemotSnmpdTransUdp begemotSnmpdTransLsock begemotSnmpdLocalPortTable
CLEANFILES= oid.h tree.c tree.h
MAN=	bsnmpd.1 snmpmod.3
WARNS?=	6
NO_WERROR=

FILESGROUPS= BMIBS DEFS

BMIBS=	FOKUS-MIB.txt BEGEMOT-MIB.txt BEGEMOT-SNMPD.txt
BMIBSDIR= ${SHAREDIR}/snmp/mibs
DEFS=	tree.def
DEFSDIR= ${SHAREDIR}/snmp/defs

CFLAGS+= -I${CONTRIB}/lib -I${CONTRIB}/snmpd -I. -DUSE_LIBBEGEMOT
CFLAGS+= -DUSE_TCPWRAPPERS
DPADD=	${LIBBEGEMOT} ${LIBBSNMP}
LDADD=	-lbegemot -lbsnmp -lwrap

LDFLAGS= -export-dynamic

oid.h: tree.def
	gensnmptree -e ${XSYM} < ${.ALLSRC} > ${.TARGET}

.ORDER: tree.c tree.h
tree.c tree.h: tree.def
	gensnmptree -l < ${.ALLSRC}

MANFILTER=	sed -e 's%@MODPATH@%${LIBDIR}/%g'		\
		    -e 's%@DEFPATH@%${DEFSDIR}/%g'		\
		    -e 's%@MIBSPATH@%${BMIBSDIR}/%g'

.include <bsd.prog.mk>
