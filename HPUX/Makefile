#
# Copyright (C) 2012 by Darren Reed.
#
# See the IPFILTER.LICENCE file for details on licencing.
#
SHELL=/bin/sh
DEPOT=IPF.v4.1.depot
DEPOTDIR=/var/spool/sw
DEST=/opt/ipf
SBINDEST=/sbin
MANDIR=${DEST}/man
BINDEST=${DEST}/bin
EXTRADIR=${DEST}/examples
INCDIR=${DEST}/include
ETCDIR=/etc/opt/ipf
RCDIR=/sbin/init.d
#CFLAGS=-I$(TOP)
#
OBJ=.
HERE=HPUX/$(CPUDIR)
DEST=.
TOP=..
TOOL=$(TOP)/tools
TMP=/tmp
#
HPUXREV=`uname -r | sed -e 's/^[A-Z]\.\([0-9]*\)\.\([0-9]\)/\1\2/'`
#BITS=`getconf KERNEL_BITS`
ABI=+DD${BITS}
KFLAGS=-Ae +ES1.Xindirect_calls -U__hp9000s700 \
 -D__STDC_EXT__ -D_XPG4_EXTENDED -D_HPUX_SOURCE -D__hp9000s800 -D_KERNEL \
 -DKERNEL +Xidu +ESlit -Wp,-H300000 +Hx0 +R500 -Wl,-a,archive +ESsfc -q +ESssf
DEBUG=-g
LIBS=-lnsl -lelf -L. -lipf
DEF=-D_KERNEL
ATON=-DNEED_INET_ATON
HPINC=-I/usr/conf/h
#SPU=-Dspustate_t=u_long
HFLAGS=$(KFLAGS) $(SPU) $(HPINC)
CC=cc $(ABI) -DHPUXREV=$(HPUXREV) -I$(TOP)/../pfil -I/usr/conf -I.
#
MFLAGS="BINDEST=$(BINDEST)" "SBINDEST=$(SBINDEST)" "MANDIR=$(MANDIR)" \
	'CFLAGS=$(CFLAGS) $(ARCHINC)' "IPFLKM=$(IPFLKM)" \
	"IPFLOG=$(IPFLOG)" "LOGFAC=$(LOGFAC)" "POLICY=$(POLICY)" \
	"DEBUG=$(DEBUG)" "CPUDIR=$(CPUDIR)" "HPUXREV=$(HPUXREV)" \
	"LOOKUP=$(LOOKUP)" "XID=$(XID)"
#
O2=+O2
#O2=-g
O3=+O3
#O3=-g
#
########## ########## ########## ########## ########## ########## ##########
#
CP=/bin/cp
RM=/bin/rm
CHMOD=/bin/chmod
INSTALL=$(TOP)/bsdinstall
#
DFLAGS=$(IPFLKM) $(IPFLOG) $(DEF) $(IPFBPF) $(LOOKUP) $(XID)
#-DIPFDEBUG
MODOBJS=ip_fil.o fil.o ip_state.o ip_frag.o \
	ip_nat.o ip_proxy.o ip_auth.o ip_log.o \
	ip_pool.o ip_htable.o ip_lookup.o \
	ip_sync.o ip_scan.o ip_rules.o hpux.o $(BPFILTER) md5.o radix_ipf.o
#	ip_trafcon.o
IPF=ipf.o ipfcomp.o ipf_y.o ipf_l.o
IPT=ipftest.o fil_u.o ip_state_u.o ip_frag_u.o \
	ip_nat_u.o ip_fil_u.o ip_proxy_u.o ip_sync_u.o ip_scan_u.o \
	ip_auth_u.o ip_pool_u.o ip_rules_u.o ip_log_u.o \
	ip_htable_u.o ip_lookup_u.o \
	ipf_y.o ipf_l.o ipnat_y.o ipnat_l.o ippool_y.o ippool_l.o \
	md5_u.o radix_ipf_u.o
#	ip_trafcon_u.o
IPMON=ipmon.o ipmon_y.o ipmon_l.o
IPPOOL=ippool_y.o ippool_l.o ippool.o
IPNAT=ipnat.o ipnat_y.o ipnat_l.o
IPTRAFCON=iptrafcon.o
FILS=ipfstat.o
LIBSRC=$(TOP)/lib
RANLIB=ranlib
AROPTS=cq
CCARGS=$(DEBUG) $(CFLAGS) -I$(TOP)/../pfil/HP-UX
EXTRA=$(ALLOPTS)
#
include $(TOP)/lib/Makefile

