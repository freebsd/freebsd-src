#
# Copyright (C) 1993-2001 by Darren Reed.
#
# See the IPFILTER.LICENCE file for details on licencing.
#
# $Id: Makefile,v 2.11.2.13 2002/03/06 09:43:15 darrenr Exp $
#
BINDEST=/usr/local/bin
SBINDEST=/sbin
MANDIR=/usr/local/man
#To test prototyping
CC=gcc -Wstrict-prototypes -Wmissing-prototypes
#CC=gcc
#CC=cc -Dconst=
DEBUG=-g
TOP=../..
CFLAGS=-I$$(TOP)
CPU=`uname -m`
CPUDIR=`uname -s|sed -e 's@/@@g'`-`uname -r`-`uname -m`
IPFILKERN=`/bin/ls -1tr /usr/src/sys/compile | grep -v .bak | tail -1`
#
# To enable this to work as a Loadable Kernel Module...
#
IPFLKM=-DIPFILTER_LKM
#
# To enable logging of blocked/passed packets...
#
IPFLOG=-DIPFILTER_LOG
#
# The facility you wish to log messages from ipmon to syslogd with.
#
LOGFAC=-DLOGFAC=LOG_LOCAL0

#
# Uncomment the next 3 lines if you want to view the state table a la top(1)
# (requires that you have installed ncurses).
STATETOP_CFLAGS=-DSTATETOP
#
# Where to find the ncurses include files (if not in default path), 
#
#STATETOP_INC=
#STATETOP_INC=-I/usr/local/include
#
# How to link the ncurses library
#
STATETOP_LIB=-lcurses
#STATETOP_LIB=-L/usr/local/lib -lncurses

#
# Uncomment this when building IPv6 capability.
#
#INET6=-DUSE_INET6
#
# For packets which don't match any pass rules or any block rules, set either
# FR_PASS or FR_BLOCK (respectively).  It defaults to FR_PASS if left
# undefined.  This is ignored for ipftest, which can thus return three
# results: pass, block and nomatch.  This is the sort of "block unless
# explicitly allowed" type #define switch.
#
POLICY=-DIPF_DEFAULT_PASS=FR_PASS
#
MFLAGS1='CFLAGS=$(CFLAGS) $(ARCHINC) $(SOLARIS2) $(INET6) $(IPFLOG)' \
	"IPFLOG=$(IPFLOG)" "LOGFAC=$(LOGFAC)" "POLICY=$(POLICY)" \
	"SOLARIS2=$(SOLARIS2)" "DEBUG=$(DEBUG)" "DCPU=$(CPU)" \
	"CPUDIR=$(CPUDIR)" 'STATETOP_CFLAGS=$(STATETOP_CFLAGS)' \
        'STATETOP_INC=$(STATETOP_INC)' 'STATETOP_LIB=$(STATETOP_LIB)' \
	"BITS=$(BITS)" "OBJ=$(OBJ)"
DEST="BINDEST=$(BINDEST)" "SBINDEST=$(SBINDEST)" "MANDIR=$(MANDIR)"
MFLAGS=$(MFLAGS1) "IPFLKM=$(IPFLKM)"
#
SHELL=/bin/sh
#
########## ########## ########## ########## ########## ########## ##########
#
CP=/bin/cp
RM=/bin/rm
CHMOD=/bin/chmod
INSTALL=install
#

all:
	@echo "Chose one of the following targets for making IP filter:"
	@echo ""
	@echo "solaris	- auto-selects SunOS4.1.x/Solaris 2.3-6/Solaris2.4-6x86"
	@echo "netbsd	- compile for NetBSD"
	@echo "openbsd	- compile for OpenBSD"
	@echo "freebsd	- compile for FreeBSD 2.0, 2.1 or earlier"
	@echo "freebsd22	- compile for FreeBSD-2.2 or greater"
	@echo "freebsd3	- compile for FreeBSD-3.x"
	@echo "freebsd4	- compile for FreeBSD-4.x"
	@echo "bsd	- compile for generic 4.4BSD systems"
	@echo "bsdi	- compile for BSD/OS"
	@echo "irix	- compile for SGI IRIX"
	@echo "linux	- compile for Linux 2.0.31+"
	@echo ""

