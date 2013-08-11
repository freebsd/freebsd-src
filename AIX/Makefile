#
# Copyright (C) 2012 by Darren Reed.
#
# See the IPFILTER.LICENCE file for details on licencing.
#
BINDEST=/usr/sbin
SBINDEST=/sbin
MANDIR=/usr/share/man
CC=cc
CFLAGS=-g -I$(TOP)
DEBUG=-O3
#
# For AIX 5.3
#
CPU=`uname -p`
INC=-I/usr/include
DEF=-D$(CPU) -D__$(CPU)__ -DINET -DKERNEL -D_KERNEL $(INC)
LKM=ipf
DLKM=
OBJ=.
TOP=..
HERE=AIX/$(OSREV)
AIX=`uname -v`
DEST=.
MFLAGS="BINDEST=$(BINDEST)" "SBINDEST=$(SBINDEST)" "MANDIR=$(MANDIR)" \
	'CFLAGS=$(CFLAGS)' "IPFLKM=$(IPFLKM)" \
	"IPFLOG=$(IPFLOG)" "LOGFAC=$(LOGFAC)" "POLICY=$(POLICY)" \
	"DEBUG=$(DEBUG)" "DCPU=$(CPU)" "CPUDIR=$(CPUDIR)" \
	"LOOKUP=$(LOOKUP)" "XID=$(XID)" "SCAN=$(SCAN)" "ALLOPTS=$(ALLOPTS)"
LIBS=-q$(BITS) -L. -lipf
CCARGS=$(DEBUG) $(CFLAGS) -I. -DAIX=$(AIX) -q$(BITS)
EXTRA=$(ALLOPTS)
MILLI=`../bootbits.sh`
FIXMILLI=-D_H_STRING=1 -Dbzero=bzero$(MILLI) \
-Dmemcmp=memcmp$(MILLI) \
-Dmemcpy=memcpy$(MILLI) \
-Dmemccpy=memccpy$(MILLI) \
-Dmemset=memset$(MILLI) \
-Dmemmove=memmove$(MILLI) \
-Dfill=fill$(MILLI) \
-Dstrstr=strstr$(MILLI)
#
########## ########## ########## ########## ########## ########## ##########
#
CP=/bin/cp
RM=/bin/rm
CHMOD=/bin/chmod
INSTALL=$(TOP)/bsdinstall
#
MODOBJS=ip_fil.o fil.o md5.o ip_nat.o ip_frag.o ip_state.o ip_nat6.o \
	ip_proxy.o ip_auth.o ip_log.o ip_pool.o ip_htable.o ip_lookup.o \
	ip_sync.o ip_scan.o ip_rules.o
DFLAGS=$(DEBUG) -DAIX=$(AIX) $(IPFLKM) $(IPFLOG) $(DEF) $(CFLAGS) $(DLKM) \
	$(FIXMILLI) $(IPFBPF) $(LOOKUP) $(XID) -I.
IPF=ipf.o ipfcomp.o ipf_y.o ipf_l.o
IPT=ipftest.o fil_u.o ip_frag_u.o ip_state_u.o ip_nat_u.o ip_nat6_u.o \
	ip_proxy_u.o ip_auth_u.o ip_fil_u.o ip_sync_u.o ip_scan_u.o \
	ip_log_u.o ip_pool_u.o ip_htable_u.o ip_lookup_u.o ip_rules_u.o \
	ipf_y.o ipf_l.o ipnat_y.o ipnat_l.o ippool_y.o ippool_l.o \
	md5_u.o radix_ipf_u.o
IPNAT=ipnat.o ipnat_y.o ipnat_l.o
IPMON=ipmon.o ipmon_y.o ipmon_l.o
IPPOOL=ippool_y.o ippool_l.o kmem.o ippool.o
FILS=ipfstat.o
LIBSRC=$(TOP)/lib
RANLIB=ranlib
AROPTS=-X $(BITS) crs
TOOL=$(TOP)/tools

include $(TOP)/lib/Makefile

build all: ./libipf.a ipf.exe ipfs ipfstat ipftest ipmon \
	ipnat ippool ipscan ipsyncm ipsyncs cfg_ipf $(LKM)
	-sh -c 'for i in ipftest ipmon ippool ipnat ipscan ipsyncm ipsyncs; do /bin/rm -f $(TOP)/$$i; ln -s `pwd`/$$i $(TOP); done'
	-ln -s `pwd`/ipf.exe $(TOP)/ipf