build: $(DEST)/libipf.a ipf ipfstat ipftest ipmon ipnat ippool \
	ipfs ipscan ipsyncm ipsyncs mod.o
	-sh -c 'for i in ipf ipftest ipmon ippool ipnat ipscan ipsyncm ipsyncs; do /bin/rm -f $(TOP)/$$i; ln -s `pwd`/$$i $(TOP); done'

.y.c:

.l.c:

pkg: ipf.pkg

ipfstat: $(FILS) $(DEST)/libipf.a
	$(CC) $(CCARGS) $(FILS) -o $@ $(LIBS)

ipf: $(IPF) $(DEST)/libipf.a
	$(CC) $(CCARGS) $(IPF) -o $@ $(LIBS) $(LLIB) $(LIBBPF)

ipftest: $(IPT) $(DEST)/libipf.a
	$(CC) $(CCARGS) $(IPT) -o $@ $(LIBS) $(LIBBPF)

ipnat: $(IPNAT) $(DEST)/libipf.a
	$(CC) $(CFLAGS) $(IPNAT) -o $@ $(LIBS) $(LLIB)

ipfs: ipfs.o $(DEST)/libipf.a
	$(CC) $(CCARGS) ipfs.o -o $@ $(LIBS)

ipsyncm: ipsyncm.o $(DEST)/libipf.a
	$(CC) $(CCARGS) ipsyncm.o -o $@ $(LIBS)

ipsyncs: ipsyncs.o $(DEST)/libipf.a
	$(CC) $(CCARGS) ipsyncs.o -o $@ $(LIBS)

ipsyncm.o: $(TOOL)/ipsyncm.c $(TOP)/ip_sync.h
	$(CC) $(CCARGS) -c $(TOOL)/ipsyncm.c -o $@

ipsyncs.o: $(TOOL)/ipsyncs.c $(TOP)/ip_sync.h
	$(CC) $(CCARGS) -c $(TOOL)/ipsyncs.c -o $@

ipscan: ipscan_y.o ipscan_l.o $(DEST)/libipf.a
	$(CC) $(DEBUG) ipscan_y.o ipscan_l.o -o $@ $(LLIB) $(LIBS)

ipscan_y.h ipscan_y.c: $(TOOL)/ipscan_y.y $(TOP)/ip_scan.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ipscan_l.c: $(TOOL)/lexer.c $(TOP)/ip_scan.h
	(cd $(TOOL); $(MAKE) "DEST=../$(HERE)" ../$(HERE)/$@)

ipscan_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); $(MAKE) "DEST=../$(HERE)" ../$(HERE)/$@)

ipscan_y.o: ipscan_y.c ipscan_y.h ipscan_l.h $(TOP)/ip_scan.h
	$(CC) $(CCARGS) -c ipscan_y.c -o $@

ipscan_l.o: ipscan_l.c ipscan_y.h ipscan_l.h $(TOP)/ip_scan.h
	$(CC) $(CCARGS) -I. -c ipscan_l.c -o $@

tests:
	(cd test; $(MAKE) )

ipfstat.o: $(TOOL)/ipfstat.c $(TOP)/ip_fil.h $(TOP)/ipf.h
	$(CC) $(CCARGS) $(HPINC) -c $(TOOL)/ipfstat.c -o $@

ipfs.o: $(TOOL)/ipfs.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_state.h \
		$(TOP)/ip_nat.h
	$(CC) $(CCARGS) -c $(TOOL)/ipfs.c -o $@

fil_u.o: $(TOP)/fil.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_rules.h
	$(CC) $(CCARGS) $(EXTRA) $(FIXRADIX) $(IPFBPF) -c $(TOP)/fil.c -o $@

fil.o: $(TOP)/fil.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_compat.h \
	 $(TOP)/ipl.h $(TOP)/ip_rules.h
	$(CC) -I$(TOP) $(POLICY) $(DFLAGS) $(HFLAGS) $(IPFBPF) \
		$(O2) -c $(TOP)/fil.c -o $@

ipf.o: $(TOOL)/ipf.c $(TOP)/ip_fil.h $(TOP)/ipf.h
	$(CC) $(CCARGS) -c $(TOOL)/ipf.c -o $@

ipfcomp.o: $(TOOL)/ipfcomp.c $(TOP)/ip_fil.h $(TOP)/ipf.h
	$(CC) $(CCARGS) -c $(TOOL)/ipfcomp.c -o $@

