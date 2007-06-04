#
# Copyright (C) 1993-1998 by Darren Reed.
#
# See the IPFILTER.LICENCE file for details on licencing.
#
TOP=../..
BINDEST=/usr/sbin
SBINDEST=/sbin
MANDIR=/usr/share/man
SEARCHDIRS!=echo $(BINDEST) $(SBINDEST) /bin /usr/bin /sbin /usr/sbin /usr/local/bin /usr/local/sbin | awk '{for(i=1;i<NF;i++){print $$i;}}' - | sort -u

CC=gcc -Wall -Wuninitialized -Wstrict-prototypes -O -Wmissing-prototypes -Wpointer-arith -Wno-sign-compare -Wno-traditional -Werror
#UFLAGS=-fprofile-arcs -ftest-coverage
CFLAGS=-g -I$(TOP)
#
# For NetBSD/FreeBSD
#
DEVFS!=/usr/bin/lsvfs 2>&1 | sed -n 's/.*devfs.*/-DDEVFS/p'
CPU!=uname -m
INC=-I/usr/include -I/sys -I/sys/sys -I/sys/arch
DEF=-D$(CPU) -D__$(CPU)__ -DINET -DKERNEL -D_KERNEL $(INC) $(DEVFS) -fno-builtin
IPDEF=$(DEF) -DGATEWAY -DDIRECTED_BROADCAST
VNODESHDIR=/sys/kern
MLD=$(ML)
ML=mln_ipl.c
LKM=if_ipl.o
LKMR=ipfrule.o
DLKM=
OBJ=.
DEST=$(OBJ)
MFLAGS="BINDEST=$(BINDEST)" "SBINDEST=$(SBINDEST)" "MANDIR=$(MANDIR)" \
	'CFLAGS=$(CFLAGS) $(SOLARIS2)' "IPFLKM=$(IPFLKM)" \
	"IPFLOG=$(IPFLOG)" "LOGFAC=$(LOGFAC)" "POLICY=$(POLICY)" \
	"SOLARIS2=$(SOLARIS2)" "DEBUG=$(DEBUG)" "DCPU=$(CPU)" \
	"CPUDIR=$(CPUDIR)" "LOOKUP=$(LOOKUP)" "SYNC=$(SYNC)"
LIBS=-L. -lipf $(LIBBPF)
#
########## ########## ########## ########## ########## ########## ##########
#
CP=/bin/cp
RM=/bin/rm
CHMOD=/bin/chmod
INSTALL=install
#
MODOBJS=ip_fil.o fil.o ml_ipl.o ip_nat.o ip_frag.o ip_state.o ip_proxy.o \
	ip_auth.o ip_log.o ip_pool.o ip_htable.o ip_lookup.o ip_rules.o \
	ip_scan.o ip_sync.o
#	ip_trafcon.o
DFLAGS=$(IPFLKM) $(IPFLOG) $(LOOKUP) $(SYNC) $(DEF) $(DLKM) $(IPFBPF)
IPF=ipf.o ipfcomp.o ipf_y.o ipf_l.o bpf_filter_u.o
IPT=ipftest.o fil_u.o ip_frag_u.o ip_state_u.o ip_nat_u.o \
    ip_proxy_u.o ip_auth_u.o ip_htable_u.o ip_lookup_u.o ip_pool_u.o \
    ip_scan_u.o ip_sync_u.o ip_rules_u.o ip_fil_u.o ip_log_u.o \
    ippool_y.o ippool_l.o ipf_y.o ipf_l.o ipnat_y.o ipnat_l.o \
    md5_u.o radix_u.o bpf_filter_u.o
#   ip_syn_u.o
#ip_trafcon_u.o
TOOL=$(TOP)/tools
IPNAT=ipnat.o ipnat_y.o ipnat_l.o
IPMON=ipmon.o ipmon_y.o ipmon_l.o
IPPOOL=ippool_y.o ippool_l.o kmem.o ippool.o
IPTRAFCON=iptrafcon.o
PROXYLIST=$(TOP)/ip_ftp_pxy.c $(TOP)/ip_ipsec_pxy.c $(TOP)/ip_irc_pxy.c \
	  $(TOP)/ip_netbios_pxy.c $(TOP)/ip_raudio_pxy.c $(TOP)/ip_rcmd_pxy.c \
	  $(TOP)/ip_rpcb_pxy.c $(TOP)/ip_pptp_pxy.c