ipfstat: $(FILS) ./libipf.a
	$(CC) $(CCARGS) $(STATETOP_CFLAGS) $(STATETOP_INC) $(FILS) \
	-o $@ $(LIBS) $(STATETOP_LIB)

ipf.exe: $(IPF) ./libipf.a
	$(CC) $(CCARGS) $(IPF) -o $@ $(LIBS) $(LIBBPF)

ipftest: $(IPT) ./libipf.a
	$(CC) $(CCARGS) $(IPT) -o $@ $(LIBS) $(LIBBPF)

ipnat: $(IPNAT) ./libipf.a
	$(CC) $(CCARGS) $(IPNAT) -o $@ $(LIBS)

ipfs: ipfs.o ./libipf.a
	$(CC) $(CCARGS) ipfs.o -o $@ $(LIBS)

ipsyncm: ipsyncm.o ./libipf.a
	$(CC) $(CCARGS) ipsyncm.o -o $@ $(LIBS)

ipsyncs: ipsyncs.o ./libipf.a
	$(CC) $(CCARGS) ipsyncs.o -o $@ $(LIBS)

ipsyncm.o: $(TOOL)/ipsyncm.c $(TOP)/ip_sync.h
	$(CC) $(CCARGS) -c $(TOOL)/ipsyncm.c -o $@

ipsyncs.o: $(TOOL)/ipsyncs.c $(TOP)/ip_sync.h
	$(CC) $(CCARGS) -c $(TOOL)/ipsyncs.c -o $@

tests:
	(cd test; make )

ipfstat.o: $(TOOL)/ipfstat.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_frag.h \
		$(TOP)/ip_compat.h $(TOP)/ip_state.h $(TOP)/ip_nat.h
	$(CC) $(CCARGS) $(STATETOP_CFLAGS) $(STATETOP_INC) \
		-c $(TOOL)/ipfstat.c -o $@

ipfs.o: $(TOOL)/ipfs.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_state.h \
		$(TOP)/ip_nat.h
	$(CC) $(CCARGS) -c $(TOOL)/ipfs.c -o $@

fil_u.o: $(TOP)/fil.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) $(FIXRADIX) $(IPFBPF) -c $(TOP)/fil.c -o $@

ipf.o: $(TOOL)/ipf.c $(TOP)/ip_fil.h $(TOP)/ipf.h
	$(CC) $(CCARGS) -c $(TOOL)/ipf.c -o $@

ipf_y.o: ipf_y.c ipf_y.h $(TOP)/ipf.h ipf_l.h
	$(CC) $(CCARGS) $(IPFBPF) -c ipf_y.c -o $@

ipf_l.o: ipf_l.c ipf_y.h $(TOP)/ipf.h ipf_l.h
	$(CC) $(CCARGS) -I. -c ipf_l.c -o $@

ipf_y.h ipf_y.c: $(TOOL)/ipf_y.y $(TOP)/ip_pool.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ipf_l.c ipf_l.h: $(TOOL)/lexer.c $(TOP)/ipf.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ipfcomp.o: $(TOOL)/ipfcomp.c $(TOP)/ip_fil.h $(TOP)/ipf.h
	$(CC) $(CCARGS) -c $(TOOL)/ipfcomp.c -o $@

ipftest.o: $(TOOL)/ipftest.c $(TOP)/ip_fil.h $(TOP)/ipt.h $(TOP)/ipf.h
	$(CC) $(CCARGS) -c $(TOOL)/ipftest.c -o $@

ipnat.o: $(TOOL)/ipnat.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_nat.h
	$(CC) $(CCARGS) -c $(TOOL)/ipnat.c -o $@

ipnat_y.o: ipnat_y.c ipnat_y.h $(TOP)/ip_fil.h $(TOP)/ip_compat.h \
		$(TOP)/ipf.h $(TOP)/ip_nat.h ipnat_l.h
	$(CC) $(CCARGS) -c ipnat_y.c -o $@

ipnat_l.o: ipnat_l.c ipnat_y.h $(TOP)/ip_fil.h $(TOP)/ip_compat.h \
		$(TOP)/ipf.h $(TOP)/ip_nat.h ipnat_l.h
	$(CC) $(CCARGS) -I. -c ipnat_l.c -o $@

