#
# Copyright (C) 2012 by Darren Reed.
#
# See the IPFILTER.LICENCE file for details on licencing.
#
BINDEST=/usr/local/bin
SBINDEST=/sbin
MANDIR=/usr/share/man
CC=/usr/5bin/cc
CFLAGS=-I..
TOP=..
#
# For SunOS 4.1.x
#
DCPU=$(shell uname -m)
DCPU:sh=uname -m
DEF=-D$(DCPU) -D__$(DCPU)__ -DINET -DKERNEL -D_KERNEL -Dsun $(CPU)
IPDEF=$(DEF) -DGATEWAY -DDIRECTED_BROADCAST
ATON=-DNEED_INET_ATON
DEBUG=-g
# For the LKM:
LKM=if_ipl.o
LOGFAC=-DLOGFAC=LOG_LOCAL0
OBJ=.
DEST=.
TOP=..
#
MFLAGS="BINDEST=$(BINDEST)" "SBINDEST=$(SBINDEST)" "MANDIR=$(MANDIR)" \
	'CFLAGS=$(CFLAGS) $(SOLARIS2)' "IPFLKM=$(IPFLKM)" "IPFLOG=$(IPFLOG)" \
	"LOGFAC=$(LOGFAC)" "POLICY=$(POLICY)" "SOLARIS2=$(SOLARIS2)" \
	"DEBUG=$(DEBUG)" "DCPU=$(CPU)" "CPUDIR=$(CPUDIR)" \
	"LOOKUP=$(LOOKUP)" "XID=$(XID)" "SYNC=$(SYNC)" "ALLOPTS=$(ALLOPTS)"
LIBS=-L. -lipf
#
########## ########## ########## ########## ########## ########## ##########
#
CP=/bin/cp
RM=/bin/rm
CHMOD=/bin/chmod
INSTALL=install
#
MODOBJS=ip_fil.o fil.o mls_ipl.o ip_nat.o ip_state.o ip_frag.o ip_proxy.o \
	ip_auth.o ip_log.o ip_pool.o ip_htable.o ip_lookup.o ip_scan.o \
	ip_sync.o ip_rules.o \
	radix_ipf.o md5.o
#	ip_trafcon.o
DFLAGS=$(IPFLKM) $(IPFLOG) $(DEF) $(LOOKUP) $(XID) $(SYNC)
IPF=ipf.o ipfcomp.o ipf_y.o ipf_l.o
IPFTEST=ipftest.o fil_u.o ip_nat_u.o ip_frag_u.o ip_state_u.o ip_fil_u.o \
	ip_proxy_u.o ip_auth_u.o ip_pool_u.o ip_htable_u.o ip_lookup_u.o \
	ip_scan_u.o ip_sync_u.o ip_rules_u.o ip_log_u.o \
	ippool_y.o ippool_l.o ipf_y.o ipf_l.o ipnat_y.o ipnat_l.o \
	md5_u.o radix_ipf_u.o
#	ip_trafcon_u.o
IPNAT=ipnat.o ipnat_y.o ipnat_l.o
IPMON=ipmon.o ipmon_y.o ipmon_l.o
IPPOOL=ippool_y.o ippool_l.o kmem.o ippool.o
IPTRAFCON=iptrafcon.o
IPFSTAT=ipfstat.o
LIBSRC=$(TOP)/lib
TOOL=$(TOP)/tools
RANLIB=ranlib
AROPTS=cq
HERE=SunOS4
CCARGS=-I. $(DEBUG) $(CFLAGS)
EXTRA=-DIPFILTER_LOG -DIPFILTER_LOOKUP

include $(TOP)/lib/Makefile

build all: libipf.a ipf ipfstat ipftest ipmon ipnat ipfs ippool ipscan \
	ipsyncm ipsyncs if_ipl.o
	-sh -c 'for i in ipf ipftest ipmon ippool ipnat ipscan ipsyncm ipsyncs; do /bin/rm -f $(TOP)/$$i; ln -s `pwd`/$$i $(TOP); done'

ipfstat: $(IPFSTAT) libipf.a
	$(CC) $(CCARGS) $(STATETOP_CFLAGS) $(STATETOP_INC) $(IPFSTAT) \
	-o $@ $(LIBS) $(STATETOP_LIB) -lkvm

ipf: $(IPF) libipf.a
	$(CC) $(CCARGS) $(IPF) -o $@ $(LIBS) -ll -lkvm $(LIBBPF)

ipfs: ipfs.o
	$(CC) $(CCARGS) ipfs.o -o $@