FILS=ipfstat.o
LIBSRC=$(TOP)/lib
RANLIB=ranlib
AROPTS=cq
HERE!=pwd
CCARGS=-I. $(DEBUG) $(CFLAGS) $(UFLAGS)
KCARGS=-I. $(DEBUG) $(CFLAGS)
#
# Extra is option kernel things we always want in user space.
#
EXTRA=$(ALLOPTS)

include $(TOP)/lib/Makefile

build all: machine $(OBJ)/libipf.a ipf ipfs ipfstat ipftest ipmon ipnat \
	   ippool ipscan ipsyncm ipsyncs $(LKM) $(LKMR)
	-sh -c 'for i in ipf ipftest ipmon ippool ipnat ipscan ipsyncm ipsyncs; do /bin/rm -f $(TOP)/$$i; ln -s `pwd`/$$i $(TOP); done'
	-/bin/rm -f ../tools ./tools
	-ln -s ../tools .
	-ln -s ../tools ..

bpf.h:
	echo '#define DEV_BPF 1' > bpf.h

$(TOP)/ip_compat.h: bpf.h

machine: Makefile.kmod
	if [ -f Makefile.kmod ] ; then \
		make -f Makefile.kmod depend MKUPDATE=no; \
	fi

Makefile.kmod:
	if [ -f /usr/share/mk/bsd.kmod.mk -a "`uname -s`" = "NetBSD" ] ; then \
		rm -f Makefile.kmod; \
		ln -s /usr/share/mk/bsd.kmod.mk Makefile.kmod; \
	fi

ipfstat: $(FILS) $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(STATETOP_CFLAGS) $(STATETOP_INC) $(FILS) \
	-o $@ $(LIBS) $(STATETOP_LIB) -lkvm

ipf: $(IPF) $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(IPF) -o $@ $(LIBS) -ll $(LIBBPF)

ipftest: $(IPT) $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(IPT) -o $@ $(LIBS) -ll $(LIBBPF)

ipnat: $(IPNAT) $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(IPNAT) -o $@ $(LIBS) -lkvm -ll

ipfs: ipfs.o
	$(CC) $(CCARGS) ipfs.o -o $@

ipsyncm: ipsyncm.o $(OBJ)/libipf.a
	$(CC) $(CCARGS) ipsyncm.o -o $@ $(LIBS)

ipsyncs: ipsyncs.o $(OBJ)/libipf.a
	$(CC) $(CCARGS) ipsyncs.o -o $@ $(LIBS)

ipsyncm.o: $(TOOL)/ipsyncm.c $(TOP)/ip_sync.h
	$(CC) $(CCARGS) -c $(TOOL)/ipsyncm.c -o $@

ipsyncs.o: $(TOOL)/ipsyncs.c $(TOP)/ip_sync.h
	$(CC) $(CCARGS) -c $(TOOL)/ipsyncs.c -o $@

tests:
	(cd test; make )

ipfstat.o: $(TOOL)/ipfstat.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_frag.h \
	$(TOP)/ip_compat.h $(TOP)/ip_state.h $(TOP)/ip_nat.h $(TOP)/opts.h
	$(CC) $(CCARGS) $(STATETOP_CFLAGS) $(STATETOP_INC) \
	-c $(TOOL)/ipfstat.c -o $@

ipfs.o: $(TOOL)/ipfs.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_state.h \
	$(TOP)/ip_nat.h $(TOP)/opts.h
	$(CC) $(CCARGS) -c $(TOOL)/ipfs.c -o $@

fil_u.o: $(TOP)/fil.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_compat.h \
	    $(TOP)/opts.h $(TOP)/ip_rules.h
	$(CC) $(CCARGS) $(EXTRA) $(IPFBPF) -c $(TOP)/fil.c -o $@