tests:
	@if [ -d test ]; then (cd test; make) \
	else echo test directory not present, sorry; fi

include:
	if [ ! -f netinet/done ] ; then \
		(cd netinet; ln -s ../*.h .; ln -s ../ip_*_pxy.c .; ); \
		(cd netinet; ln -s ../ipsend/tcpip.h tcpip.h); \
		touch netinet/done; \
	fi

sunos solaris: include
	CC="$(CC)" ./buildsunos

freebsd22: include
	make setup "TARGOS=BSD" "CPUDIR=$(CPUDIR)"
	-rm -f BSD/$(CPUDIR)/ioconf.h
	@if [ -n $(IPFILKERN) ] ; then \
		if [ -f /sys/compile/$(IPFILKERN)/ioconf.h ] ; then \
		ln -s /sys/compile/$(IPFILKERN)/ioconf.h BSD/$(CPUDIR); \
		else \
		ln -s /sys/$(IPFILKERN)/ioconf.h BSD/$(CPUDIR); \
		fi \
	elif [ ! -f `uname -v|sed -e 's@^.*:\(/[^: ]*\).*@\1@'`/ioconf.h ] ; then \
		echo -n "Can't find ioconf.h in "; \
		echo `uname -v|sed -e 's@^.*:\(/[^: ]*\).*@\1@'`; \
		exit 1;\
	else \
		ln -s `uname -v|sed -e 's@^.*:\(/[^: ]*\).*@\1@'`/ioconf.h BSD/$(CPU) ; \
	fi
	make freebsd

freebsd4: include
	if [ x$INET6 = x ] ; then \
		echo "#undef INET6" > opt_inet6.h; \
	else \
		echo "#define INET6" > opt_inet6.h; \
	fi
	make setup "TARGOS=BSD" "CPUDIR=$(CPUDIR)"
	(cd BSD/$(CPUDIR); make build TOP=../.. $(MFLAGS) "ML=mlfk_ipl.c" "MLD=mlfk_ipl.c" "LKM=ipf.ko" "DLKM=-DKLD_MODULE -I/sys"; cd ..)
	(cd BSD/$(CPUDIR); make -f Makefile.ipsend TOP=../.. $(MFLAGS1); cd ..)

freebsd3 freebsd30: include
	make setup "TARGOS=BSD" "CPUDIR=$(CPUDIR)"
	(cd BSD/$(CPUDIR); make build TOP=../.. $(MFLAGS1) "ML=mlf_ipl.c" LKM= ; cd ..)
	(cd BSD/$(CPUDIR); make -f Makefile.ipsend TOP=../.. $(MFLAGS1); cd ..)

netbsd: include
	make setup "TARGOS=BSD" "CPUDIR=$(CPUDIR)"
	(cd BSD/$(CPUDIR); make build TOP=../.. $(MFLAGS) 'DLKM=-D_LKM' "ML=mln_ipl.c"; cd ..)
	(cd BSD/$(CPUDIR); make -f Makefile.ipsend TOP=../.. $(MFLAGS); cd ..)

openbsd openbsd21: include
	make setup "TARGOS=BSD" "CPUDIR=$(CPUDIR)"
	(cd BSD/$(CPUDIR); make build TOP=../.. $(MFLAGS) 'DLKM=-D_LKM' "ML=mln_ipl.c"; cd ..)
	(cd BSD/$(CPUDIR); make -f Makefile.ipsend TOP=../.. $(MFLAGS); cd ..)

freebsd freebsd20 freebsd21: include
	make setup "TARGOS=BSD" "CPUDIR=$(CPUDIR)"
	(cd BSD/$(CPUDIR); make build TOP=../.. $(MFLAGS) "ML=mlf_ipl.c"; cd ..)
	(cd BSD/$(CPUDIR); make -f Makefile.ipsend TOP=../.. $(MFLAGS); cd ..)

bsd: include
	make setup "TARGOS=BSD" "CPUDIR=$(CPUDIR)"
	(cd BSD/$(CPUDIR); make build TOP=../.. $(MFLAGS); cd ..)
	(cd BSD/$(CPUDIR); make -f Makefile.ipsend TOP=../.. $(MFLAGS); cd ..)

bsdi bsdos: include
	make setup "TARGOS=BSD" "CPUDIR=$(CPUDIR)"
	(cd BSD/$(CPUDIR); make build "CC=$(CC)" TOP=../.. $(MFLAGS) LKM= ; cd ..)
	(cd BSD/$(CPUDIR); make -f Makefile.ipsend "CC=$(CC)" TOP=../.. $(MFLAGS); cd ..)

irix IRIX: include
	make setup "TARGOS=IRIX" "CPUDIR=$(CPUDIR)"
	-(cd IRIX/$(CPUDIR); if [ $(MAKE) = make ] ; then make -f Makefile.std build TOP=../.. $(DEST) SGI=`../getrev` $(MFLAGS); else smake build SGI=`../getrev` TOP=../.. $(DEST) $(MFLAGS); fi;)
	-(cd IRIX/$(CPUDIR); if [ $(MAKE) = make ] ; then make -f Makefile.ipsend.std SGI=`../getrev` TOP=../.. $(DEST) $(MFLAGS); else smake -f Makefile.ipsend SGI=`../getrev` TOP=../.. $(DEST) $(MFLAGS); fi)

linux: include
	make setup "TARGOS=Linux" "CPUDIR=$(CPUDIR)"
	./buildlinux

linuxrev:
	(cd Linux/$(CPUDIR); make build TOP=../.. $(DEST) $(MFLAGS) LKM= ; cd ..)
	(cd Linux/$(CPUDIR); make -f Makefile.ipsend TOP=../.. $(DEST) $(MFLAGS); cd ..)

setup:
	-if [ ! -d $(TARGOS)/$(CPUDIR) ] ; then mkdir $(TARGOS)/$(CPUDIR); fi
	-rm -f $(TARGOS)/$(CPUDIR)/Makefile $(TARGOS)/$(CPUDIR)/Makefile.ipsend
	-ln -s ../Makefile $(TARGOS)/$(CPUDIR)/Makefile
	-if [ ! -f $(TARGOS)/$(CPUDIR)/Makefile.std -a \
		-f $(TARGOS)/Makefile.std ] ; then \
	    ln -s ../Makefile.std $(TARGOS)/$(CPUDIR)/Makefile.std; \
	 fi
	-if [ ! -f $(TARGOS)/$(CPUDIR)/Makefile.ipsend.std -a \
		-f $(TARGOS)/Makefile.ipsend.std ] ; then \
	    ln -s ../Makefile.ipsend.std $(TARGOS)/$(CPUDIR)/Makefile.ipsend.std; \
	 fi
	-ln -s ../Makefile.ipsend $(TARGOS)/$(CPUDIR)/Makefile.ipsend

clean: clean-include
	${RM} -f core *.o ipt fils ipf ipfstat ipftest ipmon if_ipl \
	vnode_if.h $(LKM) *~
	${RM} -rf sparcv7 sparcv9
	(cd SunOS4; make clean)
	(cd SunOS5; make clean)
	(cd BSD; make clean)
	(cd Linux; make clean)
	if [ "`uname -s`" = "IRIX" ]; then (cd IRIX; make clean); fi
	[ -d test ] && (cd test; make clean)
	(cd ipsend; make clean)

clean-include:
	sh -c 'cd netinet; for i in *; do if [ -h $$i ] ; then /bin/rm -f $$i; fi; done'
	${RM} -f netinet/done

clean-bsd: clean-include
	(cd BSD; make clean)

clean-sunos4: clean-include
	(cd SunOS4; make clean)

clean-sunos5: clean-include
	(cd SunOS5; make clean)

clean-irix: clean-include
	(cd IRIX; make clean)

clean-linux: clean-include
	(cd Linux; make clean)

get:
	-@for i in ipf.c ipt.h solaris.c ipf.h kmem.c ipft_ef.c linux.h \
		ipft_pc.c fil.c ipft_sn.c mln_ipl.c fils.c ipft_td.c \
		mls_ipl.c ip_compat.h ipl.h opt.c ip_fil.c ipl_ldev.c \
		parse.c ip_fil.h ipmon.c pcap.h ip_sfil.c ipt.c snoop.h \
		ip_state.c ip_state.h ip_nat.c ip_nat.h ip_frag.c \
		ip_frag.h ip_sfil.c misc.c; do \
		if [ ! -f $$i ] ; then \
			echo "getting $$i"; \
			sccs get $$i; \
		fi \
	done

sunos4 solaris1:
	(cd SunOS4; make build TOP=.. "CC=$(CC)" $(DEST) $(MFLAGS); cd ..)
	(cd SunOS4; make -f Makefile.ipsend "CC=$(CC)" TOP=.. $(DEST) $(MFLAGS); cd ..)

sunos5 solaris2:
	(cd SunOS5/$(CPUDIR); make build TOP=../.. "CC=$(CC)" $(DEST) $(MFLAGS) "SOLARIS2=$(SOLARIS2)" "CPU=-Dsparc -D__sparc__"; cd ..)
	(cd SunOS5/$(CPUDIR); make -f Makefile.ipsend TOP=../.. "CC=$(CC)" $(DEST) $(MFLAGS); cd ..)

sunos5x86 solaris2x86:
	(cd SunOS5/$(CPUDIR); make build TOP=../.. "CC=$(CC)" $(DEST) $(MFLAGS) "SOLARIS2=$(SOLARIS2)" "CPU=-Di86pc -Di386 -D__i386__"; cd ..)
	(cd SunOS5/$(CPUDIR); make -f Makefile.ipsend TOP=../.. "CC=$(CC)" $(DEST) $(MFLAGS); cd ..)

install-linux:
	(cd Linux/$(CPUDIR); make install "TOP=../.." $(DEST) $(MFLAGS); cd ..)
	(cd Linux/$(CPUDIR); make -f Makefile.ipsend INSTALL=$(INSTALL) install "TOP=../.." $(DEST) $(MFLAGS); cd ..)

install-bsd:
	(cd BSD/$(CPUDIR); make install "TOP=../.." $(MFLAGS); cd ..)
	(cd BSD/$(CPUDIR); make -f Makefile.ipsend INSTALL=$(INSTALL) install "TOP=../.." $(MFLAGS); cd ..)

install-sunos4: solaris
	(cd SunOS4; $(MAKE) "CPU=$(CPU)" "TOP=.." install)

install-sunos5: solaris
	(cd SunOS5; $(MAKE) "CPUDIR=`uname -p`-`uname -r`" "CPU=$(CPU) TOP=.." install)

install-irix: irix
	(cd IRIX; smake install "CPU=$(CPU) TOP=.." $(DEST) $(MFLAGS))

rcsget:
	-@for i in ipf.c ipt.h solaris.c ipf.h kmem.c ipft_ef.c linux.h \
		ipft_pc.c fil.c ipft_sn.c mln_ipl.c fils.c ipft_td.c \
		mls_ipl.c ip_compat.h ipl.h opt.c ip_fil.c ipl_ldev.c \
		parse.c ip_fil.h ipmon.c pcap.h ip_sfil.c ipt.c snoop.h \
		ip_state.c ip_state.h ip_nat.c ip_nat.h ip_frag.c \
		ip_frag.h ip_sfil.c misc.c; do \
		if [ ! -f $$i ] ; then \
			echo "getting $$i"; \
			co $$i; \
		fi \
	done

do-cvs:
	find . -type d -name CVS -print | xargs /bin/rm -rf
	find . -type f -name .cvsignore -print | xargs /bin/rm -f