ipnat_y.h ipnat_y.c: $(TOOL)/ipnat_y.y $(TOP)/ip_nat.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ipnat_l.c ipnat_l.h: $(TOOL)/lexer.c $(TOP)/ip_nat.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ip_rules.c: $(TOP)/rules/ip_rules $(TOP)/tools/ipfcomp.c ipf.exe
	./ipf.exe -cc -nf $(TOP)/rules/ip_rules

$(TOP)/ip_rules.h: ip_rules.c
	if [ ! -f $(TOP)/ip_rules.h ] ; then \
		/bin/mv -f ip_rules.h $(TOP); \
	else \
		touch $(TOP)/ip_rules.h; \
	fi

ip_nat_u.o: $(TOP)/ip_nat.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_nat.c -o $@

ip_nat6_u.o: $(TOP)/ip_nat6.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_nat6.c -o $@

ip_proxy_u.o: $(TOP)/ip_proxy.c $(TOP)/ip_proxy.h $(TOP)/ip_compat.h \
	$(TOP)/ip_ftp_pxy.c $(TOP)/ip_rcmd_pxy.c $(TOP)/ip_raudio_pxy.c \
	$(TOP)/ip_rpcb_pxy.c $(TOP)/ip_ipsec_pxy.c $(TOP)/ip_nat.h \
	$(TOP)/ip_fil.h
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
	$(CC) $(CCARGS) $(EXTRA) $(FIXRADIX) -c $(TOP)/ip_fil.c -o $@

ip_scan_u.o: $(TOP)/ip_scan.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_scan.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_scan.c -o $@

ip_sync_u.o: $(TOP)/ip_sync.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_sync.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_sync.c -o $@

ip_rules_u.o: ip_rules.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h $(TOP)/ip_rules.h
	$(CC) $(CCARGS) $(EXTRA) -c ip_rules.c -o $@

ip_log_u.o: $(TOP)/ip_log.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_log.c -o $@

ip_pool_u.o: $(TOP)/ip_pool.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
		$(TOP)/ip_pool.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_pool.c -o $@

ip_htable_u.o: $(TOP)/ip_htable.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
		$(TOP)/ip_htable.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_htable.c -o $@

ip_lookup_u.o: $(TOP)/ip_lookup.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
		$(TOP)/ip_lookup.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_lookup.c -o $@

bpf_filter_u.o: $(TOP)/bpf_filter.c $(TOP)/pcap-ipf.h $(TOP)/bpf-ipf.h
	$(CC) $(CCARGS) -c $(TOP)/bpf_filter.c -o $@

md5_u.o: $(TOP)/md5.c $(TOP)/md5.h
	$(CC) $(CCARGS) -c $(TOP)/md5.c -o $@

radix_ipf_u.o: $(TOP)/radix_ipf.c $(TOP)/radix_ipf.h
	$(CC) $(CCARGS) -c $(TOP)/radix_ipf.c -o $@

cfg_ipf: cfg_ipf.o
#	ld -o cfg_ipf cfg_ipf.o -L/usr/lib/ia64l32 -L/usr/ccs/lib/ia64l32 \
#	   -lodm -lrts -lcfg
	$(CC) $(CCARGS) -o cfg_ipf cfg_ipf.o -lodm -lrts -lcfg

cfg_ipf.o: ../aix_cfg_ipf.c
	$(CC) -D_ALL_SOURCE -D_KERNEL $(CCARGS) -c ../aix_cfg_ipf.c -o $@

$(LKM): $(MODOBJS)  ../ipfkext.exp /lib/kernex.exp /lib/syscalls.exp
	ld -G -eipfconfig -bimport:/lib/syscalls.exp -bimport:/lib/kernex.exp \
	   -bimport:/lib/kernex.exp -bimport:/lib/netinet.exp \
	   -bimport:/lib/statcmd.exp \
	   -lcsys -lsys -bexport:../ipfkext.exp -bmap:ipfkext.map \
	   $(MODOBJS) -o $(LKM)32
	/bin/rm -f $(LKM)
	ar -X $(BITS) cq $(LKM) $(LKM)32

fil.o: $(TOP)/fil.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_compat.h $(TOP)/ipl.h
	$(CC) $(POLICY) $(DFLAGS) $(IPFBPF) -c $(TOP)/fil.c -o $@