fil.o: $(TOP)/fil.c $(TOP)/ip_fil.h $(TOP)/ip_compat.h $(TOP)/ipl.h \
	    $(TOP)/ip_rules.h
	$(CC) $(KCARGS) $(POLICY) $(DFLAGS) $(IPFBPF) $(COMPIPF) \
	    -c $(TOP)/fil.c -o $@

ipf.o: $(TOOL)/ipf.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/opts.h
	$(CC) $(CCARGS) -c $(TOOL)/ipf.c -o $@

ipfcomp.o: $(TOOL)/ipfcomp.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/opts.h
	$(CC) $(CCARGS) -c $(TOOL)/ipfcomp.c -o $@

ipftest.o: $(TOOL)/ipftest.c $(TOP)/ip_fil.h $(TOP)/ipt.h $(TOP)/ipf.h \
	    $(TOP)/opts.h
	$(CC) $(CCARGS) -c $(TOOL)/ipftest.c -o $@

ipnat.o: $(TOOL)/ipnat.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_nat.h \
	    $(TOP)/opts.h
	$(CC) $(CCARGS) -c $(TOOL)/ipnat.c -o $@

ipnat_y.o: ipnat_y.c ipnat_y.h ipnat_l.h
	$(CC) $(CCARGS) -c ipnat_y.c -o $@

ipnat_l.o: ipnat_l.c ipnat_y.h
	$(CC) $(CCARGS) -I. -c ipnat_l.c -o $@

ipnat_y.c: $(TOOL)/ipnat_y.y
	(cd $(TOOL); make "DEST=$(HERE)" $(HERE)/$@)

ipnat_y.h: ipnat_y.c

ipnat_l.c: $(TOOL)/lexer.c $(TOP)/ip_nat.h
	(cd $(TOOL); make "DEST=$(HERE)" $(HERE)/$@)

ipnat_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make "DEST=$(HERE)" $(HERE)/$@)

ip_nat_u.o: $(TOP)/ip_nat.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_nat.c -o $@

ip_proxy_u.o: $(TOP)/ip_proxy.c $(TOP)/ip_proxy.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h $(PROXYLIST) $(TOP)/ip_nat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_proxy.c -o $@

ip_frag_u.o: $(TOP)/ip_frag.c $(TOP)/ip_frag.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_frag.c -o $@

ip_state_u.o: $(TOP)/ip_state.c $(TOP)/ip_state.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h $(TOP)/ip_nat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_state.c -o $@

ip_auth_u.o: $(TOP)/ip_auth.c $(TOP)/ip_auth.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_auth.c -o $@

ip_fil_u.o: $(TOP)/ip_fil.c $(TOP)/ip_fil.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_fil.c -o $@

ip_rules_u.o: ip_rules.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_rules.h
	$(CC) $(CCARGS) $(EXTRA) -c ip_rules.c -o $@

ip_scan_u.o: $(TOP)/ip_scan.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_scan.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_scan.c -o $@

ip_sync_u.o: $(TOP)/ip_sync.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_sync.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_sync.c -o $@

ip_pool_u.o: $(TOP)/ip_pool.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_pool.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_pool.c -o $@

ip_htable_u.o: $(TOP)/ip_htable.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_htable.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_htable.c -o $@

ip_lookup_u.o: $(TOP)/ip_lookup.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_lookup.h $(TOP)/ip_pool.h $(TOP)/ip_htable.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_lookup.c -o $@

ip_trafcon_u.o: $(TOP)/ip_trafcon.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_trafcon.h
	$(CC) $(CCARGS) -c $(TOP)/ip_trafcon.c -o $@

ip_log_u.o: $(TOP)/ip_log.c $(TOP)/ip_fil.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_log.c -o $@

md5_u.o: $(TOP)/md5.c $(TOP)/md5.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/md5.c -o $@

radix_u.o: $(TOP)/md5.c $(TOP)/radix_ipf.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/radix.c -o $@

bpf_filter_u.o: $(TOP)/bpf_filter.c $(TOP)/pcap-ipf.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/bpf_filter.c -o $@

if_ipl.o: $(MODOBJS) 
	ld -r $(MODOBJS) -o $(LKM)
	${RM} -f if_ipl

