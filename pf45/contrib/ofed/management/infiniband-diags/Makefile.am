
INCLUDES = -I$(top_builddir)/include/ -I$(srcdir)/include -I$(includedir) -I$(includedir)/infiniband

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS =
endif

sbin_PROGRAMS = src/ibaddr src/ibnetdiscover src/ibping src/ibportstate \
	        src/ibroute src/ibstat src/ibsysstat src/ibtracert \
	        src/perfquery src/sminfo src/smpdump src/smpquery \
	        src/saquery src/vendstat

if ENABLE_TEST_UTILS
sbin_PROGRAMS += src/ibsendtrap src/mcm_rereg_test
endif

sbin_SCRIPTS = scripts/ibcheckerrs scripts/ibchecknet scripts/ibchecknode \
               scripts/ibcheckport scripts/ibhosts scripts/ibstatus \
	       scripts/ibswitches scripts/ibnodes scripts/ibrouters \
	       scripts/ibcheckwidth scripts/ibcheckportwidth \
	       scripts/ibcheckstate scripts/ibcheckportstate \
	       scripts/ibcheckerrors scripts/ibclearerrors \
	       scripts/ibclearcounters scripts/ibdatacounts \
	       scripts/ibdatacounters scripts/ibdiscover.pl \
	       scripts/dump_lfts.sh scripts/dump_mfts.sh \
	       scripts/set_nodedesc.sh \
	       scripts/ibqueryerrors.pl scripts/ibswportwatch.pl \
	       scripts/iblinkinfo.pl scripts/ibprintswitch.pl \
	       scripts/ibprintca.pl scripts/ibprintrt.pl \
	       scripts/ibfindnodesusing.pl scripts/ibidsverify.pl \
	       scripts/check_lft_balance.pl

src_ibaddr_SOURCES = src/ibaddr.c src/ibdiag_common.c
src_ibaddr_CFLAGS = -Wall $(DBGFLAGS)

src_ibnetdiscover_SOURCES = src/ibnetdiscover.c src/grouping.c src/ibdiag_common.c
src_ibnetdiscover_CFLAGS = -Wall $(DBGFLAGS)
src_ibnetdiscover_LDFLAGS = -Wl,--rpath -Wl,$(libdir)

src_ibping_SOURCES = src/ibping.c src/ibdiag_common.c
src_ibping_CFLAGS = -Wall $(DBGFLAGS)

src_ibportstate_SOURCES = src/ibportstate.c src/ibdiag_common.c
src_ibportstate_CFLAGS = -Wall $(DBGFLAGS)

src_ibroute_SOURCES = src/ibroute.c src/ibdiag_common.c
src_ibroute_CFLAGS = -Wall $(DBGFLAGS)

src_ibstat_SOURCES = src/ibstat.c
src_ibstat_CFLAGS = -Wall $(DBGFLAGS)

src_ibsysstat_SOURCES = src/ibsysstat.c src/ibdiag_common.c
src_ibsysstat_CFLAGS = -Wall $(DBGFLAGS)

src_ibtracert_SOURCES = src/ibtracert.c src/ibdiag_common.c
src_ibtracert_CFLAGS = -Wall $(DBGFLAGS)
src_ibtracert_LDFLAGS = -Wl,--rpath -Wl,$(libdir)

src_perfquery_SOURCES = src/perfquery.c src/ibdiag_common.c
src_perfquery_CFLAGS = -Wall $(DBGFLAGS)

src_sminfo_SOURCES = src/sminfo.c src/ibdiag_common.c
src_sminfo_CFLAGS = -Wall $(DBGFLAGS)

src_smpdump_SOURCES = src/smpdump.c
src_smpdump_CFLAGS = -Wall $(DBGFLAGS)

src_smpquery_SOURCES = src/smpquery.c src/ibdiag_common.c
src_smpquery_CFLAGS = -Wall $(DBGFLAGS)
src_smpquery_LDFLAGS = -Wl,--rpath -Wl,$(libdir)

