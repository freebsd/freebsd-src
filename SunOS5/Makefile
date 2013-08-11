#
# Copyright (C) 2012 by Darren Reed.
#
# See the IPFILTER.LICENCE file for details on licencing.
#
TOP=$(shell echo `pwd`/..)
TOP:sh=echo `pwd`/..
BINDEST=/usr/local/bin
SBINDEST=/sbin/$(OBJ)
MANDIR=/usr/share/man
CC=cc
CFLAGS=-I$(TOP)
#
# For SunOS 5.x
#
CPUDIR=$(shell echo `uname -p`-`uname -r`)
CPUDIR:sh=echo `uname -p`-`uname -r`
ROOT=$(CPUDIR)/root
PKGDIR=$(ROOT)/opt/ipf
PKGMAN=$(PKGDIR)/man
PKGBIN=$(PKGDIR)/bin/$(OBJ)
#CTFCONVERT=ctfconvert -i -l IPF
CTFCONVERT=echo
#CTFMERGE=ctfmerge -l IPF -o
CTFMERGE=echo
#
DEST=$(OBJ)
TOOL=$(TOP)/tools
#CPU=$(shell uname -p)
#REV=$(shell uname -v)
#CPU:sh=uname -p
#REV:sh=uname -r
HERE=SunOS5/$(CPUDIR)
BITS=$(shell if optisa amd64 sparcv9 >/dev/null 2>&1; then echo "64"; else echo "32"; fi)
BITS:sh=if optisa amd64 sparcv9 >/dev/null 2>&1; then echo "64"; else echo "32"; fi
ARCH=$(shell uname -p)
ARCH:sh=uname -p

OBJ32_i386=i86
OBJ64_i386=amd64
OBJ32_sparc=sparcv7
OBJ64_sparc=sparcv9
OBJ32=$(OBJ32_$(ARCH))
OBJ64=$(OBJ64_$(ARCH))

PROTO32_i386=prototype_ia32
PROTO64_i386=prototype_amd64
PROTO32_sparc=prototype32
PROTO64_sparc=prototype64
PROTO32=$(PROTO32_$(ARCH))
PROTO64=$(PROTO64_$(ARCH))

CC=gcc -Wall
DEBUG=-g
LIBS=-lsocket -lnsl -lelf -L$(OBJ) -lipf $(LIBBPF)
DEF=-I$(TOP)/../pfil -D_KERNEL -DSUNDDI $(IPFBPF) $(INSTANCE) -gdwarf-2
OBJ=.
PKG=ipf
PROTO=prototype
BITTYPE=`if [ "$(BITS)" = 64 ];then echo ' (64-bit)'; fi`
OBJDIR=$(CPUDIR)/$(OBJ)
TMP=/tmp/ipfpkgs
ATON=-DNEED_INET_ATON
ROOTINC=$(ROOT)/usr/include
#
MFLAGS="BINDEST=$(BINDEST)" "SBINDEST=$(SBINDEST)" "MANDIR=$(MANDIR)" \
        'CFLAGS=$(CFLAGS) $(ARCHINC) $(SOLARIS2)' "IPFLKM=$(IPFLKM)" \
        "IPFLOG=$(IPFLOG)" "LOGFAC=$(LOGFAC)" "POLICY=$(POLICY)" \
        "SOLARIS2=$(SOLARIS2)" "DEBUG=$(DEBUG)" "CPUDIR=$(CPUDIR)" \
	"BITS=$(BITS)" "OBJ=$(OBJ)" "LOOKUP=$(LOOKUP)" "SYNC=$(SYNC)" \
	"XID=$(XID)" "ALLOPTS=$(ALLOPTS)"
#
########## ########## ########## ########## ########## ########## ##########
#
CP=/bin/cp
RM=/bin/rm
CHMOD=/bin/chmod
INSTALL=$(TOP)/bsdinstall
LIBSRC=$(TOP)/lib
RANLIB=echo
AROPTS=crs
#
DFLAGS=$(IPFLKM) $(DEF) $(SOLARIS2) $(IPFLOG) $(LOOKUP) $(SYNC) $(XID)
#-DIPFDEBUG
MODOBJS=$(OBJ)/ip_fil.o $(OBJ)/fil.o $(OBJ)/solaris.o $(OBJ)/ip_state.o \
    $(OBJ)/ip_frag.o $(OBJ)/ip_nat.o $(OBJ)/ip_proxy.o $(OBJ)/ip_auth.o \
    $(OBJ)/ip_pool.o $(OBJ)/ip_htable.o $(OBJ)/ip_lookup.o $(OBJ)/ip_log.o \
    $(OBJ)/ip_scan.o $(OBJ)/ip_sync.o $(OBJ)/radix_ipf.o $(OBJ)/md5.o \
    $(OBJ)/ip_nat6.o $(OBJ)/ip_dstlist.o $(OBJ)/ip_fil_compat.o \
    $(OBJ)/arc4random.o $(OBJ)/mblk_misc.o $(BPFILTER)
