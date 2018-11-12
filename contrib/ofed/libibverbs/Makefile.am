INCLUDES = -I$(srcdir)/include

lib_LTLIBRARIES = src/libibverbs.la

AM_CFLAGS = -g -Wall -D_GNU_SOURCE

src_libibverbs_la_CFLAGS = $(AM_CFLAGS) -DIBV_CONFIG_DIR=\"$(sysconfdir)/libibverbs.d\"

libibverbs_version_script = @LIBIBVERBS_VERSION_SCRIPT@

src_libibverbs_la_SOURCES = src/cmd.c src/compat-1_0.c src/device.c src/init.c \
			    src/marshall.c src/memory.c src/sysfs.c src/verbs.c \
			    src/enum_strs.c
src_libibverbs_la_LDFLAGS = -version-info 1 -export-dynamic \
    $(libibverbs_version_script)
src_libibverbs_la_DEPENDENCIES = $(srcdir)/src/libibverbs.map

bin_PROGRAMS = examples/ibv_devices examples/ibv_devinfo \
    examples/ibv_asyncwatch examples/ibv_rc_pingpong examples/ibv_uc_pingpong \
    examples/ibv_ud_pingpong examples/ibv_srq_pingpong
examples_ibv_devices_SOURCES = examples/device_list.c
examples_ibv_devices_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_devinfo_SOURCES = examples/devinfo.c
examples_ibv_devinfo_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_rc_pingpong_SOURCES = examples/rc_pingpong.c examples/pingpong.c
examples_ibv_rc_pingpong_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_uc_pingpong_SOURCES = examples/uc_pingpong.c examples/pingpong.c
examples_ibv_uc_pingpong_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_ud_pingpong_SOURCES = examples/ud_pingpong.c examples/pingpong.c
examples_ibv_ud_pingpong_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_srq_pingpong_SOURCES = examples/srq_pingpong.c examples/pingpong.c
examples_ibv_srq_pingpong_LDADD = $(top_builddir)/src/libibverbs.la
examples_ibv_asyncwatch_SOURCES = examples/asyncwatch.c
examples_ibv_asyncwatch_LDADD = $(top_builddir)/src/libibverbs.la

libibverbsincludedir = $(includedir)/infiniband

libibverbsinclude_HEADERS = include/infiniband/arch.h include/infiniband/driver.h \
    include/infiniband/kern-abi.h include/infiniband/opcode.h include/infiniband/verbs.h \
    include/infiniband/sa-kern-abi.h include/infiniband/sa.h include/infiniband/marshall.h

man_MANS = man/ibv_asyncwatch.1 man/ibv_devices.1 man/ibv_devinfo.1	\
    man/ibv_rc_pingpong.1 man/ibv_uc_pingpong.1 man/ibv_ud_pingpong.1	\
    man/ibv_srq_pingpong.1 man/ibv_alloc_pd.3 man/ibv_attach_mcast.3	\
    man/ibv_create_ah.3 man/ibv_create_ah_from_wc.3			\
    man/ibv_create_comp_channel.3 man/ibv_create_cq.3			\
    man/ibv_create_qp.3 man/ibv_create_srq.3				\
    man/ibv_create_xrc_rcv_qp.3 man/ibv_event_type_str.3		\
    man/ibv_fork_init.3 man/ibv_get_async_event.3			\
    man/ibv_get_cq_event.3 man/ibv_get_device_guid.3			\
    man/ibv_get_device_list.3 man/ibv_get_device_name.3			\
    man/ibv_modify_qp.3 man/ibv_modify_srq.3 man/ibv_modify_xrc_rcv_qp.3 \
    man/ibv_open_device.3 man/ibv_open_xrc_domain.3			\
    man/ibv_poll_cq.3 man/ibv_post_recv.3 man/ibv_post_send.3		\
    man/ibv_post_srq_recv.3 man/ibv_query_device.3 man/ibv_query_gid.3	\
    man/ibv_query_pkey.3 man/ibv_query_port.3 man/ibv_query_qp.3	\
    man/ibv_query_srq.3 man/ibv_query_xrc_rcv_qp.3			\
    man/ibv_rate_to_mult.3 man/ibv_reg_mr.3 man/ibv_reg_xrc_rcv_qp.3	\
    man/ibv_req_notify_cq.3 man/ibv_resize_cq.3 man/verbs.7