ip_nat.o: $(TOP)/ip_nat.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(DFLAGS) -c $(TOP)/ip_nat.c -o $@

ip_nat6.o: $(TOP)/ip_nat6.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(DFLAGS) -c $(TOP)/ip_nat6.c -o $@

ip_frag.o: $(TOP)/ip_frag.c $(TOP)/ip_frag.h $(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(DFLAGS) -c $(TOP)/ip_frag.c -o $@

ip_state.o: $(TOP)/ip_state.c $(TOP)/ip_state.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h $(TOP)/ip_nat.h
	$(CC) $(DFLAGS) -DIPSTATE_SIZE=127 -c $(TOP)/ip_state.c -o $@

ip_proxy.o: $(TOP)/ip_proxy.c $(TOP)/ip_proxy.h $(TOP)/ip_compat.h \
	$(TOP)/ip_ftp_pxy.c $(TOP)/ip_rcmd_pxy.c $(TOP)/ip_raudio_pxy.c \
	$(TOP)/ip_rpcb_pxy.c $(TOP)/ip_ipsec_pxy.c $(TOP)/ip_nat.h \
	$(TOP)/ip_nat.h $(TOP)/ip_fil.h
	$(CC) $(DFLAGS) -c $(TOP)/ip_proxy.c -o $@

ip_auth.o: $(TOP)/ip_auth.c $(TOP)/ip_auth.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h
	$(CC) $(DFLAGS) -c $(TOP)/ip_auth.c -o $@

ip_fil.o: $(TOP)/ip_fil_aix.c $(TOP)/ip_fil.h $(TOP)/ip_compat.h $(TOP)/ip_nat.h
	$(CC) $(DFLAGS) $(COMPIPF) -c $(TOP)/ip_fil_aix.c -o $@

ip_log.o: $(TOP)/ip_log.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(DFLAGS) -c $(TOP)/ip_log.c -o $@

ip_scan.o: $(TOP)/ip_scan.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h $(TOP)/ip_scan.h
	$(CC) $(DFLAGS) -c $(TOP)/ip_scan.c -o $@

ip_sync.o: $(TOP)/ip_sync.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h $(TOP)/ip_sync.h
	$(CC) $(DFLAGS) -c $(TOP)/ip_sync.c -o $@

radix_ipf.o: $(TOP)/radix_ipf.c $(TOP)/radix_ipf.h
	$(CC) $(DFLAGS) -c $(TOP)/radix_ipf.c -o $@

ip_pool.o: $(TOP)/ip_pool.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_pool.h $(TOP)/radix_ipf.h
	$(CC) $(DFLAGS) $(FIXRADIX) -c $(TOP)/ip_pool.c -o $@

ip_htable.o: $(TOP)/ip_htable.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_htable.h
	$(CC) $(DFLAGS) -c $(TOP)/ip_htable.c -o $@

ip_lookup.o: $(TOP)/ip_lookup.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_lookup.h
	$(CC) $(DFLAGS) $(FIXRADIX) -c $(TOP)/ip_lookup.c -o $@

ip_rules.o: ip_rules.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h $(TOP)/ip_rules.h
	$(CC) -I. $(DFLAGS) -c ip_rules.c -o $@

ip_rulesx.o: ip_rules.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h $(TOP)/ip_rules.h
	$(CC) -I. -DIPFILTER_COMPILED $(DFLAGS) -c ip_rules.c -o $@

#aix.o: $(TOP)/aix.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h
#	$(CC) $(DFLAGS) -c $(TOP)/aix.c -o $@

md5.o: $(TOP)/md5.c $(TOP)/md5.h
	$(CC) $(DFLAGS) -c $(TOP)/md5.c -o $@

ipmon: $(IPMON) ./libipf.a
	$(CC) $(CCARGS) $(LOGFAC) $(IPMON) -o $@ $(LIBS) -ll

ipmon.o: $(TOOL)/ipmon.c $(TOP)/ipmon.h
	$(CC) $(CCARGS) $(LOGFAC) -c $(TOOL)/ipmon.c -o $@

ipmon_y.o: ipmon_y.c ipmon_y.h $(TOP)/ipmon.h ipmon_l.h
	$(CC) $(CCARGS) -c ipmon_y.c -o $@

ipmon_l.o: ipmon_l.c ipmon_y.h $(TOP)/ipmon.h ipmon_l.h
	$(CC) $(CCARGS) -I. -c ipmon_l.c -o $@

ipmon_y.h ipmon_y.c: $(TOOL)/ipmon_y.y $(TOP)/ipmon.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ipmon_l.c ipmon_l.h: $(TOOL)/lexer.c $(TOP)/ipmon.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ipscan: ipscan_y.o ipscan_l.o
	$(CC) $(DEBUG) ipscan_y.o ipscan_l.o -o $@ -ll $(LIBS)

ipscan_y.o: ipscan_y.c ipscan_y.h $(TOP)/ip_scan.h ipscan_l.h
	$(CC) $(CCARGS) -c ipscan_y.c -o $@

ipscan_l.o: ipscan_l.c ipscan_y.h $(TOP)/ip_scan.h ipscan_l.h
	$(CC) $(CCARGS) -I. -c ipscan_l.c -o $@

ipscan_y.h ipscan_y.c: $(TOOL)/ipscan_y.y $(TOP)/ip_scan.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ipscan_l.c ipscan_l.h: $(TOOL)/lexer.c $(TOP)/ip_scan.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ippool: $(IPPOOL)
	$(CC) $(DEBUG) -I. $(CFLAGS) $(IPPOOL) -o $@ $(LIBS) -ll

ippool.o: $(TOOL)/ippool.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_pool.h
	$(CC) $(CCARGS) -c $(TOOL)/ippool.c -o $@

ippool_y.o: ippool_y.c ippool_y.h $(TOP)/ip_pool.h ippool_l.h
	$(CC) $(CCARGS) -c ippool_y.c -o $@

ippool_l.o: ippool_l.c ippool_y.h $(TOP)/ip_pool.h ippool_l.h
	$(CC) $(CCARGS) -I. -c ippool_l.c -o $@

ippool_y.h ippool_y.c: $(TOOL)/ippool_y.y $(TOP)/ip_pool.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ippool_l.c ippool_l.h: $(TOOL)/lexer.c $(TOP)/ip_pool.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

.y.c:

.l.c:

clean:
	${RM} -f ../ipf.exe ../ipnat ../ipmon ../ippool ../ipftest
	${RM} -f ../ipscan ../ipsyncm ../ipsyncs cfg_ipf
	${RM} -f *.core *.o *.a *.o.d
	${RM} -f fils ipf.exe ipfstat ipftest ipmon if_ipl ipnat
	${RM} -f $(LKM) ioconf.h *.ko setdef1.c setdef0.c setdefs.h
	${RM} -f ip_fil.c ipf_l.c ipf_y.c ipf_y.h ipf_l.h
	${RM} -f ipscan ipscan_y.c ipscan_y.h ipscan_l.c ipscan_l.h
	${RM} -f ippool ippool_y.c ippool_y.h ippool_l.c ippool_l.h
	${RM} -f ipnat_y.c ipnat_y.h ipnat_l.c ipnat_l.h
	${RM} -f ipmon_y.c ipmon_y.h ipmon_l.c ipmon_l.h
	${RM} -f ipscan ipscan_y.c ipscan_y.h ipscan_l.c y.tab.? lex.yy.c ipfs
	${RM} -f ipsyncm ipsyncs ip_rules.c ip_rules.h

	${MAKE} -f Makefile.ipsend ${MFLAGS} clean
	-(for i in *; do \
		if [ -d $${i} -a -f $${i}/Makefile ] ; then \
			cd $${i}; (make TOP=../.. clean); cd ..; \
			rm $${i}/Makefile $${i}/Makefile.ipsend; \
			rmdir $${i}; \
		fi \
	done)

install:
	cp cfg_ipf /usr/lib/methods
	cp $(LKM) /usr/lib/drivers/ipf
	-for i in ipfs ipnat ipscan ipfstat; do \
		$(INSTALL) -s -c -g system -m 755 -o root $$i $(SBINDEST); \
	done
	$(INSTALL) -s -c -g system -m 755 -o root ipf.exe $(SBINDEST)/ipf
	-for i in ipmon ipftest ipsyncs ipsyncm; do \
		$(INSTALL) -s -c -g system -m 755 -o root $$i $(BINDEST); \
	done
	(cd $(TOP)/man; make INSTALL=../bsdinstall MANDIR=$(MANDIR) install; cd $(TOP))
	sync
