#
# Copyright (C) 2012 by Darren Reed.
#
# See the IPFILTER.LICENCE file for details on licencing.
#
#gcc -D__KERNEL__ -I/usr/src/linux/include -Wall -Wstrict-prototypes -O2
#-fomit-frame-pointer -fno-strength-reduce -pipe -m486 -malign-loops=2
#-malign-jumps=2 -malign-functions=2 -DCPU=586  -c -o ip_input.o ip_input.c

SPECFILE=ipfilter.spec
ROOTDIR=/usr/src/redhat/BUILD/ipfilter/
BINDEST=/usr/bin
SBINDEST=/sbin
MANDIR=/usr/share/man
#CC=cc -Wall -Wuninitialized -Wstrict-prototypes -Werror -O
CFLAGS=-g -I$(TOP)
#
# For Linux
#
CPUTYPE:=`uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc64/ -e s/arm.*/arm/ -e s/sa110/arm/`
ARCH:=$(shell uname -i)
OBJ=.
TOP=..
TOOL=$(TOP)/tools
CPU:=$(shell uname -i)
KREV:=$(shell uname -r)
MODEXT:=$(shell modprobe -l | head -n 1 | sed -e 's/.*\.\([^\.]*\)$$/\1/')
MODDIR=/lib/modules/$(KREV)
IPV4DIR=$(MODDIR)/kernel/net/ipv4/
CPUDIR=$(shell uname -s|sed -e 's@/@@g')-$(KREV)-$(ARCH)
HERE=Linux
#LEXLIB=-ll
DEST=$(OBJ)
DEBUG=-g
LINUXKERNEL=/usr/src/linux
LIBS=-L$(OBJ) -lipf $(LIBBPF)
INC=-nostdinc -I$(TOP) -I$(LINUXKERNEL)/include/
DEF=-D$(CPU) -D__$(CPU)__ -DINET -D__KERNEL__ -DCONFIG_NETFILTER -DMODULE
KCFLAGS=-Wall -Wstrict-prototypes -O2 -fomit-frame-pointer  -Wno-trigraphs \
	-fno-strict-aliasing -fno-common -DLINUX=$(LINUX) -I/usr/src/linux/asm/${mcore}
IPDEF=$(DEF) -DGATEWAY -DDIRECTED_BROADCAST
IPFILC=ip_lfil.c
XCFLAGS=-DLINUX=$(LINUX) -I.. -I`pwd`/.. -I`pwd`/../..  $(LOOKUP) $(XID) $(IPFLOG) -O2
EXTRA_CFLAGS += $(XCFLAGS)
#
MFLAGS="BINDEST=$(BINDEST)" "SBINDEST=$(SBINDEST)" "MANDIR=$(MANDIR)" \
	'CFLAGS=$(CFLAGS)' "IPFLKM=$(IPFLKM)" \
	"IPFLOG=$(IPFLOG)" "LOGFAC=$(LOGFAC)" "POLICY=$(POLICY)" \
	"DEBUG=$(DEBUG)" "DCPU=$(CPU)" "CPUDIR=$(CPUDIR)"
#
########## ########## ########## ########## ########## ########## ##########
#
CP=/bin/cp
RM=/bin/rm
CHMOD=/bin/chmod
INSTALL=install
INSTALL=$(TOP)/bsdinstall
LIBSRC=$(TOP)/lib
RANLIB=ranlib
AROPTS=crs
#
MODOBJS=$(OBJ)/linux.o $(OBJ)/ip_fil.o $(OBJ)/fil.o $(OBJ)/ip_nat.o \
	$(OBJ)/ip_frag.o $(OBJ)/ip_scan.o $(OBJ)/ip_sync.o $(OBJ)/ip_nat6.o \
	$(OBJ)/ip_state.o $(OBJ)/ip_proxy.o $(OBJ)/ip_auth.o \
	$(OBJ)/ip_lookup.o $(OBJ)/ip_pool.o $(OBJ)/ip_htable.o \
	$(OBJ)/ip_dstlist.o $(OBJ)/ip_log.o $(OBJ)/radix_ipf.o $(OBJ)/md5.o
KCCARGS=$(KCFLAGS) -D_BSD_SOURCE=1 $(IPFLKM) $(DEF) $(DLKM) -march=$(ARCH) \
	$(INC) -iwithprefix include -I. $(LOOKUP) $(XID) $(IPFLOG)
