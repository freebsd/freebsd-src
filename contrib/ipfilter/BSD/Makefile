#
# Copyright (C) 1993-1998 by Darren Reed.
#
# Redistribution and use in source and binary forms are permitted
# provided that this notice is preserved and due credit is given
# to the original author and the contributors.
#
BINDEST=/usr/sbin
SBINDEST=/sbin
SEARCHDIRS=$(BINDEST) $(SBINDEST) /bin /usr/bin /sbin /usr/sbin \
	/usr/local/bin /usr/local/sbin
MANDIR=/usr/share/man
CC=cc -Wall -Wstrict-prototypes -Wuninitialized -O
CFLAGS=-g -I$(TOP)
#
# For NetBSD/FreeBSD
#
DEVFS!=/usr/bin/lsvfs 2>&1 | sed -n 's/.*devfs.*/-DDEVFS/p'
CPU!=uname -m
INC=-I/usr/include -I/sys -I/sys/sys -I/sys/arch
DEF=-D$(CPU) -D__$(CPU)__ -DINET -DKERNEL -D_KERNEL $(INC) $(DEVFS)
IPDEF=$(DEF) -DGATEWAY -DDIRECTED_BROADCAST
VNODESHDIR=/sys/kern
MLD=$(ML) vnode_if.h
ML=mln_ipl.c
IPFILC=ip_fil.c
LKM=if_ipl.o
DLKM=
MFLAGS="BINDEST=$(BINDEST)" "SBINDEST=$(SBINDEST)" "MANDIR=$(MANDIR)" \
	'CFLAGS=$(CFLAGS) $(SOLARIS2)' "IPFLKM=$(IPFLKM)" \
	"IPFLOG=$(IPFLOG)" "LOGFAC=$(LOGFAC)" "POLICY=$(POLICY)" \
	"SOLARIS2=$(SOLARIS2)" "DEBUG=$(DEBUG)" "DCPU=$(CPU)" \
	"CPUDIR=$(CPUDIR)"
#
########## ########## ########## ########## ########## ########## ##########
#
CP=/bin/cp
RM=/bin/rm
CHMOD=/bin/chmod
INSTALL=install
#
MODOBJS=ip_fil.o fil_k.o ml_ipl.o ip_nat.o ip_frag.o ip_state.o ip_proxy.o \
	ip_auth.o ip_log.o
DFLAGS=$(IPFLKM) $(DEF) $(DLKM)
IPF=ipf.o parse.o common.o opt.o facpri.o
IPT=ipt.o parse.o common.o fil.o ipft_sn.o ipft_ef.o ipft_td.o ipft_pc.o \
    opt.o ipft_tx.o misc.o ip_frag_u.o ip_state_u.o ip_nat_u.o ip_proxy_u.o \
    ip_auth_u.o ipft_hx.o ip_fil_u.o ip_log_u.o natparse.o facpri.o \
    printnat.o printstate.o
IPNAT=ipnat.o kmem.o natparse.o common.o printnat.o
FILS=fils.o parse.o kmem.o opt.o facpri.o common.o printstate.o

build all: ipf ipfs ipfstat ipftest ipmon ipnat $(LKM)
	/bin/rm -f $(TOP)/ipf
	ln -s `pwd`/ipf $(TOP)
	/bin/rm -f $(TOP)/ipftest
	ln -s `pwd`/ipftest $(TOP)
	/bin/rm -f $(TOP)/ipmon
	ln -s `pwd`/ipmon $(TOP)
	/bin/rm -f $(TOP)/ipnat
	ln -s `pwd`/ipnat $(TOP)

ipfstat: $(FILS)
	$(CC) -static $(DEBUG) $(CFLAGS) $(STATETOP_CFLAGS) $(STATETOP_INC) \
		$(FILS) -o $@ $(LIBS) $(STATETOP_LIB) -lkvm

ipf: $(IPF)
	$(CC) -static $(DEBUG) $(CFLAGS) $(IPF) -o $@ $(LIBS)

ipftest: $(IPT)
	$(CC) $(DEBUG) $(CFLAGS) $(IPT) -o $@ $(LIBS)

ipnat: $(IPNAT)
	$(CC) -static $(DEBUG) $(CFLAGS) $(IPNAT) -o $@ $(LIBS) -lkvm

ipfs: ipfs.o
	$(CC) -static $(DEBUG) $(CFLAGS) ipfs.o -o $@ $(LIBS)

tests:
	(cd test; make )

fils.o: $(TOP)/fils.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_frag.h \
	$(TOP)/ip_compat.h $(TOP)/ip_state.h $(TOP)/ip_nat.h
	$(CC) $(DEBUG) $(CFLAGS) $(STATETOP_CFLAGS) $(STATETOP_INC) \
	-c $(TOP)/fils.c -o $@

ipfs.o: $(TOP)/ipfs.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_state.h \
	$(TOP)/ip_nat.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/ipfs.c -o $@

fil.o: $(TOP)/fil.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_compat.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/fil.c -o $@