ipf_y.o: ipf_y.c $(TOP)/ip_fil.h $(TOP)/ipf.h ipf_y.h ipf_l.h
	$(CC) $(CCARGS) $(IPFBPF) -c ipf_y.c -o $@

ipf_l.o: ipf_l.c $(TOP)/ip_fil.h $(TOP)/ipf.h ipf_y.h
	$(CC) $(CCARGS)  -I. -I$(TOP)/tools -c ipf_l.c -o $@

ipf_y.h ipf_y.c: $(TOOL)/ipf_y.y $(TOP)/ipf.h $(TOP)/ip_fil.h
	(cd $(TOOL); $(MAKE) "DEST=../$(HERE)" ../$(HERE)/$@)

ipf_l.c: $(TOOL)/lexer.c $(TOP)/ipf.h $(TOP)/ip_fil.h $(TOOL)/lexer.h
	(cd $(TOOL); $(MAKE) "DEST=../$(HERE)" ../$(HERE)/$@)

ipf_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); $(MAKE) "DEST=../$(HERE)" ../$(HERE)/$@)

ipftest.o: $(TOOL)/ipftest.c $(TOP)/ip_fil.h $(TOP)/ipt.h $(TOP)/ipf.h
	$(CC) $(CCARGS) -c $(TOOL)/ipftest.c -o $@

ipnat.o: $(TOOL)/ipnat.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_nat.h
	$(CC) $(CCARGS) -c $(TOOL)/ipnat.c -o $@

ipnat_y.o: ipnat_y.c ipnat_y.h $(TOP)/ip_nat.h ipnat_l.h
	$(CC) $(CCARGS) -c ipnat_y.c -o $@

ipnat_l.o: ipnat_l.c ipnat_y.h $(TOP)/ip_nat.h
	$(CC) $(CCARGS) -I. -c ipnat_l.c -o $@

ipnat_y.h ipnat_y.c: $(TOOL)/ipnat_y.y $(TOP)/ip_nat.h
	(cd $(TOOL); $(MAKE) "DEST=../$(HERE)" ../$(HERE)/$@)

ipnat_l.c: $(TOOL)/lexer.c $(TOP)/ip_nat.h
	(cd $(TOOL); $(MAKE) "DEST=../$(HERE)" ../$(HERE)/$@)

ipnat_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); $(MAKE) "DEST=../$(HERE)" ../$(HERE)/$@)

mod.o:  $(MODOBJS)
	-if [ "$(BITS)" = "64" ] ; then \
		/bin/rm -f libmilli.a; \
		ln -s /usr/lib/pa20_64/milli.a libmilli.a; \
		ld -r $(MODOBJS) -L. -lmilli -o $@; \
	else \
		/bin/rm -f libmilli.a; \
		ln -s /usr/lib/milli.a libmilli.a; \
		ld -r $(MODOBJS) -L. -lmilli -o $@; \
	fi

ip_rules.c: $(TOP)/rules/ip_rules $(TOP)/tools/ipfcomp.c ipf
	./ipf -cc -nf $(TOP)/rules/ip_rules

$(TOP)/ip_rules.h: ip_rules.c
	if [ ! -f $(TOP)/ip_rules.h ] ; then \
		/bin/mv -f ip_rules.h $(TOP); \
	else \
		touch $(TOP)/ip_rules.h; \
	fi

ip_rules.o: ip_rules.c $(TOP)/ip_rules.h
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) $(O3) -c ip_rules.c -o $@

ip_nat.o: $(TOP)/ip_nat.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) $(O3) -c $(TOP)/ip_nat.c -o $@

ip_state.o: $(TOP)/ip_state.c $(TOP)/ip_state.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) $(O3) -c $(TOP)/ip_state.c -o $@

ip_proxy.o: $(TOP)/ip_proxy.c $(TOP)/ip_proxy.h $(TOP)/ip_compat.h \
	    $(TOP)/ip_h323_pxy.c $(TOP)/ip_irc_pxy.c $(TOP)/ip_ipsec_pxy.c \
	    $(TOP)/ip_ftp_pxy.c $(TOP)/ip_raudio_pxy.c $(TOP)/ip_rpcb_pxy.c \
	    $(TOP)/ip_nat.h $(TOP)/ip_fil.h
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) $(O2) -c $(TOP)/ip_proxy.c -o $@

ip_frag.o: $(TOP)/ip_frag.c $(TOP)/ip_frag.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) $(O3) -c $(TOP)/ip_frag.c -o $@