src_saquery_SOURCES = src/saquery.c src/ibdiag_common.c
src_saquery_CFLAGS = -Wall -DOSM_VENDOR_INTF_OPENIB -DVENDOR_RMPP_SUPPORT -DDUAL_SIDED_RMPP $(DBGFLAGS)
src_saquery_LDFLAGS = -Wl,--rpath -Wl,$(libdir)

src_ibsendtrap_SOURCES = src/ibsendtrap.c src/ibdiag_common.c
src_ibsendtrap_CFLAGS = -Wall $(DBGFLAGS)
src_ibsendtrap_LDFLAGS = -Wl,--rpath -Wl,$(libdir)

src_vendstat_SOURCES = src/vendstat.c src/ibdiag_common.c
src_vendstat_CFLAGS = -Wall $(DBGFLAGS)

src_mcm_rereg_test_SOURCES = src/mcm_rereg_test.c
src_mcm_rereg_test_CFLAGS = -Wall $(DBGFLAGS)

man_MANS = man/ibaddr.8 man/ibcheckerrors.8 man/ibcheckerrs.8 \
	man/ibchecknet.8 man/ibchecknode.8 man/ibcheckport.8 \
	man/ibcheckportstate.8 man/ibcheckportwidth.8 man/ibcheckstate.8 \
	man/ibcheckwidth.8 man/ibclearcounters.8 man/ibclearerrors.8 \
	man/ibhosts.8 man/ibnetdiscover.8 man/ibnodes.8 man/ibping.8 \
	man/ibportstate.8 man/ibroute.8 man/ibstat.8 man/ibstatus.8 \
	man/ibswitches.8 man/ibtracert.8 man/perfquery.8 man/sminfo.8 \
	man/smpdump.8 man/smpquery.8 man/saquery.8 man/vendstat.8 \
	man/dump_lfts.8 man/dump_mfts.8 man/ibdiscover.8 man/ibsysstat.8 \
	man/iblinkinfo.8 man/ibqueryerrors.8 man/ibswportwatch.8 \
	man/ibprintswitch.8 man/ibprintca.8 man/ibfindnodesusing.8 \
	man/ibdatacounts.8 man/ibdatacounters.8 \
	man/ibrouters.8 man/ibprintrt.8 man/ibidsverify.8 \
	man/check_lft_balance.8

BUILT_SOURCES = ibdiag_version
ibdiag_version:
	if [ -x $(top_srcdir)/../gen_ver.sh ] ; then \
		ver_file=$(top_builddir)/include/ibdiag_version.h ; \
		ibdiag_ver=`cat $$ver_file | sed -ne '/#define IBDIAG_VERSION /s/^.*\"\(.*\)\"$$/\1/p'` ; \
		ver=`$(top_srcdir)/../gen_ver.sh $(PACKAGE)` ; \
		if [ $$ver != $$ibdiag_ver ] ; then \
			cat $$ver_file | sed -e '/#define IBDIAG_VERSION /s/\".*\"/\"'$$ver'\"/' > tmp_new_version ; \
			cat tmp_new_version > $$ver_file && rm -f tmp_new_version ; \
		fi ; \
	fi

EXTRA_DIST = scripts include infiniband-diags.spec.in infiniband-diags.spec \
	$(man_MANS) autogen.sh

dist-hook:
	if [ -x $(top_srcdir)/../gen_chlog.sh ] ; then \
		$(top_srcdir)/../gen_chlog.sh $(PACKAGE) > $(distdir)/ChangeLog ; \
	fi

# install this to a default location.
install-data-hook:
	$(top_srcdir)/config/install-sh -c -m 444 $(top_srcdir)/scripts/IBswcountlimits.pm $(DESTDIR)/$(PERL_INSTALLDIR)/IBswcountlimits.pm