IPF=$(OBJ)/ipf.o $(OBJ)/ipfcomp.o $(OBJ)/ipf_y.o $(OBJ)/ipf_l.o
IPT=$(OBJ)/ipftest.o $(OBJ)/ip_fil_u.o $(OBJ)/ip_state_u.o \
    $(OBJ)/ip_frag_u.o $(OBJ)/ip_nat_u.o $(OBJ)/fil_u.o $(OBJ)/ip_nat6_u.o \
    $(OBJ)/ip_pool_u.o $(OBJ)/ip_htable_u.o $(OBJ)/ip_lookup_u.o \
    $(OBJ)/ip_proxy_u.o $(OBJ)/ip_auth_u.o $(OBJ)/ip_log_u.o \
    $(OBJ)/ippool_y.o $(OBJ)/ippool_l.o $(OBJ)/ipnat_y.o $(OBJ)/ipnat_l.o \
    $(OBJ)/ipf_y.o $(OBJ)/ipf_l.o $(OBJ)/radix_ipf_u.o $(OBJ)/md5_u.o \
    $(OBJ)/ip_dstlist_u.o $(OBJ)/ip_scan_u.o $(OBJ)/ip_sync_u.o
#	$(OBJ)/ip_trafcon_u.o
IPNAT=$(OBJ)/ipnat.o $(OBJ)/ipnat_y.o $(OBJ)/ipnat_l.o
IPMON=$(OBJ)/ipmon.o $(OBJ)/ipmon_y.o $(OBJ)/ipmon_l.o
IPPOOL=$(OBJ)/ippool_y.o $(OBJ)/ippool_l.o $(OBJ)/kmem.o $(OBJ)/ippool.o
IPTRAFCON=$(OBJ)/iptrafcon.o
FILS=$(OBJ)/ipfstat.o
#
CCARGS=-I. -I$(CPUDIR) $(DEBUG) $(CFLAGS) $(LOOKUP) $(XID) $(IPFLOG) -DLINUX=$(LINUX)
EXTRA=-DIPFILTER_LOG -DIPFILTER_LOOKUP

include $(TOP)/lib/Makefile

$(OBJ):
	@if [ ! -d $(OBJ) ] ; then \
		mkdir $(OBJ) ; \
		if [ $(LINUX) -ge 20600 ] ; then \
			ln -s ../Makefile.kbuild $(OBJ)/Makefile ; \
		fi ; \
		ln -s ../Makefile.ipsend $(OBJ) ; \
	fi

build all: $(OBJ) $(OBJ)/ipf $(OBJ)/ipfs $(OBJ)/ipfstat $(OBJ)/ipftest \
	   $(OBJ)/ipmon $(OBJ)/ipnat $(OBJ)/ippool
	-sh -c 'for i in ipf ipftest ipmon ippool ipnat ipscan ipsyncm ipsyncs; do /bin/rm -f $(TOP)/$$i; ln -s `pwd`/$(OBJ)/$$i $(TOP); done'

ipflkm:
	if [ $(LINUX) -lt 20499 ] ; then \
		make $(OBJ)/ipfilter.o; \
	else \
		(cd $(OBJ); pwd; unset MAKEFLAGS; make -C "$(MODDIR)/build" SUBDIRS="`pwd`" TOP="../.."  CPUDIR="$(CPUDIR)" EXTRA_CFLAGS="$(EXTRA_CFLAGS)" OBJ= modules); \
	fi

$(OBJ)/ipfstat: $(FILS) $(OBJ)/libipf.a $(TOP)/ip_fil.h $(TOP)/ipf.h \
	    $(TOP)/ip_frag.h $(TOP)/ip_compat.h $(TOP)/ip_state.h \
	    $(TOP)/ip_nat.h
	-if [ ! -f /usr/lib/libelf.so ] ; then \
		(cd /usr/lib; a=`echo libelf.so.*|head -n 1`; \
		 if [ "$$a" != "" ] ; then ln -s $$a libelf.so; fi) \
	fi
	$(CC) $(CCARGS) $(FILS) -o $@ -lelf $(LIBS) $(STATETOP_LIB)

$(OBJ)/ipf: $(IPF) $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(IPF) -o $@ $(LIBS) $(LEXLIB) $(LIBBPF)