#	$(OBJ)/ip_trafcon.o
IPF=$(OBJ)/ipf.o $(OBJ)/ipfcomp.o $(OBJ)/ipf_y.o $(OBJ)/ipf_l.o
IPT=$(OBJ)/ipftest.o $(OBJ)/ip_fil_u.o $(OBJ)/ip_state_u.o \
    $(OBJ)/ip_frag_u.o $(OBJ)/ip_nat_u.o $(OBJ)/fil_u.o \
    $(OBJ)/ip_scan_u.o $(OBJ)/ip_sync_u.o $(OBJ)/ip_pool_u.o \
    $(OBJ)/ip_htable_u.o $(OBJ)/ip_lookup_u.o $(OBJ)/ip_proxy_u.o \
    $(OBJ)/ip_auth_u.o $(OBJ)/ip_log_u.o $(OBJ)/ip_rules_u.o\
    $(OBJ)/ippool_y.o $(OBJ)/ippool_l.o $(OBJ)/ipnat_y.o $(OBJ)/ipnat_l.o \
    $(OBJ)/ipf_y.o $(OBJ)/ipf_l.o $(OBJ)/radix_ipf_u.o $(OBJ)/md5_u.o \
    $(OBJ)/ip_nat6_u.o $(OBJ)/ip_dstlist_u.o
#	$(OBJ)/ip_trafcon_u.o
IPNAT=$(OBJ)/ipnat.o $(OBJ)/ipnat_y.o $(OBJ)/ipnat_l.o
IPMON=$(OBJ)/ipmon.o $(OBJ)/ipmon_y.o $(OBJ)/ipmon_l.o
IPPOOL=$(OBJ)/ippool_y.o $(OBJ)/ippool_l.o $(OBJ)/kmem.o $(OBJ)/ippool.o
IPTRAFCON=$(OBJ)/iptrafcon.o
FILS=$(OBJ)/ipfstat.o
#
CCARGS=-I. $(DEBUG) $(CFLAGS)
EXTRA=$(ALLOPTS)

include $(TOP)/lib/Makefile

$(OBJ):
	@if [ ! -d $(OBJ) ]; then mkdir $(OBJ); fi


sunos5 solaris2 build: $(OBJ) $(OBJ)/libipf.a $(OBJ)/ipf.exe $(OBJ)/ipfstat \
    $(OBJ)/ipftest $(OBJ)/ipmon $(OBJ)/ipnat $(OBJ)/ipf $(OBJ)/ipfs \
    $(OBJ)/ippool $(OBJ)/ipfrule
	/bin/rm -f $(TOP)/ipf $(TOP)/$(OBJ)/ipf
	if [ -f /usr/lib/isaexec -a "$(OBJ)" != . ] ; then \
		mkdir -p $(TOP)/$(OBJ); \
		cp /usr/lib/isaexec $(TOP)/ipf; \
	fi
	ln -s `pwd`/$(OBJ)/ipf.exe $(TOP)/$(OBJ)/ipf
	-(for i in ipftest ipmon ippool ipnat ipscan ipsyncm ipsyncs; do \
		/bin/rm -f $(TOP)/$$i $(TOP)/$(OBJ)/$$i; \
		if [ -f /usr/lib/isaexec -a "$(OBJ)" != "." ] ; then \
			ln $(TOP)/ipf $(TOP)/$$i; \
		fi; \
		ln -s `pwd`/$(OBJ)/$$i $(TOP)/$(OBJ)/$$i; \
		done;\
	)

pkg: $(CPUDIR)/ipf.pkg

$(OBJ)/ipfstat: $(FILS) $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(FILS) -o $@ $(LIBS) $(STATETOP_LIB) -lkvm

$(OBJ)/ipf.exe: $(IPF) $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(IPF) -o $@ $(LIBS) -ll $(LIBBPF)

# Magic dependency on ipf.exe creates the $(TOP)/$(OBJ{32,64}) directories.
$(OBJ)/ipftest: $(IPT) $(OBJ)/ipf.exe $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(IPT) -o $@ $(LIBS) -ll $(LIBBPF)

$(OBJ)/ipnat: $(IPNAT) $(OBJ)/libipf.a
	$(CC) $(CFLAGS) $(IPNAT) -o $@ $(LIBS) -lkvm -ll