fil_k.o: $(TOP)/fil.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_compat.h \
	 $(TOP)/ipl.h
	$(CC) $(DEBUG) $(CFLAGS) $(POLICY) $(DFLAGS) -c $(TOP)/fil.c -o $@

ipf.o: $(TOP)/ipf.c $(TOP)/ip_fil.h $(TOP)/ipf.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/ipf.c -o $@

ipt.o: $(TOP)/ipt.c $(TOP)/ip_fil.h $(TOP)/ipt.h $(TOP)/ipf.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/ipt.c -o $@

misc.o: $(TOP)/misc.c $(TOP)/ip_fil.h $(TOP)/ipt.h $(TOP)/ipf.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/misc.c -o $@

opt.o: $(TOP)/opt.c $(TOP)/ip_fil.h $(TOP)/ipf.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/opt.c -o $@

ipnat.o: $(TOP)/ipnat.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_nat.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/ipnat.c -o $@

natparse.o: $(TOP)/natparse.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_nat.h \
	$(TOP)/ip_compat.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/natparse.c -o $@

printnat.o: $(TOP)/printnat.c $(TOP)/ip_fil.h $(TOP)/ipf.h $(TOP)/ip_nat.h \
	$(TOP)/ip_compat.h $(TOP)/ip_proxy.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/printnat.c -o $@

printstate.o: $(TOP)/printstate.c $(TOP)/ip_fil.h $(TOP)/ipf.h \
	$(TOP)/ip_state.h $(TOP)/ip_compat.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/printstate.c -o $@

ipft_sn.o: $(TOP)/ipft_sn.c $(TOP)/ipt.h $(TOP)/ipf.h $(TOP)/ip_fil.h \
	$(TOP)/snoop.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/ipft_sn.c -o $@

ipft_ef.o: $(TOP)/ipft_ef.c $(TOP)/ipf.h $(TOP)/ip_fil.h $(TOP)/ipt.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/ipft_ef.c -o $@

ipft_td.o: $(TOP)/ipft_td.c $(TOP)/ipf.h $(TOP)/ip_fil.h $(TOP)/ipt.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/ipft_td.c -o $@

ipft_pc.o: $(TOP)/ipft_pc.c $(TOP)/ipf.h $(TOP)/ip_fil.h $(TOP)/ipt.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/ipft_pc.c -o $@

ipft_tx.o: $(TOP)/ipft_tx.c $(TOP)/ipf.h $(TOP)/ip_fil.h $(TOP)/ipt.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/ipft_tx.c -o $@

ipft_hx.o: $(TOP)/ipft_hx.c $(TOP)/ipf.h $(TOP)/ip_fil.h $(TOP)/ipt.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/ipft_hx.c -o $@

ip_nat_u.o: $(TOP)/ip_nat.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/ip_nat.c -o $@

ip_proxy_u.o: $(TOP)/ip_proxy.c $(TOP)/ip_proxy.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h $(TOP)/ip_ftp_pxy.c $(TOP)/ip_rcmd_pxy.c \
	$(TOP)/ip_raudio_pxy.c $(TOP)/ip_ipsec_pxy.c $(TOP)/ip_nat.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/ip_proxy.c -o $@

ip_frag_u.o: $(TOP)/ip_frag.c $(TOP)/ip_frag.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/ip_frag.c -o $@

ip_state_u.o: $(TOP)/ip_state.c $(TOP)/ip_state.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h $(TOP)/ip_nat.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/ip_state.c -o $@

ip_auth_u.o: $(TOP)/ip_auth.c $(TOP)/ip_auth.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/ip_auth.c -o $@

ip_fil_u.o: $(TOP)/$(IPFILC) $(TOP)/ip_fil.h $(TOP)/ip_compat.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/$(IPFILC) -o $@

ip_log_u.o: $(TOP)/ip_log.c $(TOP)/ip_fil.h $(TOP)/ip_compat.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/ip_log.c -o $@

if_ipl.o: $(MODOBJS) 
	ld -r $(MODOBJS) -o $(LKM)
	${RM} -f if_ipl

ipf.ko ipl.ko: $(MODOBJS) 
	gensetdefs $(MODOBJS)
	$(CC) $(DEBUG) $(CFLAGS) -c setdef0.c
	$(CC) $(DEBUG) $(CFLAGS) -c setdef1.c
	ld -Bshareable -o $(LKM) setdef0.o $(MODOBJS) setdef1.o