$(OBJ)/ipftest: $(IPT) $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(IPT) -o $@ $(LIBS) $(LEXLIB) $(LIBBPF)
	/bin/rm -f $(TOP)/ipftest
	ln -s `pwd`/ipftest $(TOP)

$(OBJ)/ipnat: $(IPNAT) $(OBJ)/libipf.a
	-if [ ! -f /usr/lib/libelf.so ] ; then \
		(cd /usr/lib; a=`echo libelf.so.*|head -n 1`; \
		 if [ "$$a" != "" ] ; then ln -s $$a libelf.so; fi) \
	fi
	$(CC) $(CCARGS) $(IPNAT) -o $@ $(LIBS) -lelf $(LEXLIB)

$(OBJ)/ipfs: $(OBJ)/ipfs.o $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(OBJ)/ipfs.o -o $@ $(LIBS)

$(OBJ)/ipsyncm: ipsyncm.o $(OBJ)/libipf.a
	$(CC) $(CCARGS) ipsyncm.o -o $@

$(OBJ)/ipsyncs: ipsyncs.o $(OBJ)/ibipf.a
	$(CC) $(CCARGS) ipsyncs.o -o $@

tests:
	(cd test; make )

$(OBJ)/ipfstat.o: $(TOOL)/ipfstat.c $(TOP)/ip_fil.h $(TOP)/ipf.h
	$(CC) $(CCARGS) $(STATETOP_CFLAGS) -c $(TOOL)/ipfstat.c -o $@

$(OBJ)/ipfs.o: $(TOOL)/ipfs.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_state.h \
		$(TOP)/ip_nat.h
	$(CC) $(CCARGS) -c $(TOOL)/ipfs.c -o $@

$(OBJ)/fil_u.o: $(TOP)/fil.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(IPFBPF) -c $(TOP)/fil.c -o $@

$(OBJ)/fil.o: fil.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_compat.h ipf-linux.h
	$(CC) $(KCCARGS) $(POLICY) $(IPFBPF) -c fil.c -o $@

fil.c: $(TOP)/fil.c Makefile
	sed -e '/^#/,$$d' $< > $@
	echo '#include "ipf-linux.h"' >> $@
	sed -ne '/END OF INCLUDES/,$$p' $< >> $@

$(OBJ)/ipf.o: $(TOOL)/ipf.c $(TOP)/ip_fil.h $(TOP)/ipf.h
	$(CC) $(CCARGS) -c $(TOOL)/ipf.c -o $@

$(OBJ)/ipfcomp.o: $(TOOL)/ipfcomp.c $(TOP)/ip_fil.h $(TOP)/ipf.h
	$(CC) $(CCARGS) -c $(TOOL)/ipfcomp.c -o $@

$(OBJ)/ipf_y.o: $(OBJ)/ipf_y.c $(TOP)/ip_fil.h $(TOP)/ipf.h \
	    $(OBJ)/ipf_y.h $(OBJ)/ipf_l.h
	$(CC) $(CCARGS) $(IPFBPF) -c $(OBJ)/ipf_y.c -o $@

$(OBJ)/ipf_l.o: $(OBJ)/ipf_l.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(OBJ)/ipf_y.h \
	    $(OBJ)/ipf_l.h
	$(CC) $(CCARGS) -I. -I$(TOP)/tools -c $(OBJ)/ipf_l.c -o $@

$(OBJ)/ipf_y.h $(OBJ)/ipf_y.c: $(TOOL)/ipf_y.y $(TOP)/ipf.h $(TOP)/ip_fil.h
	(cd $(TOOL); make "DEST=../$(HERE)/$(OBJ)" ../$(HERE)/$@)
	mv $(OBJ)/ipf_y.c $(OBJ)/ipf_y.c.NP
	sed -e 's/int ipf_yygrowstack();/int ipf_yygrowstack(void);/' \
	    $(OBJ)/ipf_y.c.NP > $(OBJ)/ipf_y.c

$(OBJ)/ipf_l.c: $(TOOL)/lexer.c $(TOP)/ipf.h $(TOP)/ip_fil.h
	(cd $(TOOL); make "DEST=../$(HERE)/$(OBJ)" ../$(HERE)/$@)

$(OBJ)/ipf_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make "DEST=../$(HERE)/$(OBJ)" ../$(HERE)/$@)