$(OBJ)/ipfs: $(OBJ)/ipfs.o
	$(CC) $(CCARGS) $(OBJ)/ipfs.o -o $@

$(OBJ)/ipsyncm: $(OBJ)/ipsyncm.o $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(OBJ)/ipsyncm.o -o $@ $(LIBS)

$(OBJ)/ipsyncs: $(OBJ)/ipsyncs.o $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(OBJ)/ipsyncs.o -o $@ $(LIBS)

$(OBJ)/ipsyncm.o: $(TOOL)/ipsyncm.c $(TOP)/ip_sync.h
	$(CC) $(CCARGS) -c $(TOOL)/ipsyncm.c -o $@

$(OBJ)/ipsyncs.o: $(TOOL)/ipsyncs.c $(TOP)/ip_sync.h
	$(CC) $(CCARGS) -c $(TOOL)/ipsyncs.c -o $@

tests:
	(cd test; make )

$(OBJ)/ipfstat.o: $(TOOL)/ipfstat.c $(TOP)/ip_fil.h $(TOP)/ipf.h \
		$(TOP)/ip_state.h $(TOP)/ip_frag.h
	$(CC) $(CCARGS) $(STATETOP_CFLAGS) $(STATETOP_INC) \
		-c $(TOOL)/ipfstat.c -o $@

$(OBJ)/ipfs.o: $(TOOL)/ipfs.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_state.h \
		$(TOP)/ip_nat.h
	$(CC) $(CCARGS) -c $(TOOL)/ipfs.c -o $@

$(OBJ)/fil_u.o: $(TOP)/fil.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_nat.h
	$(CC) $(CCARGS) $(EXTRA) $(IPFBPF) -c $(TOP)/fil.c -o $@

$(OBJ)/fil.o: $(TOP)/fil.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_nat.h
	$(CC) -I$(TOP) $(POLICY) $(DFLAGS) $(IPFBPF) $(COMPATIPF) \
		-c $(TOP)/fil.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ipf.o: $(TOOL)/ipf.c $(TOP)/ip_fil.h $(TOP)/ipf.h
	$(CC) $(CCARGS) -c $(TOOL)/ipf.c -o $@

$(OBJ)/ipfcomp.o: $(TOOL)/ipfcomp.c $(TOP)/ip_fil.h $(TOP)/ipf.h
	$(CC) $(CCARGS) -c $(TOOL)/ipfcomp.c -o $@

$(OBJ)/ipf_y.o: ipf_y.c $(TOP)/ip_fil.h $(TOP)/ipf.h ipf_y.h ipf_l.h
	$(CC) $(CCARGS) $(IPFBPF) -c ipf_y.c -o $@

$(OBJ)/ipf_l.o: ipf_l.c $(TOP)/ip_fil.h $(TOP)/ipf.h ipf_y.h ipf_l.h
	$(CC) $(CCARGS) -I. -I$(TOP)/tools -c ipf_l.c -o $@

ipf_y.h ipf_y.c: $(TOOL)/ipf_y.y $(TOP)/ipf.h $(TOP)/ip_fil.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ipf_l.c: $(TOOL)/lexer.c $(TOP)/ipf.h $(TOP)/ip_fil.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ipf_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

$(OBJ)/ipftest.o: $(TOOL)/ipftest.c $(TOP)/ip_fil.h $(TOP)/ipt.h $(TOP)/ipf.h
	$(CC) $(CCARGS) -c $(TOOL)/ipftest.c -o $@

$(OBJ)/ipnat.o: $(TOOL)/ipnat.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_nat.h
	$(CC) $(CCARGS) -c $(TOOL)/ipnat.c -o $@

$(OBJ)/ipnat_y.o: ipnat_y.c ipnat_y.h $(TOP)/ip_nat.h ipnat_l.h
	$(CC) $(CCARGS) -c ipnat_y.c -o $@

$(OBJ)/ipnat_l.o: ipnat_l.c ipnat_y.h $(TOP)/ip_nat.h ipnat_l.h
	$(CC) $(CCARGS) -I. -I$(TOP)/tools -c ipnat_l.c -o $@

ipnat_y.h ipnat_y.c: $(TOOL)/ipnat_y.y $(TOP)/ip_nat.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ipnat_l.c: $(TOOL)/lexer.c $(TOP)/ip_nat.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ipnat_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

$(OBJ)/mlso_rule.o: $(TOP)/mlso_rule.c
	$(CC) -I$(TOP) $(DFLAGS) -c $(TOP)/mlso_rule.c -o $@