ipfrule.ko.5: ip_rulesx.o $(MLR)
	ld -warn-common -r -d -o $(.TARGET:S/.ko/.kld/) ip_rulesx.o $(MLR)
	ld -Bshareable -d -warn-common -o $(LKMR:S/.5$//) $(.TARGET:S/.ko/.kld/) 
ipfrule.ko: ip_rulesx.o $(MLR)
	gensetdefs ip_rulesx.o $(MLR)
	$(CC) $(KCARGS) -c setdef0.c
	$(CC) $(KCARGS) -c setdef1.c
	ld -Bshareable -o $@ setdef0.o ip_rulesx.o $(MLR) setdef1.o

ipf.ko.5 ipl.ko.5: $(MODOBJS)
	ld -warn-common -r -d -o $(.TARGET:S/.ko/.kld/) $(MODOBJS)
	ld -Bshareable -d -warn-common -o $(LKM:S/.5$//) $(.TARGET:S/.ko/.kld/)

ipf.ko ipl.ko: $(MODOBJS) 
	gensetdefs $(MODOBJS)
	$(CC) $(KCARGS) -c setdef0.c
	$(CC) $(KCARGS) -c setdef1.c
	ld -Bshareable -o $@ setdef0.o $(MODOBJS) setdef1.o

ip_nat.o: $(TOP)/ip_nat.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(KCARGS) $(DFLAGS) -c $(TOP)/ip_nat.c -o $@

ip_frag.o: $(TOP)/ip_frag.c $(TOP)/ip_frag.h $(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(KCARGS) $(DFLAGS) -c $(TOP)/ip_frag.c -o $@

ip_state.o: $(TOP)/ip_state.c $(TOP)/ip_state.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h $(TOP)/ip_nat.h
	$(CC) $(KCARGS) $(DFLAGS) -c $(TOP)/ip_state.c -o $@

ip_proxy.o: $(TOP)/ip_proxy.c $(TOP)/ip_proxy.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h $(PROXYLIST) $(TOP)/ip_nat.h
	$(CC) $(KCARGS) $(DFLAGS) -c $(TOP)/ip_proxy.c -o $@

ip_auth.o: $(TOP)/ip_auth.c $(TOP)/ip_auth.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h
	$(CC) $(KCARGS) $(DFLAGS) -c $(TOP)/ip_auth.c -o $@

ip_fil.c:
	/bin/rm -f ip_fil.c
	ln -s $(TOP)/ip_fil_`uname -s|tr A-Z a-z`.c ip_fil.c

ip_fil.o: ip_fil.c $(TOP)/ip_fil.h $(TOP)/ip_compat.h $(TOP)/ip_nat.h
	$(CC) $(KCARGS) $(DFLAGS) $(COMPIPF) -c ip_fil.c -o $@

ip_log.o: $(TOP)/ip_log.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(KCARGS) $(DFLAGS) -c $(TOP)/ip_log.c -o $@

ip_scan.o: $(TOP)/ip_scan.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h $(TOP)/ip_scan.h
	$(CC) $(KCARGS) $(DFLAGS) -c $(TOP)/ip_scan.c -o $@

ip_sync.o: $(TOP)/ip_sync.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h $(TOP)/ip_sync.h
	$(CC) $(KCARGS) $(DFLAGS) -c $(TOP)/ip_sync.c -o $@

ip_pool.o: $(TOP)/ip_pool.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_lookup.h $(TOP)/ip_pool.h
	$(CC) $(KCARGS) $(DFLAGS) -c $(TOP)/ip_pool.c -o $@

ip_htable.o: $(TOP)/ip_htable.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_lookup.h $(TOP)/ip_htable.h
	$(CC) $(KCARGS) $(DFLAGS) -c $(TOP)/ip_htable.c -o $@

ip_lookup.o: $(TOP)/ip_lookup.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_pool.h $(TOP)/ip_htable.h $(TOP)/ip_lookup.h
	$(CC) $(KCARGS) $(DFLAGS) -c $(TOP)/ip_lookup.c -o $@

ip_trafcon.o: $(TOP)/ip_trafcon.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	      $(TOP)/ip_trafcon.h
	$(CC) $(KCARGS) $(DFLAGS) -c $(TOP)/ip_trafcon.c -o $@

vnode_if.h: $(VNODESHDIR)/vnode_if.src
	mkdir -p ../sys
	if [ -f $(VNODESHDIR)/vnode_if.sh ] ; then \
		sh $(VNODESHDIR)/vnode_if.sh $(VNODESHDIR)/vnode_if.src; \
	fi
	if [ -f $(VNODESHDIR)/vnode_if.pl ] ; then \
		perl $(VNODESHDIR)/vnode_if.pl $(VNODESHDIR)/vnode_if.src; \
	fi
	if [ -f ../sys/vnode_if.h ] ; then mv ../sys/vnode_if.h .; fi
	rmdir ../sys

ml_ipl.o: vnode_if.h $(TOP)/$(MLD) $(TOP)/ipl.h
	-/bin/rm -f vnode_if.c
	$(CC) -I. $(CFLAGS) $(DFLAGS) -c $(TOP)/$(ML) -o $@

ip_rules.o: ip_rules.c $(TOP)/ip_rules.h
	$(CC) -I. $(CFLAGS) $(DFLAGS) $(COMPIPF) -c ip_rules.c -o $@

ip_rules.c: $(TOP)/rules/ip_rules $(TOP)/tools/ipfcomp.c ipf
	./ipf -cc -nf $(TOP)/rules/ip_rules

$(TOP)/ip_rules.h: ip_rules.c
	if [ ! -f $(TOP)/ip_rules.h ] ; then \
		/bin/mv -f ip_rules.h $(TOP); \
	else \
		touch $(TOP)/ip_rules.h; \
	fi

ip_rulesx.o: ip_rules.c $(TOP)/ip_rules.h
	$(CC) -I. $(CFLAGS) $(DFLAGS) -DIPFILTER_COMPILED -c ip_rules.c -o $@

mlf_rule.o: $(TOP)/mlf_rule.c $(TOP)/ip_rules.h
	$(CC) -I. $(CFLAGS) $(DFLAGS) -c $(TOP)/mlf_rule.c -o $@

mln_rule.o: $(TOP)/mln_rule.c $(TOP)/ip_rules.h
	$(CC) -I. $(CFLAGS) $(DFLAGS) -c $(TOP)/mln_rule.c -o $@

mlo_rule.o: $(TOP)/mlo_rule.c $(TOP)/ip_rules.h
	$(CC) -I. $(CFLAGS) $(DFLAGS) -c $(TOP)/mlo_rule.c -o $@

mlfk_rule.o: $(TOP)/mlfk_rule.c $(TOP)/ip_rules.h
	$(CC) -I. $(CFLAGS) $(DFLAGS) -c $(TOP)/mlfk_rule.c -o $@

ipf_y.o: ipf_y.c ipf_y.h $(TOP)/ipf.h ipf_l.h $(TOP)/opts.h
	$(CC) $(CCARGS) $(IPFBPF) -c ipf_y.c -o $@

ipf_l.o: ipf_l.c ipf_y.h $(TOP)/ipf.h ipf_l.h $(TOP)/opts.h
	$(CC) $(CCARGS) -I. -c ipf_l.c -o $@

ipf_y.c: $(TOOL)/ipf_y.y $(TOP)/ipf.h $(TOP)/opts.h
	(cd $(TOOL); make "DEST=$(HERE)" $(HERE)/$@)

ipf_y.h: ipf_y.c

ipf_l.c: $(TOOL)/lexer.c $(TOP)/ipf.h $(TOP)/opts.h
	(cd $(TOOL); make "DEST=$(HERE)" $(HERE)/$@)

ipf_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make "DEST=$(HERE)" $(HERE)/$@)

ipmon: $(IPMON) $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(IPMON) -o $@ $(LIBS) -ll

ipmon.o: $(TOOL)/ipmon.c $(TOP)/ipmon.h
	$(CC) $(CCARGS) $(LOGFAC) -c $(TOOL)/ipmon.c -o $@

ipmon_y.o: ipmon_y.c ipmon_y.h $(TOP)/ipmon.h ipmon_l.h
	$(CC) $(CCARGS) -c ipmon_y.c -o $@

ipmon_l.o: ipmon_l.c ipmon_y.h $(TOP)/ipmon.h
	$(CC) $(CCARGS) -I. -c ipmon_l.c -o $@

ipmon_y.c: $(TOOL)/ipmon_y.y $(TOP)/ipmon.h
	(cd $(TOOL); make "DEST=$(HERE)" $(HERE)/$@)

ipmon_y.h: ipmon_y.c

ipmon_l.c: $(TOOL)/lexer.c $(TOP)/ipmon.h
	(cd $(TOOL); make "DEST=$(HERE)" $(HERE)/$@)

ipmon_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make "DEST=$(HERE)" $(HERE)/$@)

ipscan: ipscan_y.o ipscan_l.o
	$(CC) $(DEBUG) ipscan_y.o ipscan_l.o -o $@ -ll $(LIBS) -lkvm

ipscan_y.o: ipscan_y.c ipscan_y.h $(TOP)/ip_scan.h ipscan_l.h
	$(CC) $(CCARGS) -c ipscan_y.c -o $@

ipscan_l.o: ipscan_l.c ipscan_y.h $(TOP)/ip_scan.h
	$(CC) $(CCARGS) -I. -c ipscan_l.c -o $@

ipscan_y.c: $(TOOL)/ipscan_y.y $(TOP)/ip_scan.h
	(cd $(TOOL); make "DEST=$(HERE)" $(HERE)/$@)

ipscan_y.h: ipscan_y.c

ipscan_l.c ipscan_l.h: $(TOOL)/lexer.c $(TOP)/ip_scan.h
	(cd $(TOOL); make "DEST=$(HERE)" $(HERE)/$@)

ippool: $(IPPOOL) $(OBJ)/libipf.a
	$(CC) $(DEBUG) -I. $(CFLAGS) $(IPPOOL) -o $@ -ll -lkvm -L. -lipf

ippool.o: $(TOOL)/ippool.c $(TOP)/ip_pool.h
	$(CC) $(CCARGS) -c $(TOOL)/ippool.c -o $@

ippool_y.o: ippool_y.c ippool_y.h $(TOP)/ip_pool.h ippool_l.h
	$(CC) $(CCARGS) -c ippool_y.c -o $@

ippool_l.o: ippool_l.c ippool_y.h $(TOP)/ip_pool.h
	$(CC) $(CCARGS) -I. -c ippool_l.c -o $@

ippool_y.c: $(TOOL)/ippool_y.y $(TOP)/ip_pool.h ippool_l.h
	(cd $(TOOL); make "DEST=$(HERE)" $(HERE)/$@)

ippool_y.h: ippool_y.c

ippool_l.c: $(TOOL)/lexer.c $(TOP)/ip_pool.h
	(cd $(TOOL); make "DEST=$(HERE)" $(HERE)/$@)

ippool_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make "DEST=$(HERE)" $(HERE)/$@)