$(OBJ)/ippool_y.o: $(OBJ)/ippool_y.c $(TOP)/ip_fil.h $(TOP)/ipf.h \
	    $(OBJ)/ippool_y.h $(OBJ)/ippool_l.h
	$(CC) $(CCARGS) -c $(OBJ)/ippool_y.c -o $@

$(OBJ)/ippool_l.o: $(OBJ)/ippool_l.c $(TOP)/ip_fil.h $(TOP)/ipf.h \
	$(OBJ)/ippool_y.h $(OBJ)/ippool_l.h
	$(CC) $(CCARGS) -I. -I$(TOP)/tools -c $(OBJ)/ippool_l.c -o $@

$(OBJ)/ippool_y.h $(OBJ)/ippool_y.c: $(TOOL)/ippool_y.y $(TOP)/ipf.h \
	$(TOP)/ip_fil.h
	(cd $(TOOL); make "DEST=../$(HERE)/$(OBJ)" ../$(HERE)/$@)
	mv $(OBJ)/ippool_y.c $(OBJ)/ippool_y.c.NP
	sed -e 's/int ippool_yygrowstack();/int ippool_yygrowstack(void);/' \
	    $(OBJ)/ippool_y.c.NP > $(OBJ)/ippool_y.c

$(OBJ)/ippool_l.c: $(TOOL)/lexer.c $(TOP)/ipf.h $(TOP)/ip_fil.h
	(cd $(TOOL); make "DEST=../$(HERE)/$(OBJ)" ../$(HERE)/$@)

$(OBJ)/ippool_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make "DEST=../$(HERE)/$(OBJ)" ../$(HERE)/$@)

$(OBJ)/ippool.o: $(TOOL)/ippool.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_pool.h
	$(CC) $(CCARGS) -c $(TOOL)/ippool.c -o $@

$(OBJ)/ippool: $(IPPOOL) $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(IPPOOL) -o $@ $(LIBS) -lelf $(LEXLIB)

$(OBJ)/ipnat_y.o: $(OBJ)/ipnat_y.c $(TOP)/ip_fil.h $(TOP)/ipf.h \
	    $(OBJ)/ipnat_y.h $(OBJ)/ipnat_l.h
	$(CC) $(CCARGS) -c $(OBJ)/ipnat_y.c -o $@

$(OBJ)/ipnat_l.o: $(OBJ)/ipnat_l.c $(TOP)/ip_fil.h $(TOP)/ipf.h \
	$(OBJ)/ipnat_y.h $(OBJ)/ipnat_l.h
	$(CC) $(CCARGS) -I. -I$(TOP)/tools -c $(OBJ)/ipnat_l.c -o $@

$(OBJ)/ipnat_y.h $(OBJ)/ipnat_y.c: $(TOOL)/ipnat_y.y $(TOP)/ipf.h \
	$(TOP)/ip_fil.h
	(cd $(TOOL); make "DEST=../$(HERE)/$(OBJ)" ../$(HERE)/$@)
	mv $(OBJ)/ipnat_y.c $(OBJ)/ipnat_y.c.NP
	sed -e 's/int ipnat_yygrowstack();/int ipnat_yygrowstack(void);/' \
	    $(OBJ)/ipnat_y.c.NP > $(OBJ)/ipnat_y.c

$(OBJ)/ipnat_l.c: $(TOOL)/lexer.c $(TOP)/ipf.h $(TOP)/ip_fil.h
	(cd $(TOOL); make "DEST=../$(HERE)/$(OBJ)" ../$(HERE)/$@)

$(OBJ)/ipnat_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make "DEST=../$(HERE)/$(OBJ)" ../$(HERE)/$@)

$(OBJ)/ipftest.o: $(TOOL)/ipftest.c $(TOP)/ip_fil.h $(TOP)/ipt.h $(TOP)/ipf.h
	$(CC) $(CCARGS) -c $(TOOL)/ipftest.c -o $@

$(OBJ)/ipnat.o: $(TOOL)/ipnat.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_nat.h
	$(CC) $(CCARGS) -c $(TOOL)/ipnat.c -o $@

$(OBJ)/md5_u.o: $(TOP)/md5.c $(TOP)/md5.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/md5.c -o $@

$(OBJ)/radix_ipf_u.o: $(TOP)/radix_ipf.c $(TOP)/radix_ipf.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/radix_ipf.c -o $@

