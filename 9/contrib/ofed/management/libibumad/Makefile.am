
SUBDIRS = .

INCLUDES = -I$(srcdir)/include/infiniband -I$(includedir)

man_MANS = man/umad_debug.3 man/umad_get_ca.3 \
	   man/umad_get_ca_portguids.3 man/umad_get_cas_names.3 \
	   man/umad_get_mad.3 man/umad_get_port.3 man/umad_init.3 \
	   man/umad_open_port.3 man/umad_close_port.3 man/umad_size.3 \
	   man/umad_status.3 man/umad_alloc.3 man/umad_free.3 \
	   man/umad_dump.3 man/umad_addr_dump.3 man/umad_get_fd.3 \
	   man/umad_get_mad.3 man/umad_get_mad_addr.3 \
	   man/umad_set_grh_net.3 man/umad_set_grh.3 \
	   man/umad_set_addr_net.3 man/umad_set_addr.3 man/umad_set_pkey.3 \
	   man/umad_get_pkey.3 \
	   man/umad_register.3 man/umad_register_oui.3 man/umad_unregister.3 \
	   man/umad_send.3 man/umad_recv.3 man/umad_poll.3 \
	   man/umad_get_issm_path.3

lib_LTLIBRARIES = libibumad.la

libibumad_la_CFLAGS = -Wall

if HAVE_LD_VERSION_SCRIPT
libibumad_version_script = -Wl,--version-script=$(srcdir)/src/libibumad.map
else
libibumad_version_script =
endif

libibumad_la_SOURCES = src/umad.c
libibumad_la_LDFLAGS = -version-info $(ibumad_api_version) \
    -export-dynamic $(libibumad_version_script)
libibumad_la_DEPENDENCIES = $(srcdir)/src/libibumad.map

libibumadincludedir = $(includedir)/infiniband

libibumadinclude_HEADERS = $(srcdir)/include/infiniband/umad.h

EXTRA_DIST = $(srcdir)/include/infiniband/umad.h \
	libibumad.spec.in libibumad.spec \
	$(srcdir)/src/libibumad.map libibumad.ver \
	$(man_MANS) autogen.sh

dist-hook:
	if [ -x $(top_srcdir)/../gen_chlog.sh ] ; then \
		$(top_srcdir)/../gen_chlog.sh $(PACKAGE) > $(distdir)/ChangeLog ; \
	fi

install-data-hook:
	cd $(DESTDIR)$(mandir)/man3 && \
	$(RM) umad_done.3 && \
	$(RM) umad_release_ca.3 && \
	$(RM) umad_release_port.3 && \
	$(LN_S) umad_init.3 umad_done.3 && \
	$(LN_S) umad_get_ca.3 umad_release_ca.3 && \
	$(LN_S) umad_get_port.3 umad_release_port.3