iptrafcon.o: $(TOP)/iptrafcon.c
	$(CC) $(CCARGS) -c $< -o $@

iptrafcon: $(IPTRAFCON) $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(IPTRAFCON) -o $@ $(LIBS)

.y.c:

.l.c:

clean:
	${RM} -f ../ipf ../ipnat ../ipmon ../ippool ../ipftest 
	${RM} -f ../ipscan ../ipsyncm ../ipsyncs
	${RM} -f *.core *.o *.a ipt ipfstat ipf ipfstat ipftest ipmon
	${RM} -f if_ipl ipnat ipfrule.ko* ipf.kld* ipfrule.kld*
	${RM} -f vnode_if.h $(LKM) ioconf.h *.ko setdef1.c setdef0.c setdefs.h
	${RM} -f ip_fil.c ipf_l.c ipf_y.c ipf_y.h ipf_l.h
	${RM} -f ipscan ipscan_y.c ipscan_y.h ipscan_l.c ipscan_l.h
	${RM} -f ippool ippool_y.c ippool_y.h ippool_l.c ippool_l.h
	${RM} -f ipnat_y.c ipnat_y.h ipnat_l.c ipnat_l.h
	${RM} -f ipmon_y.c ipmon_y.h ipmon_l.c ipmon_l.h
	${RM} -f ipsyncm ipsyncs ipfs ip_rules.c ip_rules.h bpf.h
	${RM} -f *.da *.gcov *.bb *.bbg tools

	${MAKE} -f Makefile.ipsend ${MFLAGS} clean
	if [ -f Makefile.kmod ] ; then \
		${MAKE} -f Makefile.kmod ${MFLAGS} clean; \
	fi
	-(for i in *; do \
		if [ -d $${i} -a -f $${i}/Makefile ] ; then \
			cd $${i}; (make TOP=../.. clean); cd ..; \
			/bin/rm -f $${i}/Makefile $${i}/Makefile.ipsend; \
			/bin/rm -f $${i}/Makefile.kmod; \
			rmdir $${i}; \
		fi \
	done)