$(OBJ)/ip_nat_u.o: $(TOP)/ip_nat.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h \
	    $(TOP)/ip_fil.h
	$(CC) $(CCARGS) -c $(TOP)/ip_nat.c -o $@

$(OBJ)/ip_nat6_u.o: $(TOP)/ip_nat6.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h \
	    $(TOP)/ip_fil.h
	$(CC) $(CCARGS) -c $(TOP)/ip_nat6.c -o $@

$(OBJ)/ip_proxy_u.o: $(TOP)/ip_proxy.c $(TOP)/ip_proxy.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h
	$(CC) $(CCARGS) -c $(TOP)/ip_proxy.c -o $@

$(OBJ)/ip_frag_u.o: $(TOP)/ip_frag.c $(TOP)/ip_frag.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h
	$(CC) $(CCARGS) -c $(TOP)/ip_frag.c -o $@

$(OBJ)/ip_scan_u.o: $(TOP)/ip_scan.c $(TOP)/ip_scan.h $(TOP)/ip_compat.h \
	    $(TOP)/ip_fil.h
	$(CC) $(CCARGS) -c $(TOP)/ip_scan.c -o $@

$(OBJ)/ip_sync_u.o: $(TOP)/ip_sync.c $(TOP)/ip_sync.h $(TOP)/ip_compat.h \
	    $(TOP)/ip_fil.h
	$(CC) $(CCARGS) -c $(TOP)/ip_sync.c -o $@

$(OBJ)/ip_state_u.o: $(TOP)/ip_state.c $(TOP)/ip_state.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h $(TOP)/ip_nat.h
	$(CC) $(CCARGS) -c $(TOP)/ip_state.c -o $@

$(OBJ)/ip_auth_u.o: $(TOP)/ip_auth.c $(TOP)/ip_auth.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h
	$(CC) $(CCARGS) -c $(TOP)/ip_auth.c -o $@

$(OBJ)/ip_pool_u.o: $(TOP)/ip_pool.c $(TOP)/ip_pool.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h
	$(CC) $(CCARGS) -c $(TOP)/ip_pool.c -o $@

