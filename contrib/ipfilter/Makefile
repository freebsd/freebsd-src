#
# (C)opyright 1993, 1994, 1995 by Darren Reed.
#
# This code may be freely distributed as long as it retains this notice
# and is not changed in any way.  The author accepts no responsibility
# for the use of this software.  I hate legaleese, don't you ?
#
# $Id: Makefile,v 2.0.2.12 1997/05/24 08:13:34 darrenr Exp $
#
# where to put things.
#
BINDEST=/usr/local/bin
SBINDEST=/sbin
MANDIR=/usr/local/man
#To test prototyping
#CC=gcc -Wstrict-prototypes -Wmissing-prototypes -Werror
CC=gcc
DEBUG=-g
CFLAGS=-I$$(TOP)
CPU=`uname -m`
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
	"SOLARIS2=$(SOLARIS2)" "DEBUG=$(DEBUG)" "DCPU=$(CPU)"
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

include:
	mkdir -p netinet
	(cd netinet; /bin/rm -f *; ln -s ../*.h .; ln -s ../ip_ftp_pxy.c .)

sunos solaris: include
	./buildsunos

freebsd22 freebsd30: include
	-if [ ! -d BSD/$(CPU) ] ; then mkdir BSD/$(CPU); fi
	-rm -f BSD/$(CPU)/ioconf.h
	@if [ -n $(IPFILKERN) ] ; then \
		ln -s /sys/$(IPFILKERN)/ioconf.h BSD/$(CPU); \
	elif [ ! -f `uname -v|sed -e 's@^.*:\(/[^: ]*\).*@\1@'`/ioconf.h ] ; then \
		echo -n "Can't find ioconf.h in "; \
		echo `uname -v|sed -e 's@^.*:\(/[^: ]*\).*@\1@'`; \
		exit 1;\
	else \
		ln -s `uname -v|sed -e 's@^.*:\(/[^: ]*\).*@\1@'`/ioconf.h BSD/$(CPU) ; \
	fi
	make freebsd

netbsd: include
	-if [ ! -d BSD/$(CPU) ] ; then mkdir BSD/$(CPU); fi
	-rm -f BSD/$(CPU)/Makefile BSD/$(CPU)/Makefile.ipsend
	-ln -s ../Makefile BSD/$(CPU)/Makefile
	-ln -s ../Makefile.ipsend BSD/$(CPU)/Makefile.ipsend
	(cd BSD/$(CPU); make build "TOP=../.." $(MFLAGS) "ML=mln_ipl.c"; cd ..)
	(cd BSD/$(CPU); make -f Makefile.ipsend "TOP=../.." $(MFLAGS); cd ..)

freebsd freebsd20 freebsd21: include
	-if [ ! -d BSD/$(CPU) ] ; then mkdir BSD/$(CPU); fi
	-rm -f BSD/$(CPU)/Makefile BSD/$(CPU)/Makefile.ipsend
	-ln -s ../Makefile BSD/$(CPU)/Makefile
	-ln -s ../Makefile.ipsend BSD/$(CPU)/Makefile.ipsend
	(cd BSD/$(CPU); make build "TOP=../.." $(MFLAGS) "ML=mlf_ipl.c"; cd ..)
	(cd BSD/$(CPU); make -f Makefile.ipsend "TOP=../.." $(MFLAGS); cd ..)

bsd: include
	-if [ ! -d BSD/$(CPU) ] ; then mkdir BSD/$(CPU); fi
	-rm -f BSD/$(CPU)/Makefile BSD/$(CPU)/Makefile.ipsend
	-ln -s ../Makefile BSD/$(CPU)/Makefile
	-ln -s ../Makefile.ipsend BSD/$(CPU)/Makefile.ipsend
	(cd BSD/$(CPU); make build "TOP=../.." $(MFLAGS); cd ..)
	(cd BSD/$(CPU); make -f Makefile.ipsend "TOP=../.." $(MFLAGS); cd ..)

bsdi bsdos: include
	-if [ ! -d BSD/$(CPU) ] ; then mkdir BSD/$(CPU); fi
	-rm -f BSD/$(CPU)/Makefile BSD/$(CPU)/Makefile.ipsend
	-ln -s ../Makefile BSD/$(CPU)/Makefile
	-ln -s ../Makefile.ipsend BSD/$(CPU)/Makefile.ipsend
	(cd BSD/$(CPU); make build "TOP=../.." $(MFLAGS) LKM= ; cd ..)
	(cd BSD/$(CPU); make -f Makefile.ipsend "TOP=../.." $(MFLAGS); cd ..)

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

sunos4 solaris1:
	(cd SunOS4; make build TOP=.. $(MFLAGS); cd ..)
	(cd SunOS4; make -f Makefile.ipsend TOP=.. $(MFLAGS); cd ..)

sunos5 solaris2:
	(cd SunOS5/$(CPU); make build TOP=../.. $(MFLAGS) "SOLARIS2=$(SOLARIS2)" "CPU=-Dsparc -D__sparc__"; cd ..)
	(cd SunOS5/$(CPU); make -f Makefile.ipsend TOP=../.. $(MFLAGS); cd ..)

sunos5x86 solaris2x86:
	(cd SunOS5/$(CPU); make build TOP=../.. $(MFLAGS) "SOLARIS2=$(SOLARIS2)" "CPU=-Di86pc -Di386 -D__i386__"; cd ..)
	(cd SunOS5/$(CPU); make -f Makefile.ipsend TOP=../.. $(MFLAGS); cd ..)

install-bsd: bsd
	(cd BSD/$(CPU); make install "TOP=../.." $(MFLAGS); cd ..)
	(cd BSD/$(CPU); make -f Makefile.ipsend INSTALL=$(INSTALL) install "TOP=../.." $(MFLAGS); cd ..)

install-sunos4: solaris
	(cd SunOS4; $(MAKE) "CPU=$(CPU) TOP=.." install)

install-sunos5: solaris
	(cd SunOS5; $(MAKE) "CPU=$(CPU) TOP=.." install)

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
