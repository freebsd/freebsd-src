#AUTOMAKE_OPTIONS = ../util/ansi2knr no-dependencies
AUTOMAKE_OPTIONS = ../util/ansi2knr
bin_PROGRAMS = ntpd
INCLUDES = -I$(top_srcdir)/include
# LDADD might need RESLIB and ADJLIB
LDADD =	version.o @LIBPARSE@ ../libntp/libntp.a @LIBRSAREF@
ntpd_LDADD = $(LDADD) -lm
DISTCLEANFILES = .version version.c
#EXTRA_DIST = ntpd.mak
ETAGS_ARGS = Makefile.am
###							Y2Kfixes
check_PROGRAMS = @MAKE_CHECK_Y2K@
EXTRA_PROGRAMS = check_y2k

check-local: @MAKE_CHECK_Y2K@
	[ -z "@MAKE_CHECK_Y2K@" ] || ./@MAKE_CHECK_Y2K@

ntpd_SOURCES = jupiter.h map_vme.c ntp_config.c ntp_control.c ntp_filegen.c \
	ntp_intres.c ntp_io.c ntp_loopfilter.c ntp_monitor.c ntp_peer.c \
	ntp_proto.c ntp_refclock.c ntp_request.c ntp_restrict.c ntp_timer.c \
	ntp_util.c ntpd.c refclock_acts.c refclock_arbiter.c refclock_arc.c \
	refclock_as2201.c refclock_atom.c refclock_bancomm.c \
	refclock_chronolog.c refclock_chu.c refclock_conf.c refclock_datum.c \
	refclock_dumbclock.c refclock_fg.c refclock_gpsvme.c refclock_heath.c \
	refclock_hpgps.c refclock_irig.c refclock_jupiter.c refclock_leitch.c \
	refclock_local.c refclock_msfees.c refclock_mx4200.c refclock_nmea.c \
	refclock_oncore.c refclock_palisade.c refclock_palisade.h \
	refclock_parse.c refclock_pcf.c refclock_pst.c refclock_ptbacts.c \
	refclock_shm.c refclock_tpro.c refclock_trak.c refclock_true.c \
	refclock_ulink.c refclock_usno.c refclock_wwv.c refclock_wwvb.c

$(PROGRAMS): $(LDADD)

../libntp/libntp.a:
	cd ../libntp && $(MAKE)

../libparse/libparse.a:
	cd ../libparse && $(MAKE)

version.o: $(ntpd_OBJECTS) ../libntp/libntp.a @LIBPARSE@ @LIBRSAREF@ Makefile
	$(top_builddir)/scripts/mkver ntpd
	$(COMPILE) -c version.c