ipsyncm: ipsyncm.o libipf.a
	$(CC) $(CCARGS) ipsyncm.o -o $@ $(LIBS)

ipsyncs: ipsyncs.o libipf.a
	$(CC) $(CCARGS) ipsyncs.o -o $@ $(LIBS)

ipsyncm.o: $(TOOL)/ipsyncm.c $(TOP)/ip_sync.h
	$(CC) -DIPFILTER_SYNC $(CCARGS) -c $(TOOL)/ipsyncm.c -o $@

ipsyncs.o: $(TOOL)/ipsyncs.c $(TOP)/ip_sync.h
	$(CC) -DIPFILTER_SYNC $(CCARGS) -c $(TOOL)/ipsyncs.c -o $@

ipftest: $(IPFTEST) libipf.a
	$(CC) $(CCARGS) $(IPFTEST) -o $@ $(LIBS) -ll -L. $(LIBBPF)

ipnat: $(IPNAT) libipf.a
	$(CC) $(CCARGS) $(IPNAT) -o $@ $(LIBS) -lkvm -ll

tests:
	(cd test; make )

ipfs.o: $(TOOL)/ipfs.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_state.h $(TOP)/ip_nat.h
	$(CC) $(CCARGS) -c $(TOOL)/ipfs.c -o $@

ipfstat.o: $(TOOL)/ipfstat.c $(TOP)/ip_fil.h $(TOP)/ipf.h
	$(CC) $(CCARGS) $(STATETOP_CFLAGS) $(STATETOP_INC) \
	-c $(TOOL)/ipfstat.c -o $@

fil_u.o: $(TOP)/fil.c $(TOP)/ip_fil.h $(TOP)/ipf.h
	$(CC) $(CCARGS) $(EXTRA) $(IPFBPF) -c $(TOP)/fil.c -o $@

fil.o: $(TOP)/fil.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ipl.h
	$(CC) $(CCARGS) $(POLICY) $(DFLAGS) $(IPFBPF) -c $(TOP)/fil.c -o $@

ipf.o: $(TOOL)/ipf.c $(TOP)/ip_fil.h $(TOP)/ipf.h
	$(CC) $(CCARGS) -c $(TOOL)/ipf.c -o $@

md5.o: $(TOP)/md5.c $(TOP)/md5.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/md5.c -o $@

radix_ipf.o: $(TOP)/radix_ipf.c $(TOP)/radix_ipf.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/radix_ipf.c -o $@

bpf_filter_u.o: $(TOP)/bpf_filter.c
	$(CC) $(CCARGS) -c $(TOP)/bpf_filter.c -o $@

md5_u.o: $(TOP)/md5.c $(TOP)/md5.h
	$(CC) $(CCARGS) -c $(TOP)/md5.c -o $@

radix_ipf_u.o: $(TOP)/radix_ipf.c $(TOP)/radix_ipf.h
	$(CC) $(CCARGS) -c $(TOP)/radix_ipf.c -o $@

ipfcomp.o: $(TOOL)/ipfcomp.c $(TOP)/ip_fil.h $(TOP)/ipf.h
	$(CC) $(CCARGS) -c $(TOOL)/ipfcomp.c -o $@

ipftest.o: $(TOOL)/ipftest.c $(TOP)/ip_fil.h $(TOP)/ipt.h $(TOP)/ipf.h \
	$(TOP)/ip_lookup.h $(TOP)/ip_pool.h $(TOP)/ip_htable.h
	$(CC) $(CCARGS) -c $(TOOL)/ipftest.c -o $@

ipnat.o: $(TOOL)/ipnat.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_nat.h
	$(CC) $(CCARGS) -c $(TOOL)/ipnat.c -o $@

if_ipl.o: $(MODOBJS)
	ld -r $(MODOBJS) -o $(LKM)
	${RM} -f if_ipl

ip_nat.o: $(TOP)/ip_nat.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/ip_nat.c -o $@

ip_frag.o: $(TOP)/ip_frag.c $(TOP)/ip_frag.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/ip_frag.c -o $@

ip_state.o: $(TOP)/ip_state.c $(TOP)/ip_state.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/ip_state.c -o $@

ip_scan.o: $(TOP)/ip_scan.c $(TOP)/ip_scan.h $(TOP)/ip_compat.h
	$(CC) -DIPFILTER_SCAN $(CCARGS) $(DFLAGS) -c $(TOP)/ip_scan.c -o $@

