INCLUDES = -I$(srcdir)/include

lib_LTLIBRARIES = src/librdmacm.la

AM_CFLAGS = -g -Wall -D_GNU_SOURCE

src_librdmacm_la_CFLAGS = $(AM_CFLAGS)

if HAVE_LD_VERSION_SCRIPT
    librdmacm_version_script = -Wl,--version-script=$(srcdir)/src/librdmacm.map
else
    librdmacm_version_script =
endif

src_librdmacm_la_SOURCES = src/cma.c
src_librdmacm_la_LDFLAGS = -version-info 1 -export-dynamic \
			   $(librdmacm_version_script)
src_librdmacm_la_DEPENDENCIES =  $(srcdir)/src/librdmacm.map

bin_PROGRAMS = examples/ucmatose examples/rping examples/udaddy examples/mckey
examples_ucmatose_SOURCES = examples/cmatose.c
examples_ucmatose_LDADD = $(top_builddir)/src/librdmacm.la
examples_rping_SOURCES = examples/rping.c
examples_rping_LDADD = $(top_builddir)/src/librdmacm.la
examples_udaddy_SOURCES = examples/udaddy.c
examples_udaddy_LDADD = $(top_builddir)/src/librdmacm.la
examples_mckey_SOURCES = examples/mckey.c
examples_mckey_LDADD = $(top_builddir)/src/librdmacm.la

librdmacmincludedir = $(includedir)/rdma

librdmacminclude_HEADERS = include/rdma/rdma_cma_abi.h \
			   include/rdma/rdma_cma.h

man_MANS = \
	man/rdma_accept.3 \
	man/rdma_ack_cm_event.3 \
	man/rdma_bind_addr.3 \
	man/rdma_connect.3 \
	man/rdma_create_event_channel.3 \
	man/rdma_create_id.3 \
	man/rdma_create_qp.3 \
	man/rdma_destroy_event_channel.3 \
	man/rdma_destroy_id.3 \
	man/rdma_destroy_qp.3 \
	man/rdma_disconnect.3 \
	man/rdma_free_devices.3 \
	man/rdma_get_cm_event.3 \
	man/rdma_get_devices.3 \
	man/rdma_get_src_port.3 \
	man/rdma_get_dst_port.3 \
	man/rdma_get_local_addr.3 \
	man/rdma_get_peer_addr.3 \
	man/rdma_join_multicast.3 \
	man/rdma_leave_multicast.3 \
	man/rdma_listen.3 \
	man/rdma_migrate_id.3 \
	man/rdma_notify.3 \
	man/rdma_reject.3 \
	man/rdma_resolve_addr.3 \
	man/rdma_resolve_route.3 \
	man/rdma_event_str.3 \
	man/rdma_set_option.3 \
	man/ucmatose.1 \
	man/udaddy.1 \
	man/mckey.1 \
	man/rping.1 \
	man/rdma_cm.7

EXTRA_DIST = include/rdma/rdma_cma_abi.h include/rdma/rdma_cma.h \
	     src/librdmacm.map librdmacm.spec.in $(man_MANS)

dist-hook: librdmacm.spec
	cp librdmacm.spec $(distdir)
