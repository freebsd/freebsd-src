#
# Copyright (C) 1993-1997 by Darren Reed.
#
# Redistribution and use in source and binary forms are permitted
# provided that this notice is preserved and due credit is given
# to the original author and the contributors.
#
# $Id: Makefile,v 2.0.2.26.2.10 1998/05/23 05:01:23 darrenr Exp $
#
BINDEST=/usr/local/bin
SBINDEST=/sbin
MANDIR=/usr/local/man
#To test prototyping
#CC=gcc -Wstrict-prototypes -Wmissing-prototypes -Werror
CC=gcc
#CC=cc -Dconst=
DEBUG=-g
CFLAGS=-I$$(TOP)
CPU=`uname -m`
CPUDIR=`uname -s|sed -e 's@/@@g'`-`uname -r`-`uname -m`
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
# For packets which don't match any pass rules or any block rules, set either
# FR_PASS or FR_BLOCK (respectively).  It defaults to FR_PASS if left
# undefined.  This is ignored for ipftest, which can thus return three
# results: pass, block and nomatch.  This is the sort of "block unless
# explicitly allowed" type #define switch.
#
POLICY=-DIPF_DEFAULT_PASS=FR_PASS
#
MFLAGS="BINDEST=$(BINDEST)" "SBINDEST=$(SBINDEST)" "MANDIR=$(MANDIR)" \
	'CFLAGS=$(CFLAGS) $(SOLARIS2)' "IPFLKM=$(IPFLKM)" \
	"IPFLOG=$(IPFLOG)" "LOGFAC=$(LOGFAC)" "POLICY=$(POLICY)" \
	"SOLARIS2=$(SOLARIS2)" "DEBUG=$(DEBUG)" "DCPU=$(CPU)" \
	"CPUDIR=$(CPUDIR)"
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
	@echo "solaris	- auto-selects SunOS4.1.x/Solaris 2.[45]/Solaris2.[45]-x86"
	@echo "netbsd	- compile for NetBSD"
	@echo "openbsd	- compile for OpenBSD"
	@echo "freebsd	- compile for FreeBSD 2.0, 2.1 or earlier"
	@echo "freebsd22	- compile for FreeBSD-2.2 or greater"
	@echo "bsd	- compile for generic 4.4BSD systems"
	@echo "bsdi	- compile for BSD/OS"
	@echo "irix	- compile for SGI IRIX"
	@echo "linux	- compile for Linux 2.0.31+"
	@echo ""

tests:
	@if [ -d test ]; then (cd test; make) \
	else echo test directory not present, sorry; fi

include:
	if [ ! -d netinet -o ! -f netinet/done ] ; then \
		mkdir -p netinet; \
		(cd netinet; ln -s ../*.h .; ln -s ../ip_ftp_pxy.c .); \
		(cd netinet; ln -s ../ipsend/tcpip.h tcpip.h); \
		touch netinet/done; \
	fi

sunos solaris: include
	./buildsunos

freebsd22 freebsd30: include
	make setup "TARGOS=BSD" "CPUDIR=$(CPUDIR)"
	-rm -f BSD/$(CPUDIR)/ioconf.h
	@if [ -n $(IPFILKERN) ] ; then \
		if [ -f /sys/$(IPFILKERN)/compile/ioconf.h ] ; then \
		ln -s /sys/$(IPFILKERN)/compile/ioconf.h BSD/$(CPUDIR); \
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
	(cd IRIX/$(CPUDIR); smake build TOP=../.. $(MFLAGS); cd ..)
	(cd IRIX/$(CPUDIR); make -f Makefile.ipsend TOP=../.. $(MFLAGS); cd ..)

linux: include
	make setup "TARGOS=Linux" "CPUDIR=$(CPUDIR)"
	./buildlinux

linuxrev:
	(cd Linux/$(CPUDIR); make build TOP=../.. $(MFLAGS) LKM= ; cd ..)
	(cd Linux/$(CPUDIR); make -f Makefile.ipsend TOP=../.. $(MFLAGS); cd ..)

setup:
	-if [ ! -d $(TARGOS)/$(CPUDIR) ] ; then mkdir $(TARGOS)/$(CPUDIR); fi
	-rm -f $(TARGOS)/$(CPUDIR)/Makefile $(TARGOS)/$(CPUDIR)/Makefile.ipsend
	-ln -s ../Makefile $(TARGOS)/$(CPUDIR)/Makefile
	-ln -s ../Makefile.ipsend $(TARGOS)/$(CPUDIR)/Makefile.ipsend

clean:
	${RM} -rf netinet
	${RM} -f core *.o ipt fils ipf ipfstat ipftest ipmon if_ipl \
	vnode_if.h $(LKM)
	if [ "`uname -s`" = "SunOS" ]; then (cd SunOS4; make clean); fi
	if [ "`uname -s`" = "SunOS" ]; then (cd SunOS5; make clean); fi
	(cd BSD; make clean)
	(cd Linux; make clean)
	if [ "`uname -s`" = "IRIX" ]; then (cd IRIX; make clean); fi
	[ -d test ] && (cd test; make clean)
	(cd ipsend; make clean)

clean-bsd:
	(cd BSD; make clean)

clean-sunos4:
	(cd SunOS4; make clean)

clean-sunos5:
	(cd SunOS5; make clean)

clean-irix:
	(cd IRIX; make clean)

clean-linux:
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
	(cd SunOS4; make build TOP=.. "CC=$(CC)" $(MFLAGS); cd ..)
	(cd SunOS4; make -f Makefile.ipsend "CC=$(CC)" TOP=.. $(MFLAGS); cd ..)

sunos5 solaris2:
	(cd SunOS5/$(CPUDIR); make build TOP=../.. "CC=$(CC)" $(MFLAGS) "SOLARIS2=$(SOLARIS2)" "CPU=-Dsparc -D__sparc__"; cd ..)
	(cd SunOS5/$(CPUDIR); make -f Makefile.ipsend TOP=../.. "CC=$(CC)" $(MFLAGS); cd ..)

sunos5x86 solaris2x86:
	(cd SunOS5/$(CPUDIR); make build TOP=../.. "CC=$(CC)" $(MFLAGS) "SOLARIS2=$(SOLARIS2)" "CPU=-Di86pc -Di386 -D__i386__"; cd ..)
	(cd SunOS5/$(CPUDIR); make -f Makefile.ipsend TOP=../.. "CC=$(CC)" $(MFLAGS); cd ..)

install-linux:
	(cd Linux/$(CPUDIR); make install "TOP=../.." $(MFLAGS); cd ..)
	(cd Linux/$(CPUDIR); make -f Makefile.ipsend INSTALL=$(INSTALL) install "TOP=../.." $(MFLAGS); cd ..)

install-bsd:
	(cd BSD/$(CPUDIR); make install "TOP=../.." $(MFLAGS); cd ..)
	(cd BSD/$(CPUDIR); make -f Makefile.ipsend INSTALL=$(INSTALL) install "TOP=../.." $(MFLAGS); cd ..)

install-sunos4: solaris
	(cd SunOS4; $(MAKE) "CPU=$(CPU) TOP=.." install)

install-sunos5: solaris
	(cd SunOS5; $(MAKE) "CPU=$(CPU) TOP=.." install)

install-irix: irix
	(cd IRIX; smake install "CPU=$(CPU) TOP=.." $(MFLAGS))

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