$(OBJ)/ip_rules.o: $(OBJ)/ip_rules.c $(TOP)/ip_rules.h
	$(CC) -I$(TOP) $(DFLAGS) -c $(OBJ)/ip_rules.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ip_rulesx.o: $(OBJ)/ip_rules.c $(TOP)/ip_rules.h
	$(CC) -I$(TOP) -DIPFILTER_COMPILED $(DFLAGS) -c $(OBJ)/ip_rules.c -o $@

$(OBJ)/ipfrule: $(OBJ)/ip_rulesx.o $(OBJ)/mlso_rule.o
	ld -r $(OBJ)/ip_rulesx.o $(OBJ)/mlso_rule.o -o $@

$(OBJ)/ipf: $(MODOBJS)
	if [ -f /kernel/misc/sparcv9/neti -o -f /kernel/misc/neti ] ; then \
		ld -dy -Nmisc/hook -Nmisc/neti -Nmisc/md5 -Ndrv/ip -r $(MODOBJS) -o $@; \
	else \
		ld -dy -Ndrv/ip -Ndrv/pfil -Nmisc/md5 -r $(MODOBJS) -o $@; \
	fi
	$(CTFMERGE) $@ $(MODOBJS)

$(OBJ)/md5.o: $(TOP)/md5.c $(TOP)/md5.h
	$(CC) -I$(TOP) $(DFLAGS) -c $(TOP)/md5.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/radix_ipf.o: $(TOP)/md5.c $(TOP)/radix_ipf.h $(TOP)/radix_ipf.c
	$(CC) -I$(TOP) $(DFLAGS) -c $(TOP)/radix_ipf.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ip_nat.o: $(TOP)/ip_nat.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) -c $(TOP)/ip_nat.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ip_nat6.o: $(TOP)/ip_nat6.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) -c $(TOP)/ip_nat6.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ip_state.o: $(TOP)/ip_state.c $(TOP)/ip_state.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) -c $(TOP)/ip_state.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ip_scan.o: $(TOP)/ip_scan.c $(TOP)/ip_scan.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) -c $(TOP)/ip_scan.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ip_sync.o: $(TOP)/ip_sync.c $(TOP)/ip_sync.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) -c $(TOP)/ip_sync.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ip_proxy.o: $(TOP)/ip_proxy.c $(TOP)/ip_proxy.h $(TOP)/ip_compat.h \
        $(TOP)/ip_ftp_pxy.c $(TOP)/ip_rcmd_pxy.c $(TOP)/ip_raudio_pxy.c \
        $(TOP)/ip_rpcb_pxy.c $(TOP)/ip_ipsec_pxy.c $(TOP)/ip_dns_pxy.c \
	$(TOP)/ip_tftp_pxy.c $(TOP)/ip_nat.h $(TOP)/ip_pptp_pxy.c \
	$(TOP)/ip_fil.h
	$(CC) -I$(TOP) $(DFLAGS) -c $(TOP)/ip_proxy.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ip_frag.o: $(TOP)/ip_frag.c $(TOP)/ip_frag.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) -c $(TOP)/ip_frag.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ip_auth.o: $(TOP)/ip_auth.c $(TOP)/ip_auth.h $(TOP)/ip_compat.h
	$(CC) -I$(TOP) $(DFLAGS) -c $(TOP)/ip_auth.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ip_pool.o: $(TOP)/ip_pool.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_pool.h $(TOP)/ip_lookup.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/ip_pool.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ip_htable.o: $(TOP)/ip_htable.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_htable.h $(TOP)/ip_lookup.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/ip_htable.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ip_dstlist.o: $(TOP)/ip_dstlist.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_dstlist.h $(TOP)/ip_lookup.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/ip_dstlist.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ip_fil_compat.o: $(TOP)/ip_fil_compat.c $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h $(TOP)/ip_state.h $(TOP)/ip_nat.h
	$(CC) $(CCARGS) $(DFLAGS) $(COMPATIPF) -c $(TOP)/ip_fil_compat.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ip_lookup.o: $(TOP)/ip_lookup.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_pool.h $(TOP)/ip_htable.h $(TOP)/ip_lookup.h \
	$(TOP)/ip_dstlist.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/ip_lookup.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/arc4random.o: $(TOP)/arc4random.c
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/arc4random.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/mblk_misc.o: $(TOP)/SunOS5/mblk_misc.c
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/SunOS5/mblk_misc.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ip_trafcon.o: $(TOP)/ip_trafcon.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_trafcon.h
	$(CC) $(CCARGS) $(DFLAGS) -c $(TOP)/ip_trafcon.c -o $@

