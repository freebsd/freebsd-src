#
# (C)opyright 1993, 1994, 1995 by Darren Reed.
#
# This code may be freely distributed as long as it retains this notice
# and is not changed in any way.  The author accepts no responsibility
# for the use of this software.  I hate legaleese, don't you ?
#
# $Id: Makefile,v 2.0.1.5 1997/02/16 06:17:04 darrenr Exp $
#
# where to put things.
#
BINDEST=/usr/local/ip_fil3.1.1/bin
SBINDEST=/usr/local/ip_fil3.1.1/sbin
MANDIR=/usr/local/ip_fil3.1.1/man
CC=gcc
DEBUG=-g
CFLAGS=-I$$(TOP)
DCPU=`uname -m`
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
	"CC=$(CC)" 'CFLAGS=$(CFLAGS) $(SOLARIS2)' "IPFLKM=$(IPFLKM)" \
	"IPFLOG=$(IPFLOG)" "LOGFAC=$(LOGFAC)" "POLICY=$(POLICY)" \
	"SOLARIS2=$(SOLARIS2)" "DEBUG=$(DEBUG)" "ARCH=$(ARCH)"
#
########## ########## ########## ########## ########## ########## ##########
#
CP=/bin/cp
RM=/bin/rm
CHMOD=/bin/chmod
INSTALL=install
#
DFLAGS=$(IPFLKM) $(IPFLOG) $(DEF)

all:
	@echo "Chose one of the following targets for making IP filter:"
	@echo ""
	@echo "solaris	- auto-selects SunOS4.1.x/Solaris 2.[45]/Soalris2.[45]-x86"
	@echo "bsd	- compile for 4.4BSD based Unixes (FreeBSD/NetBSD/OpenBSD)"
	@echo "bsdi	- compile for BSD/OS"
	@echo ""

tests:
	@if [ -d test ]; then (cd test; make) \
	else echo test directory not present, sorry; fi

sunos solaris:
	./buildsunos

sunos4 solaris1:
	(cd SunOS4; make build TOP=.. $(MFLAGS); cd ..)
	(cd SunOS4; make -f Makefile.ipsend TOP=.. $(MFLAGS); cd ..)

sunos5 solaris2:
	(cd SunOS5/$(DCPU); make build TOP=../.. $(MFLAGS) "SOLARIS2=$(SOLARIS2)" "CPU=-Dsparc -D__sparc__"; cd ..)
	(cd SunOS5/$(DCPU); make -f Makefile.ipsend TOP=../.. $(MFLAGS); cd ..)

sunos5x86 solaris2x86:
	(cd SunOS5/$(DCPU); make build TOP=../.. $(MFLAGS) "SOLARIS2=$(SOLARIS2)" "CPU=-Di86pc -Di386 -D__i386__"; cd ..)
	(cd SunOS5/$(DCPU); make -f Makefile.ipsend TOP=../.. $(MFLAGS); cd ..)

bsd netbsd freebsd:
	-if [ ! -d BSD/$(DCPU) ] ; then mkdir BSD/$(DCPU); fi
	-rm -f BSD/$(DCPU)/Makefile BSD/$(DCPU)/Makefile.ipsend
	-ln -s ../Makefile BSD/$(DCPU)/Makefile
	-ln -s ../Makefile.ipsend BSD/$(DCPU)/Makefile.ipsend
	(cd BSD/$(DCPU); make build "TOP=../.." $(MFLAGS); cd ..)
	(cd BSD/$(DCPU); make -f Makefile.ipsend "TOP=../.." $(MFLAGS); cd ..)

bsdi bsdos:
	-if [ ! -d BSD/$(DCPU) ] ; then mkdir BSD/$(DCPU); fi
	-rm -f BSD/$(DCPU)/Makefile BSD/$(DCPU)/Makefile.ipsend
	-ln -s ../Makefile BSD/$(DCPU)/Makefile
	-ln -s ../Makefile.ipsend BSD/$(DCPU)/Makefile.ipsend
	(cd BSD/$(DCPU); make build "TOP=../.." $(MFLAGS) LKM= ; cd ..)
	(cd BSD/$(DCPU); make -f Makefile.ipsend "TOP=../.." $(MFLAGS); cd ..)

clean:
	${RM} -f core *.o ipt fils ipf ipfstat ipftest ipmon if_ipl \
	vnode_if.h $(LKM)
	(cd SunOS4; make clean)
	(cd SunOS5; make clean)
	(cd BSD; make clean)
	[ -d test ] && (cd test; make clean)
	(cd ipsend; make clean)

clean-bsd:
	(cd BSD; make clean)

clean-sunos4:
	(cd SunOS4; make clean)

clean-sunos5:
	(cd SunOS5; make clean)

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

install-bsd: bsd
	(cd BSD/$(DCPU); $(MAKE) "TOP=../.." install)
install-sunos4: solaris
	(cd SunOS4; $(MAKE) "TOP=.." install)
install-sunos5: solaris
	(cd SunOS5; $(MAKE) "TOP=.." install)

# XXX FIXME: bogus to depend on all!
install: all ip_fil.h
	-$(CP) ip_fil.h /usr/include/netinet/ip_fil.h
	-$(CHMOD) 444 /usr/include/netinet/ip_fil.h
	-$(INSTALL) -cs -g wheel -m 755 -o root ipfstat ipf ipnat $(SBINDEST)
	-$(INSTALL) -cs -g wheel -m 755 -o root ipmon ipftest $(BINDEST)
	(cd man; $(MAKE) INSTALL=$(INSTALL) MANDIR=$(MANDIR) install; cd ..)

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