DEBIAN = debian/changelog debian/compat debian/control debian/copyright \
    debian/ibverbs-utils.install debian/libibverbs1.install \
    debian/libibverbs1.postinst debian/libibverbs-dev.install \
    debian/rules

EXTRA_DIST = include/infiniband/driver.h include/infiniband/kern-abi.h \
    include/infiniband/opcode.h include/infiniband/verbs.h include/infiniband/marshall.h \
    include/infiniband/sa-kern-abi.h include/infiniband/sa.h \
    src/ibverbs.h examples/pingpong.h \
    src/libibverbs.map libibverbs.spec.in $(man_MANS)

dist-hook: libibverbs.spec
	cp libibverbs.spec $(distdir)

install-data-hook:
	cd $(DESTDIR)$(mandir)/man3 && \
	$(RM) ibv_ack_async_event.3 && \
	$(RM) ibv_ack_cq_events.3 && \
	$(RM) ibv_close_device.3 && \
	$(RM) ibv_close_xrc_domain.3 && \
	$(RM) ibv_create_xrc_srq.3 && \
	$(RM) ibv_dealloc_pd.3 && \
	$(RM) ibv_dereg_mr.3 && \
	$(RM) ibv_destroy_ah.3 && \
	$(RM) ibv_destroy_comp_channel.3 && \
	$(RM) ibv_destroy_cq.3 && \
	$(RM) ibv_destroy_qp.3 && \
	$(RM) ibv_destroy_srq.3 && \
	$(RM) ibv_detach_mcast.3 && \
	$(RM) ibv_free_device_list.3 && \
	$(RM) ibv_init_ah_from_wc.3 && \
	$(RM) ibv_unreg_xrc_rcv_qp.3 && \
	$(RM) mult_to_ibv_rate.3 && \
	$(RM) ibv_node_type_str.3 && \
	$(RM) ibv_port_state_str.3 && \
	$(LN_S) ibv_get_async_event.3 ibv_ack_async_event.3 && \
	$(LN_S) ibv_get_cq_event.3 ibv_ack_cq_events.3 && \
	$(LN_S) ibv_open_device.3 ibv_close_device.3 && \
	$(LN_S) ibv_open_xrc_domain.3 ibv_close_xrc_domain.3 && \
	$(LN_S) ibv_create_srq.3 ibv_create_xrc_srq.3 && \
	$(LN_S) ibv_alloc_pd.3 ibv_dealloc_pd.3 && \
	$(LN_S) ibv_reg_mr.3 ibv_dereg_mr.3 && \
	$(LN_S) ibv_create_ah.3 ibv_destroy_ah.3 && \
	$(LN_S) ibv_create_comp_channel.3 ibv_destroy_comp_channel.3 && \
	$(LN_S) ibv_create_cq.3 ibv_destroy_cq.3 && \
	$(LN_S) ibv_create_qp.3 ibv_destroy_qp.3 && \
	$(LN_S) ibv_create_srq.3 ibv_destroy_srq.3 && \
	$(LN_S) ibv_attach_mcast.3 ibv_detach_mcast.3 && \
	$(LN_S) ibv_get_device_list.3 ibv_free_device_list.3 && \
	$(LN_S) ibv_create_ah_from_wc.3 ibv_init_ah_from_wc.3 && \
	$(LN_S) ibv_reg_xrc_rcv_qp.3 ibv_unreg_xrc_rcv_qp.3 && \
	$(LN_S) ibv_rate_to_mult.3 mult_to_ibv_rate.3 && \
	$(LN_S) ibv_event_type_str.3 ibv_node_type_str.3 && \
	$(LN_S) ibv_event_type_str.3 ibv_port_state_str.3