ip_auth.o: $(TOP)/ip_auth.c $(TOP)/ip_auth.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) $(O2) -c $(TOP)/ip_auth.c -o $@

ip_scan.o: $(TOP)/ip_scan.c $(TOP)/ip_scan.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) $(O2) -c $(TOP)/ip_scan.c -o $@

ip_sync.o: $(TOP)/ip_sync.c $(TOP)/ip_sync.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) $(O2) -c $(TOP)/ip_sync.c -o $@

ip_trafcon.o: $(TOP)/ip_trafcon.c $(TOP)/ip_trafcon.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) -c $(TOP)/ip_trafcon.c -o $@

radix_ipf.o: $(TOP)/radix_ipf.c $(TOP)/radix_ipf.h
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) $(O2) -c $(TOP)/radix_ipf.c -o $@

ip_pool.o: $(TOP)/ip_pool.c $(TOP)/ip_pool.h $(TOP)/ip_compat.h radix_ipf.h
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) $(FIXRADIX) $(O2) \
		-c $(TOP)/ip_pool.c -o $@

ip_htable.o: $(TOP)/ip_htable.c $(TOP)/ip_htable.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) $(O2) -c $(TOP)/ip_htable.c -o $@

ip_lookup.o: $(TOP)/ip_lookup.c $(TOP)/ip_lookup.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) $(FIXRADIX) $(O2) \
		-c $(TOP)/ip_lookup.c -o $@

ip_nat_u.o: $(TOP)/ip_nat.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_nat.c -o $@

ip_frag_u.o: $(TOP)/ip_frag.c $(TOP)/ip_frag.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_frag.c -o $@

ip_state_u.o: $(TOP)/ip_state.c $(TOP)/ip_state.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_state.c -o $@

ip_auth_u.o: $(TOP)/ip_auth.c $(TOP)/ip_auth.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_auth.c -o $@

ip_proxy_u.o: $(TOP)/ip_proxy.c $(TOP)/ip_proxy.h $(TOP)/ip_compat.h \
	    $(TOP)/ip_h323_pxy.c $(TOP)/ip_irc_pxy.c $(TOP)/ip_ipsec_pxy.c \
	    $(TOP)/ip_ftp_pxy.c $(TOP)/ip_raudio_pxy.c $(TOP)/ip_rpcb_pxy.c \
	    $(TOP)/ip_nat.h $(TOP)/ip_fil.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_proxy.c -o $@

ip_log_u.o: $(TOP)/ip_log.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
		$(TOP)/ip_nat.h $(TOP)/ip_frag.h $(TOP)/ip_state.h \
		$(TOP)/ip_auth.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_log.c -o $@

ip_fil_u.o: $(TOP)/ip_fil.c $(TOP)/ip_fil.h
	$(CC) $(CCARGS) $(EXTRA) $(FIXRADIX) -c $(TOP)/ip_fil.c -o $@

ip_scan_u.o: $(TOP)/ip_scan.c $(TOP)/ip_scan.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_scan.c -o $@

ip_sync_u.o: $(TOP)/ip_sync.c $(TOP)/ip_sync.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_sync.c -o $@

ip_rules_u.o: ip_rules.c $(TOP)/ip_rules.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c ip_rules.c -o $@

ip_pool_u.o: $(TOP)/ip_pool.c $(TOP)/ip_pool.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_pool.c -o $@

ip_htable_u.o: $(TOP)/ip_htable.c $(TOP)/ip_htable.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_htable.c -o $@

ip_lookup_u.o: $(TOP)/ip_lookup.c $(TOP)/ip_lookup.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_lookup.c -o $@

ip_trafcon_u.o: $(TOP)/ip_trafcon.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_trafcon.h
	$(CC) $(CCARGS) -c $(TOP)/ip_trafcon.c -o $@

ip_fil.o: $(TOP)/ip_fil_hpux.c $(TOP)/ip_fil.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) $(O2) \
		$(COMPIPF) -c $(TOP)/ip_fil_hpux.c -o $@

ip_log.o: $(TOP)/ip_log.c $(TOP)/ip_fil.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) $(O2) -c $(TOP)/ip_log.c -o $@

hpux.o: $(TOP)/hpux.c $(TOP)/ipl.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) $(O2) -c $(TOP)/hpux.c -o $@

bpf_filter_u.o: $(TOP)/bpf_filter.c $(TOP)/bpf-ipf.h $(TOP)/pcap-ipf.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/bpf_filter.c -o $@