$(OBJ)/ip_htable_u.o: $(TOP)/ip_htable.c $(TOP)/ip_htable.h \
	$(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(CCARGS) -c $(TOP)/ip_htable.c -o $@

$(OBJ)/ip_dstlist_u.o: $(TOP)/ip_dstlist.c $(TOP)/ip_dstlist.h \
	$(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(CCARGS) -c $(TOP)/ip_dstlist.c -o $@

$(OBJ)/ip_lookup_u.o: $(TOP)/ip_lookup.c $(TOP)/ip_lookup.h \
	$(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(CCARGS) -c $(TOP)/ip_lookup.c -o $@

$(OBJ)/ip_fil_u.o: $(TOP)/ip_fil.c $(TOP)/ip_fil.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) -c $(TOP)/ip_fil.c -o $@

$(OBJ)/ip_log_u.o: $(TOP)/ip_log.c $(TOP)/ip_fil.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) -c $(TOP)/ip_log.c -o $@

$(OBJ)/ipfilter.o: $(MODOBJS)
	ld -r $(MODOBJS) -o $(OBJ)/ipfilter.o
	${RM} -f $(OBJ)/ipfilter

$(OBJ)/ip_nat.o: ip_nat.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h ipf-linux.h
	$(CC) $(KCCARGS) -c ip_nat.c -o $@

$(OBJ)/ip_nat6.o: ip_nat6.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h ipf-linux.h
	$(CC) $(KCCARGS) -c ip_nat6.c -o $@

ip_nat.c: $(TOP)/ip_nat.c Makefile
	sed -e '/^#/,$$d' $< > $@
	echo '#include "ipf-linux.h"' >> $@
	sed -ne '/END OF INCLUDES/,$$p' $< >> $@

$(OBJ)/ip_frag.o: ip_frag.c $(TOP)/ip_frag.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h ipf-linux.h
	$(CC) $(KCCARGS) -c ip_frag.c -o $@

ip_frag.c: $(TOP)/ip_frag.c Makefile
	sed -e '/^#/,$$d' $< > $@
	echo '#include "ipf-linux.h"' >> $@
	sed -ne '/END OF INCLUDES/,$$p' $< >> $@

$(OBJ)/ip_state.o: ip_state.c $(TOP)/ip_state.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h $(TOP)/ip_nat.h ipf-linux.h
	$(CC) $(KCCARGS) -c ip_state.c -o $@

ip_state.c: $(TOP)/ip_state.c Makefile
	sed -e '/^#/,$$d' $< > $@
	echo '#include "ipf-linux.h"' >> $@
	sed -ne '/END OF INCLUDES/,$$p' $< >> $@

$(OBJ)/ip_proxy.o: ip_proxy.c $(TOP)/ip_proxy.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h ipf-linux.h
	$(CC) $(KCCARGS) -c ip_proxy.c -o $@

ip_proxy.c: $(TOP)/ip_proxy.c Makefile
	sed -e '/^#/,$$d' $< > $@
	echo '#include "ipf-linux.h"' >> $@
	sed -ne '/END OF INCLUDES/,$$p' $< >> $@

$(OBJ)/ip_auth.o: ip_auth.c $(TOP)/ip_auth.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h ipf-linux.h
	$(CC) $(KCCARGS) -c ip_auth.c -o $@

ip_auth.c: $(TOP)/ip_auth.c Makefile
	sed -e '/^#/,$$d' $< > $@
	echo '#include "ipf-linux.h"' >> $@
	sed -ne '/END OF INCLUDES/,$$p' $< >> $@

$(OBJ)/ip_lookup.o: ip_lookup.c $(TOP)/ip_lookup.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h ipf-linux.h
	$(CC) $(KCCARGS) -c ip_lookup.c -o $@

ip_lookup.c: $(TOP)/ip_lookup.c Makefile
	sed -e '/^#/,$$d' $< > $@
	echo '#include "ipf-linux.h"' >> $@
	sed -ne '/END OF INCLUDES/,$$p' $< >> $@

$(OBJ)/ip_htable.o: ip_htable.c $(TOP)/ip_htable.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h ipf-linux.h
	$(CC) $(KCCARGS) -c ip_htable.c -o $@

ip_htable.c: $(TOP)/ip_htable.c Makefile
	sed -e '/^#/,$$d' $< > $@
	echo '#include "ipf-linux.h"' >> $@
	sed -ne '/END OF INCLUDES/,$$p' $< >> $@

$(OBJ)/ip_dstlist.o: ip_dstlist.c $(TOP)/ip_dstlist.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h ipf-linux.h
	$(CC) $(KCCARGS) -c ip_dstlist.c -o $@

ip_dstlist.c: $(TOP)/ip_dstlist.c Makefile
	sed -e '/^#/,$$d' $< > $@
	echo '#include "ipf-linux.h"' >> $@
	sed -ne '/END OF INCLUDES/,$$p' $< >> $@

$(OBJ)/ip_pool.o: ip_pool.c $(TOP)/ip_pool.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h ipf-linux.h
	$(CC) $(KCCARGS) -c ip_pool.c -o $@

ip_pool.c: $(TOP)/ip_pool.c Makefile
	sed -e '/^#/,$$d' $< > $@
	echo '#include "ipf-linux.h"' >> $@
	sed -ne '/END OF INCLUDES/,$$p' $< >> $@

$(OBJ)/ip_scan.o: ip_scan.c $(TOP)/ip_scan.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h ipf-linux.h
	$(CC) $(KCCARGS) -c ip_scan.c -o $@

ip_scan.c: $(TOP)/ip_scan.c Makefile
	sed -e '/^#/,$$d' $< > $@
	echo '#include "ipf-linux.h"' >> $@
	sed -ne '/END OF INCLUDES/,$$p' $< >> $@

$(OBJ)/ip_sync.o: ip_sync.c $(TOP)/ip_sync.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h ipf-linux.h
	$(CC) $(KCCARGS) -c ip_sync.c -o $@

ip_sync.c: $(TOP)/ip_sync.c Makefile
	sed -e '/^#/,$$d' $< > $@
	echo '#include "ipf-linux.h"' >> $@
	sed -ne '/END OF INCLUDES/,$$p' $< >> $@

$(OBJ)/ip_log.o: ip_log.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h ipf-linux.h
	$(CC) $(KCCARGS) -c ip_log.c -o $@

ip_log.c: $(TOP)/ip_log.c Makefile
	sed -e '/^#/,$$d' $< > $@
	echo '#include "ipf-linux.h"' >> $@
	sed -ne '/END OF INCLUDES/,$$p' $< >> $@

$(OBJ)/radix_ipf.o: radix_ipf.c $(TOP)/radix_ipf.h ipf-linux.h
	$(CC) $(KCCARGS) -c radix_ipf.c -o $@

$(OBJ)/md5.o: $(TOP)/md5.c $(TOP)/md5.h
	$(CC) $(KCCARGS) -c $(TOP)/md5.c -o $@

radix_ipf.c: $(TOP)/radix_ipf.c Makefile
	sed -e '/^#/,$$d' $< > $@
	echo '#include "ipf-linux.h"' >> $@
	sed -ne '/END OF INCLUDES/,$$p' $< >> $@

ip_rules.c: $(TOP)/rules/ip_rules $(TOP)/tools/ipfcomp.o $(OBJ)/ipf
	$(OBJ)/ipf -cc -nf $(TOP)/rules/ip_rules

$(TOP)/ip_rules.h: ip_rules.c
	if [ ! -f $(TOP)/ip_rules.h ] ; then \
		/bin/mv -f ip_rules.h $(TOP); \
	else \
		touch $(TOP)/ip_rules.h; \
	fi

$(OBJ)/linux.o: $(TOP)/linux.c $(TOP)/radix_ipf.h ipf-linux.h
	$(CC) $(KCCARGS) -DMODVERSIONS -D__GENKSYMS__ -DEXPORT_SYMTAB -c $(TOP)/linux.c -o $@

$(OBJ)/ip_fil.o: $(TOP)/ip_fil_linux.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	ipf-linux.h
	$(CC) $(KCCARGS) -c $(TOP)/ip_fil_linux.c -o $@

$(OBJ)/ipmon: $(IPMON) $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(IPMON) -o $@ $(LIBS) $(LEXLIB) -ldl

$(OBJ)/ipmon.o: $(TOOL)/ipmon.c $(TOP)/ipmon.h
	$(CC) $(CCARGS) $(LOGFAC) -c $(TOOL)/ipmon.c -o $@

$(OBJ)/ipmon_y.o: $(OBJ)/ipmon_y.c $(TOP)/ip_fil.h $(TOP)/ipf.h \
	    $(OBJ)/ipmon_y.h $(OBJ)/ipmon_l.h
	$(CC) $(CCARGS) -c $(OBJ)/ipmon_y.c -o $@

$(OBJ)/ipmon_l.o: $(OBJ)/ipmon_l.c $(TOP)/ip_fil.h $(TOP)/ipf.h \
	$(OBJ)/ipmon_y.h $(OBJ)/ipmon_l.h
	$(CC) $(CCARGS) -I. -I$(TOP)/tools -c $(OBJ)/ipmon_l.c -o $@

$(OBJ)/ipmon_y.h $(OBJ)/ipmon_y.c: $(TOOL)/ipmon_y.y $(TOP)/ipf.h \
	$(TOP)/ip_fil.h
	(cd $(TOOL); make "DEST=../$(HERE)/$(OBJ)" ../$(HERE)/$@)
	mv $(OBJ)/ipmon_y.c $(OBJ)/ipmon_y.c.NP
	sed -e 's/int ipmon_yygrowstack();/int ipmon_yygrowstack(void);/' \
	    $(OBJ)/ipmon_y.c.NP > $(OBJ)/ipmon_y.c

$(OBJ)/ipmon_l.c: $(TOOL)/lexer.c $(TOP)/ipf.h $(TOP)/ip_fil.h
	(cd $(TOOL); make "DEST=../$(HERE)/$(OBJ)" ../$(HERE)/$@)

$(OBJ)/ipmon_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make "DEST=../$(HERE)/$(OBJ)" ../$(HERE)/$@)