ip_sync.o: $(TOP)/ip_sync.c $(TOP)/ip_sync.h $(TOP)/ip_compat.h
	$(CC) -DIPFILTER_SYNC $(CCARGS) $(DFLAGS) -c $(TOP)/ip_sync.c -o $@

ip_pool.o: $(TOP)/ip_pool.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h $(TOP)/ip_pool.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/ip_pool.c -o $@

ip_htable.o: $(TOP)/ip_htable.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_htable.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/ip_htable.c -o $@

ip_lookup.o: $(TOP)/ip_pool.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_pool.h $(TOP)/ip_htable.h $(TOP)/ip_lookup.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/ip_lookup.c -o $@

ip_trafcon.o: $(TOP)/ip_trafcon.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_trafcon.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/ip_trafcon.c -o $@

ip_rules.c: ipf $(TOP)/tools/ipfcomp.c $(TOP)/rules/ip_rules
	./ipf -cc -nf $(TOP)/rules/ip_rules

$(TOP)/ip_rules.h: ip_rules.c
	if [ ! -f $(TOP)/ip_rules.h ] ; then \
		/bin/mv -f ip_rules.h $(TOP); \
	else \
		touch $(TOP)/ip_rules.h; \
	fi

ip_rules.o: ip_rules.c $(TOP)/ip_rules.h
	$(CC) -I. $(CFLAGS) $(DFLAGS) -c ip_rules.c -o $@

ip_proxy.o: $(TOP)/ip_proxy.c $(TOP)/ip_proxy.h $(TOP)/ip_compat.h \
	$(TOP)/ip_ftp_pxy.c $(TOP)/ip_rcmd_pxy.c $(TOP)/ip_raudio_pxy.c \
	$(TOP)/ip_rpcb_pxy.c $(TOP)/ip_nat.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/ip_proxy.c -o $@

ip_auth.o: $(TOP)/ip_auth.c $(TOP)/ip_auth.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/ip_auth.c -o $@

ip_nat_u.o: $(TOP)/ip_nat.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_nat.c -o $@

ip_frag_u.o: $(TOP)/ip_frag.c $(TOP)/ip_frag.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_frag.c -o $@

ip_rules_u.o: ip_rules.c $(TOP)/ip_rules.h $(TOP)/ip_compat.h
	$(CC) -DIPFILTER_SCAN $(CCARGS) $(EXTRA) -c ip_rules.c -o $@

ip_state_u.o: $(TOP)/ip_state.c $(TOP)/ip_state.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_state.c -o $@

ip_scan_u.o: $(TOP)/ip_scan.c $(TOP)/ip_scan.h $(TOP)/ip_compat.h
	$(CC) -DIPFILTER_SCAN $(CCARGS) $(EXTRA) -c $(TOP)/ip_scan.c -o $@

ip_sync_u.o: $(TOP)/ip_sync.c $(TOP)/ip_sync.h $(TOP)/ip_compat.h
	$(CC) -DIPFILTER_SYNC $(CCARGS) $(EXTRA) -c $(TOP)/ip_sync.c -o $@

ip_pool_u.o: $(TOP)/ip_pool.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_pool.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_pool.c -o $@

ip_htable_u.o: $(TOP)/ip_htable.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_htable.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_htable.c -o $@

ip_lookup_u.o: $(TOP)/ip_lookup.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_pool.h $(TOP)/ip_htable.h $(TOP)/ip_lookup.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_lookup.c -o $@

ip_trafcon_u.o: $(TOP)/ip_trafcon.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_trafcon.h
	$(CC) $(CCARGS) -c $(TOP)/ip_trafcon.c -o $@


ip_proxy_u.o: $(TOP)/ip_proxy.c $(TOP)/ip_proxy.h $(TOP)/ip_compat.h \
	$(TOP)/ip_ftp_pxy.c $(TOP)/ip_rcmd_pxy.c $(TOP)/ip_raudio_pxy.c \
	$(TOP)/ip_rpcb_pxy.c $(TOP)/ip_ipsec_pxy.c $(TOP)/ip_nat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_proxy.c -o $@

ip_auth_u.o: $(TOP)/ip_auth.c $(TOP)/ip_auth.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_auth.c -o $@

ip_log_u.o: $(TOP)/ip_log.c $(TOP)/ip_fil.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_log.c -o $@

ip_fil.o: $(TOP)/ip_fil_sunos4.c $(TOP)/ip_fil.h $(TOP)/ip_compat.h \
		$(TOP)/ip_nat.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/ip_fil_sunos4.c -o $@