$(OBJ)/bpf_filter.o: $(TOP)/bpf_filter.c $(TOP)/bpf-ipf.h $(TOP)/pcap-ipf.h
	$(CC) -I$(TOP) $(DFLAGS) -c $(TOP)/bpf_filter.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/bpf_filter_u.o: $(TOP)/bpf_filter.c
	$(CC) -I$(TOP) $(DFLAGS) -c $(TOP)/bpf_filter.c -o $@

$(OBJ)/md5_u.o: $(TOP)/md5.c $(TOP)/md5.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/md5.c -o $@

$(OBJ)/radix_ipf_u.o: $(TOP)/radix_ipf.c $(TOP)/radix_ipf.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/radix_ipf.c -o $@

$(OBJ)/ip_nat_u.o: $(TOP)/ip_nat.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_nat.c -o $@

$(OBJ)/ip_nat6_u.o: $(TOP)/ip_nat6.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_nat6.c -o $@

$(OBJ)/ip_frag_u.o: $(TOP)/ip_frag.c $(TOP)/ip_frag.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_frag.c -o $@

$(OBJ)/ip_state_u.o: $(TOP)/ip_state.c $(TOP)/ip_state.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_state.c -o $@

$(OBJ)/ip_scan_u.o: $(TOP)/ip_scan.c $(TOP)/ip_scan.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_scan.c -o $@

$(OBJ)/ip_sync_u.o: $(TOP)/ip_sync.c $(TOP)/ip_sync.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_sync.c -o $@

$(OBJ)/ip_pool_u.o: $(TOP)/ip_pool.c $(TOP)/ip_pool.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h $(TOP)/ip_lookup.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_pool.c -o $@

$(OBJ)/ip_htable_u.o: $(TOP)/ip_htable.c $(TOP)/ip_htable.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h $(TOP)/ip_lookup.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_htable.c -o $@

$(OBJ)/ip_dstlist_u.o: $(TOP)/ip_dstlist.c $(TOP)/ip_dstlist.h \
	$(TOP)/ip_compat.h $(TOP)/ip_fil.h $(TOP)/ip_lookup.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_dstlist.c -o $@

$(OBJ)/ip_lookup_u.o: $(TOP)/ip_lookup.c $(TOP)/ip_lookup.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h $(TOP)/ip_lookup.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_lookup.c -o $@

$(OBJ)/ip_trafcon_u.o: $(TOP)/ip_trafcon.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h \
	$(TOP)/ip_trafcon.h
	$(CC) $(CCARGS) -c $(TOP)/ip_trafcon.c -o $@

$(OBJ)/ip_auth_u.o: $(TOP)/ip_auth.c $(TOP)/ip_auth.h $(TOP)/ip_compat.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_auth.c -o $@

$(OBJ)/ip_proxy_u.o: $(TOP)/ip_proxy.c $(TOP)/ip_proxy.h $(TOP)/ip_compat.h \
        $(TOP)/ip_ftp_pxy.c $(TOP)/ip_rcmd_pxy.c $(TOP)/ip_raudio_pxy.c \
        $(TOP)/ip_rpcb_pxy.c $(TOP)/ip_ipsec_pxy.c $(TOP)/ip_dns_pxy.c \
	$(TOP)/ip_tftp_pxy.c $(TOP)/ip_pptp_pxy.c $(TOP)/ip_nat.h \
	$(TOP)/ip_fil.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_proxy.c -o $@

$(OBJ)/ip_rules.c: $(OBJ)/ipf.exe $(TOP)/tools/ipfcomp.c $(TOP)/rules/ip_rules
	$(OBJ)/ipf.exe -cc -nf $(TOP)/rules/ip_rules
	-/bin/mv -f ip_rules.c $(OBJ)

$(TOP)/ip_rules.h: $(OBJ)/ip_rules.c
	if [ -f ip_rules.h ] ; then \
		/bin/mv -f ip_rules.h $(TOP) ; \
	fi

$(OBJ)/ip_rules_u.o: $(OBJ)/ip_rules.c $(TOP)/ip_fil.h $(TOP)/ip_rules.h
	$(CC) $(CCARGS) $(EXTRA) -c $(OBJ)/ip_rules.c -o $@

$(OBJ)/ip_fil_u.o: $(TOP)/ip_fil.c $(TOP)/ip_fil.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_fil.c -o $@

$(OBJ)/ip_fil.o: $(TOP)/ip_fil_solaris.c $(TOP)/ip_fil.h
	$(CC) -I$(TOP) $(DFLAGS) $(COMPIPF) -c $(TOP)/ip_fil_solaris.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ip_log_u.o: $(TOP)/ip_log.c $(TOP)/ip_fil.h
	$(CC) $(CCARGS) $(EXTRA) -c $(TOP)/ip_log.c -o $@

