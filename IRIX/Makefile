#!smake
#
# Copyright (C) 2012 by Darren Reed.
# Copyright (C) 1997 by Marc Boucher.
#
# See the IPFILTER.LICENCE file for details on licencing.
#
# For IRIX
#
# Automatically determine compiler switches and defines for kernel compilation
# based on our current CPU:
#
DCPU=`uname -m`
# workaround minor Makefile.kernio IP22 bug
#if defined(DCPU) && ( $(DCPU) == "IP22" )
CMODELWRKAROUND="COMPILATION_MODEL=32"
#else
CMODELWRKAROUND=
#endif
KFLAGS != smake -d v -n \
	-f $(ROOT)/var/sysgen/Makefile.kernio \
		CPUBOARD=$(DCPU) $(CMODELWRKAROUND) 2>/dev/null \
	| sed -n -e '/^Global:CFLAGS =/h' -e '$$g' -e '$$s/Global:CFLAGS =//' \
		-e '$$s/ -G *8 / /' -e '$$p' -e 's/-fullwarn//'
WOFF=-woff 1172,1174,1196,1233,1412,1413,1498,1506,1692
#
SGIREV=-DIRIX=$(SGI)
BINDEST=/usr/sbin
SBINDEST=/sbin
MANDIR=/usr/share/man/local
INCDEST=/usr/include/netinet
LKMDEST=/var/sysgen/boot
HERE=IRIX/$(CPUDIR)
CPUDIR=`uname -s|sed -e 's@/@@g'`-`uname -R`-`uname -m`
RANLIB=echo
AROPTS=crs
OBJ=.
DEST=.
TOP=..
TOOL=$(TOP)/tools
LIBS=-L. -lipf
#CC=cc -Wall -Wuninitialized -Wstrict-prototypes -Werror -O
CC=cc -common
CFLAGS=-g -I$(TOP) -I.
#
ML=mli_ipl.c
MLD=$(ML)
#if defined(IPFLKM) && !empty(IPFLKM)
MLFLAGS= -G 0
LKM=ipflkm.o
#else
MLFLAGS= -G 8
LKM=ipfilter.o
#endif
MFLAGS="BINDEST=$(BINDEST)" "SBINDEST=$(SBINDEST)" "MANDIR=$(MANDIR)" \
	'CFLAGS=$(CFLAGS) $(SOLARIS2)' "IPFLKM=$(IPFLKM)" \
	"IPFLOG=$(IPFLOG)" "LOGFAC=$(LOGFAC)" "POLICY=$(POLICY)" \
	"SOLARIS2=$(SOLARIS2)" "DEBUG=$(DEBUG)" "DCPU=$(CPU)" \
	"CPUDIR=$(CPUDIR)" "LOOKUP=$(LOOKUP)" "XID=$(XID)"
CCARGS=$(DEBUG) $(CFLAGS) -D_KMEMUSER -DIPFILTER_LOG
#
########## ########## ########## ########## ########## ########## ##########
#
CP=/bin/cp
RM=/bin/rm
CHMOD=/bin/chmod
INSTALL=/sbin/install
LIBSRC=$(TOP)/lib
#
DFLAGS=$(WOFF) $(IPFLKM) $(IPFLOG) $(KFLAGS) $(MLFLAGS) -jalr $(SGIREV) \
	$(LOOKUP) $(XID) #-D_MP_NETLOCKS -DMP

include $(TOP)/lib/Makefile

include Makefile.common

#if !defined(IPFLKM) || empty(IPFLKM)
	-${RM} -f $(LKMDEST)/ipflkm.o
	/etc/autoconfig
#endif