clean:
	${RM} -f $(TOP)/ipf $(TOP)/ipnat $(TOP)/ipmon $(TOP)/ippool
	${RM} -f $(TOP)/ipftest $(TOP)/ipscan $(TOP)/ipsyncm $(TOP)/ipsyncs
	${RM} -f *.core *.o ipt fils ipf ipfstat ipftest ipmon ippool ipnat \
		ipfilter.o
	${RM} -f fil.c ip_auth.c ip_frag.c ip_htable.c ip_lookup.c ip_log.c
	${RM} -f ip_lookup.c ip_nat.c ip_pool.c ip_proxy.c ip_scan.c
	${RM} -f ip_state.c ip_sync.c radix_ipf.c ip_rules.c ip_rules.h
	${MAKE} -f Makefile.ipsend ${MFLAGS} clean
	-(for i in Linux*; do \
		if [ -d $${i} ] ; then \
			/bin/rm -rf $${i}; \
		fi \
	done)

install: $(CPUDIR)/$(SPECFILE) $(CPUDIR)/ipfilter
	mkdir -p $(ROOTDIR) $(ROOTDIR)/usr/include/netinet
	mkdir -p $(ROOTDIR)$(SBINDEST) $(ROOTDIR)$(BINDEST)
	mkdir -p $(ROOTDIR)$(IPV4DIR)
	mkdir -p $(ROOTDIR)/etc/init.d
	mkdir -p $(ROOTDIR)/etc/sysconfig
	mkdir -p $(ROOTDIR)$(MANDIR)/man1
	mkdir -p $(ROOTDIR)$(MANDIR)/man4
	mkdir -p $(ROOTDIR)$(MANDIR)/man5
	mkdir -p $(ROOTDIR)$(MANDIR)/man8
	mkdir -p $(ROOTDIR)/usr/share/ipfilter/examples
	touch $(ROOTDIR)/etc/sysconfig/ipfilter
	for i in fil nat compat state sync scan frag auth lookup pool htable proxy; do \
		$(CP) $(TOP)/ip_$$i.h $(ROOTDIR)/usr/include/netinet/ip_$$i.h; \
		$(CHMOD) 444 $(ROOTDIR)/usr/include/netinet/ip_$$i.h; \
	done
	$(CP) $(TOP)/ipl.h $(ROOTDIR)/usr/include/netinet/ipl.h
	-if [ -d $(IPV4DIR) ] ; then \
		if [ -f $(CPUDIR)/ipfilter.$(MODEXT) ] ; then \
			cp $(CPUDIR)/ipfilter.$(MODEXT) $(ROOTDIR)$(IPV4DIR)/ipfilter.$(MODEXT); \
		else \
			cp $(CPUDIR)/ipfilter.o $(ROOTDIR)$(IPV4DIR)/ipfilter.$(MODEXT); \
		fi \
	fi
	(for i in ipfstat ipf ipnat ippool ipfs; do \
		(cd $(CPUDIR); ../$(INSTALL) -c -s -g wheel -m 755 -o root $$i $(ROOTDIR)$(SBINDEST)); \
	 done;)
	(for i in ipmon ipftest; do \
		(cd $(CPUDIR); ../$(INSTALL) -c -s -g wheel -m 755 -o root $$i $(ROOTDIR)$(BINDEST)); \
	 done;)
	(cd $(CPUDIR); ../$(INSTALL) -c -g wheel -m 755 -o root ipfilter $(ROOTDIR)/etc/init.d)
	(cd $(TOP)/man; make INSTALL=../bsdinstall MANDIR=$(ROOTDIR)$(MANDIR) install; cd $(TOP))
	for i in BASIC_1.FW BASIC_2.FW BASIC.NAT example.1 example.2 \
		 example.3 example.4 example.5 example.6 example.7 \
		 example.8 example.9 example.10 example.11 example.12 \
		 example.13 example.sr firewall ftp-proxy ftppxy ipmon.conf \
		 ip_rules nat.eg nat-setup pool.conf server tcpstate; do \
		$(CP) $(TOP)/rules/$$i $(ROOTDIR)/usr/share/ipfilter/examples; \
		$(CHMOD) 444 $(ROOTDIR)/usr/share/ipfilter/examples/$$i; \
		chown root:root $(ROOTDIR)/usr/share/ipfilter/examples/$$i; \
	done
	rpmbuild -bb $(CPUDIR)/$(SPECFILE)
	rpm -i ipfilter

$(CPUDIR)/$(SPECFILE): $(SPECFILE).dist
	sed -e "s/KERNELBUILD/$(KREV)/" \
	    -e "s/MODEXT/$(MODEXT)/" \
	    -e "s/TURNONIPFILTER/`./howchkconfig`/" \
	    $< > $@

$(CPUDIR)/ipfilter:
	./mkinit `pwd`/$(CPUDIR)