$(OBJ)/ip_log.o: $(TOP)/ip_log.c $(TOP)/ip_fil.h
	$(CC) -I$(TOP) $(DFLAGS) -c $(TOP)/ip_log.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/solaris.o: $(TOP)/solaris.c $(TOP)/ipl.h
	$(CC) -I$(TOP) $(DFLAGS) -c $(TOP)/solaris.c -o $@
	$(CTFCONVERT) $@

$(OBJ)/ipmon: $(IPMON) $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(LOGFAC) $(IPMON) -o $@ $(LIBS) -ll -ldl

$(OBJ)/ipmon.o: $(TOOL)/ipmon.c $(TOP)/ipmon.h
	$(CC) $(CCARGS) $(LOGFAC) -c $(TOOL)/ipmon.c -o $@

$(OBJ)/ipmon_y.o: ipmon_y.c $(TOP)/ipmon.h ipmon_y.h ipmon_l.h
	$(CC) $(CCARGS) -c ipmon_y.c -o $@

$(OBJ)/ipmon_l.o: ipmon_l.c $(TOP)/ipmon.h ipmon_y.h ipmon_l.h
	$(CC) $(CCARGS) -I. -I$(TOP)/tools -c ipmon_l.c -o $@

ipmon_y.c $(OBJ)/ipmon_y.h: $(TOOL)/ipmon_y.y $(TOP)/ipmon.h
	(cd $(TOOL); make DEST=../$(HERE) ../$(HERE)/$@)
	mv ipmon_y.c y.tab.c
	sed -e 's/extern [a-z]* .*();//' \
	    -e 's/^\(static [a-z]* .*\)();/\1(void);/' y.tab.c > $@
	/bin/rm -f y.tab.c

ipmon_l.c: $(TOOL)/lexer.c $(TOP)/ipmon.h
	(cd $(TOOL); make DEST=../$(HERE) ../$(HERE)/$@)

ipmon_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make DEST=../$(HERE) ../$(HERE)/$@)

$(OBJ)/ipscan: $(OBJ)/ipscan_y.o $(OBJ)/ipscan_l.o $(OBJ)/libipf.a
	$(CC) $(DEBUG) $(OBJ)/ipscan_y.o $(OBJ)/ipscan_l.o -o $@ -ll $(LIBS)

$(OBJ)/ipscan_y.o: ipscan_y.c ipscan_y.h $(TOP)/ip_scan.h
	$(CC) $(CCARGS) -c ipscan_y.c -o $@

$(OBJ)/ipscan_l.o: ipscan_l.c ipscan_y.h $(TOP)/ip_scan.h ipscan_l.h
	$(CC) $(CCARGS) -I. -I$(TOP)/tools -c ipscan_l.c -o $@

ipscan_y.h ipscan_y.c: $(TOOL)/ipscan_y.y $(TOP)/ip_scan.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ipscan_l.c: $(TOOL)/lexer.c $(TOP)/ip_scan.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ipscan_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

$(OBJ)/ippool: $(IPPOOL) $(OBJ)/libipf.a
	$(CC) $(CCARGS) -I. $(IPPOOL) -o $@ $(LIBS) -ll -lkvm

$(OBJ)/ippool.o: $(TOOL)/ippool.c $(TOP)/ip_pool.h $(TOP)/ip_htable.h \
		 $(TOP)/ip_dstlist.h
	$(CC) $(CCARGS) -c $(TOOL)/ippool.c -o $@

$(OBJ)/ippool_y.o: ippool_y.c ippool_y.h $(TOP)/ip_pool.h ippool_l.h \
		   $(TOP)/ip_htable.h $(TOP)/ip_dstlist.h
	$(CC) $(CCARGS) -c ippool_y.c -o $@

$(OBJ)/ippool_l.o: ippool_l.c ippool_y.h $(TOP)/ip_pool.h ippool_l.h
	$(CC) $(CCARGS) -I. -I$(TOP)/tools -c ippool_l.c -o $@

ippool_y.h ippool_y.c: $(TOOL)/ippool_y.y $(TOP)/ip_pool.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ippool_l.c: $(TOOL)/lexer.c $(TOP)/ip_pool.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

ippool_l.h: $(TOOL)/lexer.h
	(cd $(TOOL); make "DEST=../$(HERE)" ../$(HERE)/$@)

$(OBJ)/iptrafcon.o: $(TOP)/iptrafcon.c
	$(CC) $(CCARGS) -c $(TOP)/iptrafcon.c -o $@

$(OBJ)/iptrafcon: $(IPTRAFCON) $(OBJ)/libipf.a
	$(CC) $(CCARGS) $(IPTRAFCON) -o $@ $(LIBS)

.y.c:

.l.c:

clean:
	${RM} -f $(TOP)/ipf $(TOP)/ipnat $(TOP)/ipmon $(TOP)/ippool
	${RM} -f $(TOP)/ipftest $(TOP)/ipscan $(TOP)/ipsyncm $(TOP)/ipsyncs
	if [ `basename \`pwd\`` != SunOS5 ] ; then \
		${RM} -f core *.o ipfrule ipf ipfstat ipftest ipmon ipnat ; \
		${RM} -f ipf.exe ipfs ipsyncm ipsyncs libipf.a; \
		${RM} -f ipnat.5 ipnat.4 ipnat.8 ipfilter.4 ipfilter.5; \
		${RM} -f ippool.8 ippool.5 ipscan.8 ipscan.5 ipmon.5; \
		${RM} -f prototype pkginfo postinstall postremove copyright; \
		${RM} -f ip_fil.c ipf_l.c ipf_y.c ipf_y.h ipf_l.h; \
		${RM} -f ipscan ipscan_y.c ipscan_y.h ipscan_l.c; \
		${RM} -f ippool ippool_y.c ippool_y.h ippool_l.c; \
		${RM} -f ipscan_l.h ippool_l.h ip_rules.c ip_rules.h; \
		${RM} -f ipnat_y.c ipnat_y.h ipnat_l.c ipnat_l.h; \
		${RM} -f ipmon_y.c ipmon_y.h ipmon_l.c ipmon_l.h; \
		${RM} -f ipsyncm ipsyncs ipfs; \
	fi
	/bin/rm -rf */root ipf.pkg* $(OBJ32) $(OBJ64)
	make -f Makefile.ipsend clean "TOP=${TOP}"
	-(for i in *; do \
		if [ -d $${i} -a -h $${i}/Makefile ] ; then \
			(cd $${i}; make TOP=`cd ..; cd $(TOP); pwd` clean); \
			rm $${i}/Makefile $${i}/Makefile.ipsend; \
			rmdir $${i}; \
		fi \
	 done)

$(CPUDIR)/ipf.pkg: $(CPUDIR)/ipf.pkg.common
	if [ -d $(CPUDIR)/$(OBJ32) ]; then \
		$(MAKE) PROTO=$(PROTO32) PKG=ipf OBJ=$(OBJ32) \
		 BITS=32 TOP=$(TOP) ipf.pkg.bin; \
		$(MAKE) PROTO=$(PROTO64) PKG=ipfx OBJ=$(OBJ64) \
		 BITS=64 TOP=$(TOP) ipf.pkg.bin; \
	else \
		($(MAKE) PROTO=prototype OBJ=. BITS=${BITS} \
		 TOP=$(TOP) ipf.pkg.bin) \
	fi
	touch $(CPUDIR)/ipf.pkg
	pkgtrans -s $(TMP) $(CPUDIR)/ipf.pkg `ls $(TMP)`
	/bin/rm -f ipf.pkg
	rm -rf $(TMP)/ipf*