ip_log.o: $(TOP)/ip_log.c $(TOP)/ip_fil.h $(TOP)/ip_compat.h $(TOP)/ip_nat.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/ip_log.c -o $@

ip_fil_u.o: $(TOP)/ip_fil.c $(TOP)/ip_fil.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_fil.c -o $@

ip_input.o: $(TOP)/ip_input.c $(TOP)/ip_fil.h
	$(CC) $(CCARGS) $(IPDEF) -c $(TOP)/ip_input.c -o $@

ip_output.o: $(TOP)/ip_output.c $(TOP)/ip_fil.h
	$(CC) $(CCARGS) $(IPDEF) -c $(TOP)/ip_output.c -o $@

mls_ipl.o: $(TOP)/mls_ipl.c $(TOP)/ipl.h
	-/bin/rm -f $(TOP)/vnode_if.c
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/mls_ipl.c -o $@

ipf_y.o: ipf_y.c ipf_y.h $(TOP)/ipf.h ipf_l.h
	$(CC) $(CCARGS) $(IPFBPF) -c ipf_y.c -o $@

ipf_l.o: ipf_l.c ipf_y.h $(TOP)/ipf.h
	$(CC) $(CCARGS) -I. -I$(TOP)/tools -c ipf_l.c -o $@

ipf_y.h ipf_y.c: $(TOOL)/ipf_y.y $(TOP)/ipf.h
	(cd $(TOOL); make "DEST=$(TOP)/$(HERE)" $(TOP)/$(HERE)/$@)

ipf_l.c: $(TOOL)/lexer.c $(TOP)/ipf.h
	(cd $(TOOL); make "DEST=$(TOP)/$(HERE)" $(TOP)/$(HERE)/$@)

ipf_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make "DEST=$(TOP)/$(HERE)" $(TOP)/$(HERE)/$@);

ipmon: $(IPMON) libipf.a
	$(CC) $(CCARGS) $(IPMON) -o $@ $(LIBS) -ll

ipmon.o: $(TOOL)/ipmon.c $(TOP)/ipmon.h $(TOP)/ip_fil.h
	$(CC) $(CCARGS) $(LOGFAC) -c $(TOOL)/ipmon.c -o $@

ipmon_y.o: ipmon_y.c ipmon_y.h $(TOP)/ipmon.h ipmon_l.h
	$(CC) $(CCARGS) -c ipmon_y.c -o $@

ipmon_l.o: ipmon_l.c ipmon_y.h $(TOP)/ipmon.h ipmon_l.h
	$(CC) $(CCARGS) -I. -I$(TOP)/tools -c ipmon_l.c -o $@

ipmon_y.h ipmon_y.c: $(TOOL)/ipmon_y.y $(TOP)/ipmon.h
	(cd $(TOOL); make "DEST=$(TOP)/$(HERE)" $(TOP)/$(HERE)/$@)

ipmon_l.c: $(TOOL)/lexer.c $(TOP)/ipmon.h
	(cd $(TOOL); make "DEST=$(TOP)/$(HERE)" $(TOP)/$(HERE)/$@)

ipmon_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make "DEST=$(TOP)/$(HERE)" $(TOP)/$(HERE)/$@)

ipscan: ipscan_y.o ipscan_l.o
	$(CC) $(DEBUG) ipscan_y.o ipscan_l.o -o $@ -ll $(LIBS) -lkvm

ipscan_y.o: ipscan_y.c ipscan_y.h $(TOP)/ip_scan.h ipscan_l.h
	$(CC) -DIPFILTER_SCAN $(CCARGS) -c ipscan_y.c -o $@

ipscan_l.o: ipscan_l.c ipscan_y.h $(TOP)/ip_scan.h ipscan_l.h
	$(CC) -DIPFILTER_SCAN $(CCARGS) -I. -I$(TOP)/tools -c ipscan_l.c -o $@

ipscan_y.h ipscan_y.c: $(TOOL)/ipscan_y.y $(TOP)/ip_scan.h
	(cd $(TOOL); make "DEST=$(TOP)/$(HERE)" )

ipscan_l.c: $(TOOL)/lexer.c $(TOP)/ip_scan.h
	(cd $(TOOL); make "DEST=$(TOP)/$(HERE)" $(TOP)/$(HERE)/$@)

ipscan_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make "DEST=$(TOP)/$(HERE)" $(TOP)/$(HERE)/$@)

ippool: $(IPPOOL) libipf.a
	$(CC) $(DEBUG) -I. $(CFLAGS) $(IPPOOL) -o $@ -ll -lkvm $(LIBS)