install:
	for i in ip_compat.h ip_fil.h ip_nat.h ip_state.h ip_proxy.h \
		ip_frag.h ip_auth.h; do \
		/bin/cp $(TOP)/$$i /usr/include/netinet/; \
		$(CHMOD) 444 /usr/include/netinet/$$i; \
	done
	-if [ -d /lkm -a -f if_ipl.o ] ; then \
		cp if_ipl.o /lkm; \
	fi
	-if [ -d /modules -a -f ipf.ko ] ; then \
		cp ipf.ko /modules; \
	fi
	-if [ -d /modules -a -f ipfrule.ko ] ; then \
		cp ipfrule.ko /modules; \
	fi
	-if [ -d /boot/kernel -a -f ipf.ko ] ; then \
		cp ipf.ko /boot/kernel; \
	fi
	-if [ -d /boot/kernel -a -f ipfrule.ko ] ; then \
		cp ipfrule.ko /boot/kernel; \
	fi
	-if [ -d /usr/lkm -a -f if_ipl.o ] ; then \
		cp if_ipl.o /usr/lkm; \
	fi
	-$(INSTALL) -cs -g wheel -m 755 -o root ipscan $(SBINDEST)
	(cd $(TOP)/man; make INSTALL=$(INSTALL) MANDIR=$(MANDIR) install; cd $(TOP))
	@for i in ipf:$(SBINDEST) ipfs:$(SBINDEST) ipnat:$(SBINDEST) \
	    ippool:$(BINDEST) ipsyncm:$(BINDEST) ipsyncs:$(BINDEST) \
	    ipfstat:$(SBINDEST) ipftest:$(SBINDEST) ipmon:$(BINDEST); do \
		def="`expr $$i : '[^:]*:\(.*\)'`"; \
		p="`expr $$i : '\([^:]*\):.*'`"; \
		dd=; \
		for d in $(SEARCHDIRS); do \
			if [ -f $$d/$$p ] ; then \
				echo "$(INSTALL) -cs -g wheel -m 755 -o root $$p $$d"; \
				$(INSTALL) -cs -g wheel -m 755 -o root $$p $$d; \
				dd=XXX; \
			fi; \
		done; \
		if [ -z "$$dd" ] ; then \
			echo $(INSTALL) -cs -g wheel -m 755 -o root $$p $$def; \
			$(INSTALL) -cs -g wheel -m 755 -o root $$p $$def; \
		fi \
	done
	if [ -d /etc/rc.d ] ; then \
		$(INSTALL) -c -g wheel -m 755 -o root ../ipfadm-rcd $(SBINDEST)/ipfadm; \
	fi
	(cd $(TOP)/man; make INSTALL=$(INSTALL) MANDIR=$(MANDIR) install; cd $(TOP))

coverage:
	ksh -c 'for i in *.da; do j=$${i%%.da}.c; gcov $$j 2>&1 | egrep -v "y.tab.c|Could|Creating|_l\.c|\.h"; done'  | sort -k 1n -k 3n > report
	sort -k 1n -k 3n report | perl -e 'while(<>) { next if (/^0.00/); s/\%//g; @F=split;$$lc+=$$F[2];$$t += ($$F[0]/100)*$$F[2];}  printf "%d of %d = %d%%\n", $$t, $$lc,($$t/$$lc)*100;' >> report

clean-coverage:
	/bin/rm -f *.gcov *.da