bpf_filter.o: $(TOP)/bpf_filter.c
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) $(O2) -c $(TOP)/bpf_filter.c -o $@

md5_u.o: $(TOP)/md5.c $(TOP)/md5.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/md5.c -o $@

md5.o: $(TOP)/md5.c $(TOP)/md5.h
	$(CC) -I$(TOP) $(DFLAGS) $(HFLAGS) $(O3) $(SPU) -c $(TOP)/md5.c -o $@

radix_ipf_u.o: $(TOP)/radix_ipf.c $(TOP)/radix_ipf.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/radix_ipf.c -o $@

ipmon: $(IPMON) $(DEST)/libipf.a
	$(CC) $(CCARGS) $(LOGFAC) $(IPMON) -o $@ $(LIBS) $(LLIB)

$(TOP)/ipmon.c:

ipmon_y.c ipmon_y.h: $(TOOL)/ipmon_y.y $(TOP)/ipmon.h
	(cd $(TOOL); $(MAKE) "DEST=../$(HERE)" ../$(HERE)/$@)

ipmon_l.c: $(TOOL)/lexer.c $(TOP)/ipmon.h
	(cd $(TOOL); $(MAKE) "DEST=../$(HERE)" ../$(HERE)/$@)

ipmon_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); $(MAKE) "DEST=../$(HERE)" ../$(HERE)/$@)

ipmon.o: $(TOOL)/ipmon.c
	$(CC) $(CCARGS) $(LOGFAC) -c $(TOOL)/ipmon.c -o $@

ipmon_y.o: ipmon_y.c $(TOP)/ipmon.h ipmon_l.h
	$(CC) $(CCARGS) -c ipmon_y.c -o $@

ipmon_l.o: ipmon_l.c $(TOP)/ipmon.h
	$(CC) $(CCARGS) -I. -c ipmon_l.c -o $@

ippool: $(IPPOOL)
	$(CC) $(DEBUG) -I. $(CFLAGS) $(IPPOOL) -o $@ $(LIBS) $(LLIB)

ippool.o: $(TOOL)/ippool.c $(TOP)/ipf.h $(TOP)/ip_pool.h
	$(CC) $(CCARGS) -c $(TOOL)/ippool.c -o $@

ippool_y.o: ippool_y.c ippool_y.h $(TOP)/ip_pool.h ippool_l.h
	$(CC) $(CCARGS) -c ippool_y.c -o $@

ippool_l.o: ippool_l.c ippool_y.h $(TOP)/ip_pool.h
	$(CC) $(CCARGS) -I. -c ippool_l.c -o $@

ippool_y.h ippool_y.c: $(TOOL)/ippool_y.y $(TOP)/ip_pool.h
	(cd $(TOOL); $(MAKE) "DEST=../$(HERE)" ../$(HERE)/$@)

ippool_l.c: $(TOOL)/lexer.c $(TOP)/ip_pool.h
	(cd $(TOOL); $(MAKE) "DEST=../$(HERE)" ../$(HERE)/$@)

ippool_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); $(MAKE) "DEST=../$(HERE)" ../$(HERE)/$@)

iptrafcon.o: $(TOP)/iptrafcon.c
	$(CC) $(CCARGS) -c $(TOP)/iptrafcon.c -o $@

iptrafcon: $(IPTRAFCON) libipf.a
	$(CC) $(CCARGS) $(IPTRAFCON) -o $@ $(LIBS)

clean:
	${RM} -f ../ipf ../ipnat ../ipmon ../ippool ../ipftest
	${RM} -f ../ipscan ../ipsyncm ../ipsyncs
	${RM} -f core *.o *.a ipt ipfstat ipf ipfstat ipftest ipmon
	${RM} -f if_ipl ipnat $(LKM) libmilli.a ipf.psf
	${RM} -f ip_fil.c ipf_l.c ipf_y.c ipf_y.h ipf_l.h
	${RM} -f ipscan ipscan_y.c ipscan_y.h ipscan_l.c ipscan_l.h
	${RM} -f ippool ippool_y.c ippool_y.h ippool_l.c ippool_l.h
	${RM} -f ipnat_y.c ipnat_y.h ipnat_l.c ipnat_l.h
	${RM} -f ipmon_y.c ipmon_y.h ipmon_l.c ipmon_l.h
	${RM} -f ipsyncm ipsyncs ipfs ip_rules.c ip_rules.h
	${RM} -f ipnat.5 ipnat.4 ipnat.8 ipfilter.4 ipfilter.5
	${MAKE} -f Makefile.ipsend clean
	-(for i in *; do \
		if [ -d $${i} -a -h $${i}/Makefile ] ; then \
			cd $${i}; (make TOP=../.. clean); cd ..; \
			rm $${i}/Makefile $${i}/Makefile.ipsend; \
			rmdir $${i}; \
		fi \
	 done)