ip_nat.o: $(TOP)/ip_nat.c $(TOP)/ip_nat.h $(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(DEBUG) $(CFLAGS) $(DFLAGS) -c $(TOP)/ip_nat.c -o $@

ip_frag.o: $(TOP)/ip_frag.c $(TOP)/ip_frag.h $(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(DEBUG) $(CFLAGS) $(DFLAGS) -c $(TOP)/ip_frag.c -o $@

ip_state.o: $(TOP)/ip_state.c $(TOP)/ip_state.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h $(TOP)/ip_nat.h
	$(CC) $(DEBUG) $(CFLAGS) $(DFLAGS) -c $(TOP)/ip_state.c -o $@

ip_proxy.o: $(TOP)/ip_proxy.c $(TOP)/ip_proxy.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h $(TOP)/ip_ftp_pxy.c $(TOP)/ip_raudio_pxy.c \
	$(TOP)/ip_rcmd_pxy.c $(TOP)/ip_ipsec_pxy.c $(TOP)/ip_nat.h
	$(CC) $(DEBUG) $(CFLAGS) $(DFLAGS) -c $(TOP)/ip_proxy.c -o $@

ip_auth.o: $(TOP)/ip_auth.c $(TOP)/ip_auth.h $(TOP)/ip_compat.h \
	$(TOP)/ip_fil.h
	$(CC) $(DEBUG) $(CFLAGS) $(DFLAGS) -c $(TOP)/ip_auth.c -o $@

ip_fil.o: $(TOP)/$(IPFILC) $(TOP)/ip_fil.h $(TOP)/ip_compat.h $(TOP)/ip_nat.h
	$(CC) $(DEBUG) $(CFLAGS) $(DFLAGS) -c $(TOP)/$(IPFILC) -o $@

ip_log.o: $(TOP)/ip_log.c $(TOP)/ip_compat.h $(TOP)/ip_fil.h
	$(CC) $(DEBUG) $(CFLAGS) $(DFLAGS) -c $(TOP)/ip_log.c -o $@

vnode_if.h: $(VNODESHDIR)/vnode_if.sh $(VNODESHDIR)/vnode_if.src
	mkdir -p ../sys
	sh $(VNODESHDIR)/vnode_if.sh $(VNODESHDIR)/vnode_if.src
	if [ -f ../sys/vnode_if.h ] ; then mv ../sys/vnode_if.h .; fi
	rmdir ../sys

ml_ipl.o: $(TOP)/$(MLD) $(TOP)/ipl.h
	-/bin/rm -f vnode_if.c
	$(CC) -I. $(CFLAGS) $(DFLAGS) -c $(TOP)/$(ML) -o $@

kmem.o: $(TOP)/kmem.c
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/kmem.c -o $@

parse.o: $(TOP)/parse.c $(TOP)/ip_fil.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/parse.c -o $@

common.o: $(TOP)/common.c $(TOP)/ip_fil.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/common.c -o $@

facpri.o: $(TOP)/facpri.c $(TOP)/facpri.h
	$(CC) $(DEBUG) $(CFLAGS) -c $(TOP)/facpri.c -o $@

ipmon: $(TOP)/ipmon.c
	$(CC) $(DEBUG) $(CFLAGS) $(LOGFAC) $(TOP)/ipmon.c -o $@ $(LIBS)

clean:
	${RM} -f *.core *.o ipt fils ipf ipfstat ipftest ipmon if_ipl ipnat \
	vnode_if.h $(LKM) ioconf.h *.ko setdef1.c setdef0.c setdefs.h \
	y.tab.? lex.yy.c ipfs
	${RM} -f ../opt_inet6.h ../ipftest ../ipmon ../ipf ../ipnat

	${MAKE} -f Makefile.ipsend ${MFLAGS} clean
	-(for i in *; do \
		if [ -d $${i} -a -f $${i}/Makefile ] ; then \
			cd $${i}; (make clean); cd ..; \
			rm $${i}/Makefile $${i}/Makefile.ipsend; \
			rmdir $${i}; \
		fi \
	done)

install:
	for i in ip_compat.h ip_fil.h ip_nat.h ip_state.h ip_proxy.h \
		ip_frag.h ip_auth.h; do \
		/bin/cp $(TOP)/$$i /usr/include/netinet/; \
		-$(CHMOD) 444 /usr/include/netinet/$$i; \
	done
	-if [ -d /lkm -a -f if_ipl.o ] ; then \
		cp if_ipl.o /lkm; \
	fi
	-if [ -d /modules -a -f ipf.ko ] ; then \
		cp ipf.ko /modules; \
	fi
	@for i in ipf:$(SBINDEST) ipfs:$(SBINDEST) ipnat:$(SBINDEST) \
	    ipfstat:$(SBINDEST) ipftest:$(SBINDEST) ipmon:$(BINDEST); do \
		def="`expr $$i : '[^:]*:\(.*\)'`"; \
		p="`expr $$i : '\([^:]*\):.*'`"; \
		for d in $(SEARCHDIRS); do \
			if [ -f $$d/$$p ] ; then \
				echo "$(INSTALL) -cs -g wheel -m 755 -o root $$p $$d"; \
				$(INSTALL) -cs -g wheel -m 755 -o root $$p $$d; \
				dd=$$d; \
			fi; \
		done; \
		if [ -z "$$dd" ] ; then \
			echo $(INSTALL) -cs -g wheel -m 755 -o root $$p $$def; \
			$(INSTALL) -cs -g wheel -m 755 -o root $$p $$def; \
		fi \
	done
	(cd $(TOP)/man; make INSTALL=$(INSTALL) MANDIR=$(MANDIR) install; cd $(TOP))