ippool.o: $(TOOL)/ippool.c $(TOP)/ip_pool.h
	$(CC) $(CCARGS) -c $(TOOL)/ippool.c -o $@

ippool_y.o: ippool_y.c ippool_y.h $(TOP)/ip_pool.h ippool_l.h
	$(CC) $(CCARGS) -c ippool_y.c -o $@

ippool_l.o: ippool_l.c ippool_y.h $(TOP)/ip_pool.h
	$(CC) $(CCARGS) -I. -I$(TOP)/tools -c ippool_l.c -o $@

ippool_y.h ippool_y.c: $(TOOL)/ippool_y.y $(TOP)/ip_pool.h
	(cd $(TOOL); make "DEST=$(TOP)/$(HERE)" $(TOP)/$(HERE)/$@)

ippool_l.c: $(TOOL)/lexer.c $(TOP)/ip_pool.h
	(cd $(TOOL); make "DEST=$(TOP)/$(HERE)" $(TOP)/$(HERE)/$@)

ippool_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make "DEST=$(TOP)/$(HERE)" $(TOP)/$(HERE)/$@)

ipnat_y.o: ipnat_y.c ipnat_y.h $(TOP)/ip_nat.h ipnat_l.h
	$(CC) $(CCARGS) -c ipnat_y.c -o $@

ipnat_l.o: ipnat_l.c ipnat_y.h $(TOP)/ip_nat.h ipnat_l.h
	$(CC) $(CCARGS) -I. -I$(TOP)/tools -c ipnat_l.c -o $@

ipnat_y.h ipnat_y.c: $(TOOL)/ipnat_y.y $(TOP)/ip_nat.h
	(cd $(TOOL); make "DEST=$(TOP)/$(HERE)" $(TOP)/$(HERE)/$@)

ipnat_l.c: $(TOOL)/lexer.c $(TOP)/ip_nat.h
	(cd $(TOOL); make "DEST=$(TOP)/$(HERE)" $(TOP)/$(HERE)/$@)

ipnat_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make "DEST=$(TOP)/$(HERE)" $(TOP)/$(HERE)/$@);

iptrafcon.o: $(TOP)/iptrafcon.c
	$(CC) $(CCARGS) -c $(TOP)/iptrafcon.c -o $@

iptrafcon: $(IPTRAFCON) $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(IPTRAFCON) -o $@ $(LIBS)

.y.c:

.l.c:

clean:
	${RM} -f ../ipf ../ipnat ../ipmon ../ippool ../ipftest
	${RM} -f ../ipscan ../ipsyncm ../ipsyncs
	${RM} -f core *.o *.a ipt ipfstat ipf ipfstat ipftest ipmon
	${RM} -f if_ipl ipnat $(LKM)
	${RM} -f ip_fil.c ipf_l.c ipf_y.c ipf_y.h ipf_l.h
	${RM} -f ipscan ipscan_y.c ipscan_y.h ipscan_l.c ipscan_l.h
	${RM} -f ippool ippool_y.c ippool_y.h ippool_l.c ippool_l.h
	${RM} -f ipnat_y.c ipnat_y.h ipnat_l.c ipnat_l.h
	${RM} -f ipmon_y.c ipmon_y.h ipmon_l.c ipmon_l.h
	${RM} -f ipsyncm ipsyncs ipfs ip_rules.c ip_rules.h
	${MAKE} TOP=.. -f Makefile.ipsend clean


install: all $(TOP)/ip_fil.h
	-$(CP) $(TOP)/ip_fil.h /usr/include/netinet/ip_fil.h
	-$(CHMOD) 444 /usr/include/netinet/ip_fil.h
	-$(INSTALL) -cs -g wheel -m 755 -o root ipf $(SBINDEST)
	-$(INSTALL) -cs -g wheel -m 755 -o root ipfs $(SBINDEST)
	-$(INSTALL) -cs -g wheel -m 755 -o root ipnat $(SBINDEST)
	-$(INSTALL) -cs -g wheel -m 755 -o root ipftest $(BINDEST)
	-$(INSTALL) -cs -g wheel -m 755 -o root ipfstat $(SBINDEST)
	-$(INSTALL) -cs -g wheel -m 755 -o root ipmon $(BINDEST)
	-$(INSTALL) -cs -g wheel -m 755 -o root ipscan $(BINDEST)
	(cd $(TOP)/man; make INSTALL=$(INSTALL) MANDIR=$(MANDIR) install; cd ..)