ipf.pkg:
	sed -e "s/HPREV/`uname -r`/g" ../ipf.psf.dist > ipf.psf
	swpackage -x media_type=tape -s ipf.psf @ /tmp/${DEPOT}
	mv /tmp/${DEPOT} ipf.pkg

install:
	sed -e "s/HPREV/`uname -r`/g" ../ipf.psf.dist > ipf.psf
	swpackage -s ipf.psf @ ${DEPOTDIR}/${DEPOT}
	swreg -l depot ${DEPOTDIR}/${DEPOT}
	@echo now do:
	@echo swinstall -s ${DEPOTDIR}/${DEPOT} -x reinstall=true IPF-RUN

manualinstall:
	mkdir -p -m 755 ${DEST} ${MANDIR} ${BINDEST} ${EXTRADIR} ${INCDIR}
	mkdir -p -m 700 ${ETCDIR}
	-$(INSTALL) -c -s ipftest $(BINDEST)/ipftest
	-$(INSTALL) -c -s ipmon $(BINDEST)/ipmon
	-$(INSTALL) -c -s ipsend $(BINDEST)/ipsend
	-$(INSTALL) -c -s ipresend $(BINDEST)/ipresend
	-$(INSTALL) -c $(TOP)/mkfilters $(BINDEST)/mkfilters
	-$(INSTALL) -c -s ipfs $(SBINDEST)/ipfs
	-$(INSTALL) -c -s ipnat $(SBINDEST)/ipnat
	-$(INSTALL) -c -s ipf $(SBINDEST)/ipf
	-$(INSTALL) -c -s ipfstat $(SBINDEST)/ipfstat
	-$(INSTALL) -c ipfboot $(RCDIR)
	-cp $(TOP)/man/*.[0-9] .
	-$(INSTALL) ipf.8 $(MANDIR)/man8
	-$(INSTALL) ipfs.8 $(MANDIR)/man8
	-$(INSTALL) ipnat.8 $(MANDIR)/man8
	-$(INSTALL) ipftest.1 $(MANDIR)/man1
	-$(INSTALL) mkfilters.1 $(MANDIR)/man1
	-$(INSTALL) ipf.4 $(MANDIR)/man4
	-$(INSTALL) ipfilter.4 $(MANDIR)/man4
	-$(INSTALL) ipnat.4 $(MANDIR)/man4
	-$(INSTALL) ipl.4 $(MANDIR)/man4
	-$(INSTALL) ipf.5 $(MANDIR)/man5
	-$(INSTALL) ipnat.5 $(MANDIR)/man5
	-$(INSTALL) ipfilter.5 $(MANDIR)/man5
	-$(INSTALL) ipfstat.8 $(MANDIR)/man8
	-$(INSTALL) ipmon.8 $(MANDIR)/man8
	-$(INSTALL) ipmon.5 $(MANDIR)/man5
	-$(INSTALL) ippool.8 $(MANDIR)/man8
	-$(INSTALL) ippool.5 $(MANDIR)/man5
	-$(INSTALL) ipscan.8 $(MANDIR)/man8
	-$(INSTALL) ipscan.5 $(MANDIR)/man5
	-cp $(TOP)/rules/* $(EXTRADIR)
	-$(INSTALL) -m 644 -c $(TOP)/ip_fil.h $(INCDIR)
	-$(INSTALL) -m 644 -c $(TOP)/ip_compat.h $(INCDIR)
	-$(INSTALL) -m 644 -c $(TOP)/ip_state.h $(INCDIR)
	-$(INSTALL) -m 644 -c $(TOP)/ip_nat.h $(INCDIR)
	-$(INSTALL) -m 644 -c $(TOP)/ip_frag.h $(INCDIR)
	-$(INSTALL) -m 644 -c $(TOP)/ip_proxy.h $(INCDIR)
	-$(INSTALL) -m 644 -c $(TOP)/ip_auth.h $(INCDIR)
	-touch ${ETCDIR}/ipf.conf ${ETCDIR}/ipnat.conf