$(CPUDIR)/ipf.pkg.common:
	mkdir -p $(ROOTINC)/ipfilter $(PKGDIR)/examples $(ROOT)/usr/kernel/drv
	mkdir -p $(ROOT)/etc/init.d $(TMP)
	mkdir -p $(PKGMAN)/man1 $(PKGMAN)/man4 $(PKGMAN)/man5 $(PKGMAN)/man8
	-$(INSTALL) -c ipfboot $(ROOT)/etc/init.d
	-cp $(TOP)/man/*.[0-9] .
	-$(INSTALL) -c ipf.conf $(ROOT)/usr/kernel/drv
	-$(INSTALL) ipf.8 $(PKGMAN)/man8
	-$(INSTALL) ipfs.8 $(PKGMAN)/man8
	-$(INSTALL) ipnat.8 $(PKGMAN)/man8
	-$(INSTALL) ipftest.1 $(PKGMAN)/man1
	-$(INSTALL) mkfilters.1 $(PKGMAN)/man1
	-$(INSTALL) ipf.4 $(PKGMAN)/man4
	-$(INSTALL) ipfilter.4 $(PKGMAN)/man4
	-$(INSTALL) ipnat.4 $(PKGMAN)/man4
	-$(INSTALL) ipl.4 $(PKGMAN)/man4
	-$(INSTALL) ipf.5 $(PKGMAN)/man5
	-$(INSTALL) ipnat.5 $(PKGMAN)/man5
	-$(INSTALL) ippool.5 $(PKGMAN)/man5
	-$(INSTALL) ipfilter.5 $(PKGMAN)/man5
	-$(INSTALL) ipfstat.8 $(PKGMAN)/man8
	-$(INSTALL) ipmon.8 $(PKGMAN)/man8
	-$(INSTALL) ippool.8 $(PKGMAN)/man8
	-cp $(TOP)/rules/* $(PKGDIR)/examples
	-cp $(TOP)/ip_fil.h $(TOP)/ip_compat.h $(TOP)/ip_state.h .
	-cp $(TOP)/ip_nat.h $(TOP)/ip_frag.h $(TOP)/ip_proxy.h .
	-cp $(TOP)/ip_auth.h $(TOP)/ip_htable.h $(TOP)/ip_pool.h .
	-cp $(TOP)/ip_lookup.h $(TOP)/ip_dstlist.h .
	-$(INSTALL) ip_fil.h $(ROOTINC)/ipfilter
	-$(INSTALL) ip_compat.h $(ROOTINC)/ipfilter
	-$(INSTALL) ip_state.h $(ROOTINC)/ipfilter
	-$(INSTALL) ip_nat.h $(ROOTINC)/ipfilter
	-$(INSTALL) ip_frag.h $(ROOTINC)/ipfilter
	-$(INSTALL) ip_htable.h $(ROOTINC)/ipfilter
	-$(INSTALL) ip_dstlist.h $(ROOTINC)/ipfilter
	-$(INSTALL) ip_lookup.h $(ROOTINC)/ipfilter
	-$(INSTALL) ip_pool.h $(ROOTINC)/ipfilter
	-$(INSTALL) ip_proxy.h $(ROOTINC)/ipfilter
	-$(INSTALL) ip_auth.h $(ROOTINC)/ipfilter
	touch $@

#
# For 64 bit Solaris, we build 32 and 64 bit packages, but the 64 bit package
# is smaller and only contains the 64 bit specific and not the common stuff.
#
ipf.pkg.bin:
	mkdir -p $(PKGBIN) $(ROOT)/$(SBINDEST) $(ROOT)/usr/kernel/drv/$(OBJ)
	-$(INSTALL) -c -s $(OBJDIR)/ipftest $(PKGBIN)/ipftest
	-$(INSTALL) -c -s $(OBJDIR)/ipmon $(PKGBIN)/ipmon
	-$(INSTALL) -c -s $(OBJDIR)/ipsend $(PKGBIN)/ipsend
	-$(INSTALL) -c -s $(OBJDIR)/ipresend $(PKGBIN)/ipresend
	-$(INSTALL) -c $(TOP)/mkfilters $(PKGBIN)/mkfilters
	-$(INSTALL) -c $(OBJDIR)/ipf $(ROOT)/usr/kernel/drv/$(OBJ)/ipf
	-$(INSTALL) -c -s $(OBJDIR)/ipnat $(ROOT)/$(SBINDEST)/ipnat
	-$(INSTALL) -c -s $(OBJDIR)/ippool $(ROOT)/$(SBINDEST)/ippool
	-$(INSTALL) -c -s $(OBJDIR)/ipf.exe $(ROOT)/$(SBINDEST)/ipf
	-$(INSTALL) -c -s $(OBJDIR)/ipfs $(ROOT)/$(SBINDEST)/ipfs
	-$(INSTALL) -c -s $(OBJDIR)/ipfstat $(ROOT)/$(SBINDEST)/ipfstat
#	-(cd $(CPUDIR); rm -f copyright post* prototype pkginfo)
	-(cd $(CPUDIR); rm -f prototype)
#	-(cd $(CPUDIR); ln -s ../copyright ../postinstall ../postremove .)
	-(cd $(CPUDIR); ln -s ../$(PROTO) prototype)
	-sed -e 's/ipf/$(PKG)/' -e "s/NAME=.*/&$(BITTYPE)/" \
	     -e "s/ARCH=.*/ARCH=`uname -p` (${BITS}-bit)/" \
		< pkginfo > $(CPUDIR)/pkginfo
	-if [ "$(BITS)" = 64 ]; then echo 'SUNW_ISA=$(OBJ64)' >> $(CPUDIR)/pkginfo; fi
	(cd $(CPUDIR); pkgmk -a `uname -p` -o -d $(TMP))


package install: $(CPUDIR)/ipf.pkg
	/bin/rm -f ipf.pkg
	ln -s $(CPUDIR)/ipf.pkg ipf.pkg
	@if [ `id|sed -e 's/^.[^(]*(\([^)]*\)).*/\1/'` != root ] ; then \
		echo "Can only install if root"; \
		exit 1; \
	fi
# Must add ipfx before ipf or driver attach fails.
	-[ -d $(CPUDIR)/$(OBJ64) ] && pkgadd -d ipf.pkg ipfx
	-pkgadd -d ipf.pkg ipf
