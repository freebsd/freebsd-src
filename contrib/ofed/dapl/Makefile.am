# $Id: $

OSFLAGS = -DOS_RELEASE=$(shell expr `uname -r | cut -f1 -d.` \* 65536 + `uname -r | cut -f2 -d.`)
# Check for RedHat, needed for ia64 udapl atomic operations (IA64_FETCHADD syntax)
# and built-in atomics for RedHat EL5
if OS_RHEL4
OSFLAGS += -DREDHAT_EL4
endif

if OS_RHEL5
OSFLAGS += -DREDHAT_EL5
endif

if OS_SUSE11
OSFLAGS += -DSUSE_11
endif

if EXT_TYPE_IB
XFLAGS = -DDAT_EXTENSIONS
XPROGRAMS = dapl/openib_common/ib_extensions.c
else
XFLAGS =
XPROGRAMS =
endif

if DEBUG
AM_CFLAGS = -g -Wall -D_GNU_SOURCE -DDAPL_DBG -DDAT_CONF="\"$(sysconfdir)/dat.conf\""
else
AM_CFLAGS = -g -Wall -D_GNU_SOURCE -DDAT_CONF="\"$(sysconfdir)/dat.conf\""
endif

datlibdir = $(libdir)
dapllibofadir = $(libdir)
daplliboscmdir = $(libdir)
daplliboucmdir = $(libdir)

datlib_LTLIBRARIES = dat/udat/libdat2.la
dapllibofa_LTLIBRARIES = dapl/udapl/libdaplofa.la
daplliboscm_LTLIBRARIES = dapl/udapl/libdaploscm.la
daplliboucm_LTLIBRARIES = dapl/udapl/libdaploucm.la

dat_udat_libdat2_la_CFLAGS = $(AM_CFLAGS) -D_GNU_SOURCE $(OSFLAGS) $(XFLAGS) \
				-I$(srcdir)/dat/include/ -I$(srcdir)/dat/udat/ \
				-I$(srcdir)/dat/udat/linux -I$(srcdir)/dat/common/ 

dapl_udapl_libdaplofa_la_CFLAGS = $(AM_CFLAGS) -D_GNU_SOURCE $(OSFLAGS) $(XFLAGS) \
				-DOPENIB -DCQ_WAIT_OBJECT \
				-I$(srcdir)/dat/include/ -I$(srcdir)/dapl/include/ \
				-I$(srcdir)/dapl/common -I$(srcdir)/dapl/udapl/linux \
				-I$(srcdir)/dapl/openib_common \
				-I$(srcdir)/dapl/openib_cma \
				-I$(srcdir)/dapl/openib_cma/linux
		
dapl_udapl_libdaploscm_la_CFLAGS = $(AM_CFLAGS) -D_GNU_SOURCE $(OSFLAGS) $(XFLAGS) \
                                -DOPENIB -DCQ_WAIT_OBJECT \
                                -I$(srcdir)/dat/include/ -I$(srcdir)/dapl/include/ \
                                -I$(srcdir)/dapl/common -I$(srcdir)/dapl/udapl/linux \
				-I$(srcdir)/dapl/openib_common \
                                -I$(srcdir)/dapl/openib_scm \
				-I$(srcdir)/dapl/openib_scm/linux

dapl_udapl_libdaploucm_la_CFLAGS = $(AM_CFLAGS) -D_GNU_SOURCE $(OSFLAGS) $(XFLAGS) \
                                -DOPENIB -DCQ_WAIT_OBJECT \
                                -I$(srcdir)/dat/include/ -I$(srcdir)/dapl/include/ \
                                -I$(srcdir)/dapl/common -I$(srcdir)/dapl/udapl/linux \
				-I$(srcdir)/dapl/openib_common \
                                -I$(srcdir)/dapl/openib_ucm \
				-I$(srcdir)/dapl/openib_ucm/linux				

if HAVE_LD_VERSION_SCRIPT
    dat_version_script = -Wl,--version-script=$(srcdir)/dat/udat/libdat2.map
    daplofa_version_script = -Wl,--version-script=$(srcdir)/dapl/udapl/libdaplofa.map
    daploscm_version_script = -Wl,--version-script=$(srcdir)/dapl/udapl/libdaploscm.map
    daploucm_version_script = -Wl,--version-script=$(srcdir)/dapl/udapl/libdaploucm.map
else
    dat_version_script = 
    daplofa_version_script = 
    daploscm_version_script =
    daploucm_version_script =
endif

#
# uDAT: libdat2.so
#
dat_udat_libdat2_la_SOURCES = dat/udat/udat.c \
			dat/udat/udat_api.c \
			dat/udat/udat_sr_parser.c \
			dat/udat/linux/dat_osd.c \
			dat/common/dat_api.c \
			dat/common/dat_dictionary.c \
			dat/common/dat_strerror.c \
			dat/common/dat_init.c \
			dat/common/dat_dr.c \
			dat/common/dat_sr.c
dat_udat_libdat2_la_LDFLAGS = -version-info 2:0:0 $(dat_version_script) -ldl 

#
# uDAPL OpenFabrics rdma_cm version: libdaplofa.so
#
dapl_udapl_libdaplofa_la_SOURCES = dapl/udapl/dapl_init.c \
        dapl/udapl/dapl_evd_create.c               \
        dapl/udapl/dapl_evd_query.c                \
        dapl/udapl/dapl_cno_create.c               \
        dapl/udapl/dapl_cno_modify_agent.c         \
        dapl/udapl/dapl_cno_free.c                 \
        dapl/udapl/dapl_cno_wait.c                 \
        dapl/udapl/dapl_cno_query.c                \
        dapl/udapl/dapl_lmr_create.c               \
        dapl/udapl/dapl_evd_wait.c                 \
        dapl/udapl/dapl_evd_disable.c              \
        dapl/udapl/dapl_evd_enable.c               \
        dapl/udapl/dapl_evd_modify_cno.c           \
        dapl/udapl/dapl_evd_set_unwaitable.c       \
        dapl/udapl/dapl_evd_clear_unwaitable.c     \
        dapl/udapl/linux/dapl_osd.c                \
        dapl/common/dapl_cookie.c                   \
        dapl/common/dapl_cr_accept.c                \
        dapl/common/dapl_cr_query.c                 \
        dapl/common/dapl_cr_reject.c                \
        dapl/common/dapl_cr_util.c                  \
        dapl/common/dapl_cr_callback.c              \
        dapl/common/dapl_cr_handoff.c               \
        dapl/common/dapl_ep_connect.c               \
        dapl/common/dapl_ep_create.c                \
        dapl/common/dapl_ep_disconnect.c            \
        dapl/common/dapl_ep_dup_connect.c           \
        dapl/common/dapl_ep_free.c                  \
        dapl/common/dapl_ep_reset.c                 \
        dapl/common/dapl_ep_get_status.c            \
        dapl/common/dapl_ep_modify.c                \
        dapl/common/dapl_ep_post_rdma_read.c        \
        dapl/common/dapl_ep_post_rdma_write.c       \
        dapl/common/dapl_ep_post_recv.c             \
        dapl/common/dapl_ep_post_send.c             \
        dapl/common/dapl_ep_query.c                 \
        dapl/common/dapl_ep_util.c                  \
        dapl/common/dapl_evd_dequeue.c              \
        dapl/common/dapl_evd_free.c                 \
        dapl/common/dapl_evd_post_se.c              \
        dapl/common/dapl_evd_resize.c               \
        dapl/common/dapl_evd_util.c                 \
        dapl/common/dapl_evd_cq_async_error_callb.c \
        dapl/common/dapl_evd_qp_async_error_callb.c \
        dapl/common/dapl_evd_un_async_error_callb.c \
        dapl/common/dapl_evd_connection_callb.c     \
        dapl/common/dapl_evd_dto_callb.c            \
        dapl/common/dapl_get_consumer_context.c     \
        dapl/common/dapl_get_handle_type.c          \
        dapl/common/dapl_hash.c                     \
        dapl/common/dapl_hca_util.c                 \
        dapl/common/dapl_ia_close.c                 \
        dapl/common/dapl_ia_open.c                  \
        dapl/common/dapl_ia_query.c                 \
        dapl/common/dapl_ia_util.c                  \
        dapl/common/dapl_llist.c                    \
        dapl/common/dapl_lmr_free.c                 \
        dapl/common/dapl_lmr_query.c                \
        dapl/common/dapl_lmr_util.c                 \
        dapl/common/dapl_lmr_sync_rdma_read.c       \
        dapl/common/dapl_lmr_sync_rdma_write.c      \
        dapl/common/dapl_mr_util.c                  \
        dapl/common/dapl_provider.c                 \
        dapl/common/dapl_sp_util.c                  \
        dapl/common/dapl_psp_create.c               \
        dapl/common/dapl_psp_create_any.c           \
        dapl/common/dapl_psp_free.c                 \
        dapl/common/dapl_psp_query.c                \
        dapl/common/dapl_pz_create.c                \
        dapl/common/dapl_pz_free.c                  \
        dapl/common/dapl_pz_query.c                 \
        dapl/common/dapl_pz_util.c                  \
        dapl/common/dapl_rmr_create.c               \
        dapl/common/dapl_rmr_free.c                 \
        dapl/common/dapl_rmr_bind.c                 \
        dapl/common/dapl_rmr_query.c                \
        dapl/common/dapl_rmr_util.c                 \
        dapl/common/dapl_rsp_create.c               \
        dapl/common/dapl_rsp_free.c                 \
        dapl/common/dapl_rsp_query.c                \
        dapl/common/dapl_cno_util.c                 \
        dapl/common/dapl_set_consumer_context.c     \
        dapl/common/dapl_ring_buffer_util.c         \
        dapl/common/dapl_name_service.c             \
        dapl/common/dapl_timer_util.c               \
        dapl/common/dapl_ep_create_with_srq.c       \
        dapl/common/dapl_ep_recv_query.c            \
        dapl/common/dapl_ep_set_watermark.c         \
        dapl/common/dapl_srq_create.c               \
        dapl/common/dapl_srq_free.c                 \
        dapl/common/dapl_srq_query.c                \
        dapl/common/dapl_srq_resize.c               \
        dapl/common/dapl_srq_post_recv.c            \
        dapl/common/dapl_srq_set_lw.c               \
        dapl/common/dapl_srq_util.c                 \
        dapl/common/dapl_debug.c                    \
        dapl/common/dapl_ia_ha.c                    \
        dapl/common/dapl_csp.c                      \
        dapl/common/dapl_ep_post_send_invalidate.c  \
        dapl/common/dapl_ep_post_rdma_read_to_rmr.c \
        dapl/openib_common/mem.c                    \
        dapl/openib_common/cq.c                     \
        dapl/openib_common/qp.c                     \
        dapl/openib_common/util.c                   \
        dapl/openib_cma/cm.c                        \
        dapl/openib_cma/device.c $(XPROGRAMS)

dapl_udapl_libdaplofa_la_LDFLAGS = -version-info 2:0:0 $(daplofa_version_script) \
				   -Wl,-init,dapl_init -Wl,-fini,dapl_fini \
				   -lpthread -libverbs -lrdmacm 
				
#
# uDAPL OpenFabrics Socket CM version for IB: libdaplscm.so
#
dapl_udapl_libdaploscm_la_SOURCES = dapl/udapl/dapl_init.c \
        dapl/udapl/dapl_evd_create.c               \
        dapl/udapl/dapl_evd_query.c                \
        dapl/udapl/dapl_cno_create.c               \
        dapl/udapl/dapl_cno_modify_agent.c         \
        dapl/udapl/dapl_cno_free.c                 \
        dapl/udapl/dapl_cno_wait.c                 \
        dapl/udapl/dapl_cno_query.c                \
        dapl/udapl/dapl_lmr_create.c               \
        dapl/udapl/dapl_evd_wait.c                 \
        dapl/udapl/dapl_evd_disable.c              \
        dapl/udapl/dapl_evd_enable.c               \
        dapl/udapl/dapl_evd_modify_cno.c           \
        dapl/udapl/dapl_evd_set_unwaitable.c       \
        dapl/udapl/dapl_evd_clear_unwaitable.c     \
        dapl/udapl/linux/dapl_osd.c                \
        dapl/common/dapl_cookie.c                   \
        dapl/common/dapl_cr_accept.c                \
        dapl/common/dapl_cr_query.c                 \
        dapl/common/dapl_cr_reject.c                \
        dapl/common/dapl_cr_util.c                  \
        dapl/common/dapl_cr_callback.c              \
        dapl/common/dapl_cr_handoff.c               \
        dapl/common/dapl_ep_connect.c               \
        dapl/common/dapl_ep_create.c                \
        dapl/common/dapl_ep_disconnect.c            \
        dapl/common/dapl_ep_dup_connect.c           \
        dapl/common/dapl_ep_free.c                  \
        dapl/common/dapl_ep_reset.c                 \
        dapl/common/dapl_ep_get_status.c            \
        dapl/common/dapl_ep_modify.c                \
        dapl/common/dapl_ep_post_rdma_read.c        \
        dapl/common/dapl_ep_post_rdma_write.c       \
        dapl/common/dapl_ep_post_recv.c             \
        dapl/common/dapl_ep_post_send.c             \
        dapl/common/dapl_ep_query.c                 \
        dapl/common/dapl_ep_util.c                  \
        dapl/common/dapl_evd_dequeue.c              \
        dapl/common/dapl_evd_free.c                 \
        dapl/common/dapl_evd_post_se.c              \
        dapl/common/dapl_evd_resize.c               \
        dapl/common/dapl_evd_util.c                 \
        dapl/common/dapl_evd_cq_async_error_callb.c \
        dapl/common/dapl_evd_qp_async_error_callb.c \
        dapl/common/dapl_evd_un_async_error_callb.c \
        dapl/common/dapl_evd_connection_callb.c     \
        dapl/common/dapl_evd_dto_callb.c            \
        dapl/common/dapl_get_consumer_context.c     \
        dapl/common/dapl_get_handle_type.c          \
        dapl/common/dapl_hash.c                     \
        dapl/common/dapl_hca_util.c                 \
        dapl/common/dapl_ia_close.c                 \
        dapl/common/dapl_ia_open.c                  \
        dapl/common/dapl_ia_query.c                 \
        dapl/common/dapl_ia_util.c                  \
        dapl/common/dapl_llist.c                    \
        dapl/common/dapl_lmr_free.c                 \
        dapl/common/dapl_lmr_query.c                \
        dapl/common/dapl_lmr_util.c                 \
        dapl/common/dapl_lmr_sync_rdma_read.c       \
        dapl/common/dapl_lmr_sync_rdma_write.c      \
        dapl/common/dapl_mr_util.c                  \
        dapl/common/dapl_provider.c                 \
        dapl/common/dapl_sp_util.c                  \
        dapl/common/dapl_psp_create.c               \
        dapl/common/dapl_psp_create_any.c           \
        dapl/common/dapl_psp_free.c                 \
        dapl/common/dapl_psp_query.c                \
        dapl/common/dapl_pz_create.c                \
        dapl/common/dapl_pz_free.c                  \
        dapl/common/dapl_pz_query.c                 \
        dapl/common/dapl_pz_util.c                  \
        dapl/common/dapl_rmr_create.c               \
        dapl/common/dapl_rmr_free.c                 \
        dapl/common/dapl_rmr_bind.c                 \
        dapl/common/dapl_rmr_query.c                \
        dapl/common/dapl_rmr_util.c                 \
        dapl/common/dapl_rsp_create.c               \
        dapl/common/dapl_rsp_free.c                 \
        dapl/common/dapl_rsp_query.c                \
        dapl/common/dapl_cno_util.c                 \
        dapl/common/dapl_set_consumer_context.c     \
        dapl/common/dapl_ring_buffer_util.c         \
        dapl/common/dapl_name_service.c             \
        dapl/common/dapl_timer_util.c               \
        dapl/common/dapl_ep_create_with_srq.c       \
        dapl/common/dapl_ep_recv_query.c            \
        dapl/common/dapl_ep_set_watermark.c         \
        dapl/common/dapl_srq_create.c               \
        dapl/common/dapl_srq_free.c                 \
        dapl/common/dapl_srq_query.c                \
        dapl/common/dapl_srq_resize.c               \
        dapl/common/dapl_srq_post_recv.c            \
        dapl/common/dapl_srq_set_lw.c               \
        dapl/common/dapl_srq_util.c                 \
        dapl/common/dapl_debug.c                    \
        dapl/common/dapl_ia_ha.c                    \
        dapl/common/dapl_csp.c                      \
        dapl/common/dapl_ep_post_send_invalidate.c  \
        dapl/common/dapl_ep_post_rdma_read_to_rmr.c \
        dapl/openib_common/mem.c                    \
        dapl/openib_common/cq.c                     \
        dapl/openib_common/qp.c                     \
        dapl/openib_common/util.c                   \
        dapl/openib_scm/cm.c                        \
        dapl/openib_scm/device.c $(XPROGRAMS)

dapl_udapl_libdaploscm_la_LDFLAGS = -version-info 2:0:0 $(daploscm_version_script) \
                                   -Wl,-init,dapl_init -Wl,-fini,dapl_fini \
                                   -lpthread -libverbs
                                   
#
# uDAPL OpenFabrics UD CM version for IB: libdaplucm.so
#
dapl_udapl_libdaploucm_la_SOURCES = dapl/udapl/dapl_init.c \
        dapl/udapl/dapl_evd_create.c               \
        dapl/udapl/dapl_evd_query.c                \
        dapl/udapl/dapl_cno_create.c               \
        dapl/udapl/dapl_cno_modify_agent.c         \
        dapl/udapl/dapl_cno_free.c                 \
        dapl/udapl/dapl_cno_wait.c                 \
        dapl/udapl/dapl_cno_query.c                \
        dapl/udapl/dapl_lmr_create.c               \
        dapl/udapl/dapl_evd_wait.c                 \
        dapl/udapl/dapl_evd_disable.c              \
        dapl/udapl/dapl_evd_enable.c               \
        dapl/udapl/dapl_evd_modify_cno.c           \
        dapl/udapl/dapl_evd_set_unwaitable.c       \
        dapl/udapl/dapl_evd_clear_unwaitable.c     \
        dapl/udapl/linux/dapl_osd.c                \
        dapl/common/dapl_cookie.c                   \
        dapl/common/dapl_cr_accept.c                \
        dapl/common/dapl_cr_query.c                 \
        dapl/common/dapl_cr_reject.c                \
        dapl/common/dapl_cr_util.c                  \
        dapl/common/dapl_cr_callback.c              \
        dapl/common/dapl_cr_handoff.c               \
        dapl/common/dapl_ep_connect.c               \
        dapl/common/dapl_ep_create.c                \
        dapl/common/dapl_ep_disconnect.c            \
        dapl/common/dapl_ep_dup_connect.c           \
        dapl/common/dapl_ep_free.c                  \
        dapl/common/dapl_ep_reset.c                 \
        dapl/common/dapl_ep_get_status.c            \
        dapl/common/dapl_ep_modify.c                \
        dapl/common/dapl_ep_post_rdma_read.c        \
        dapl/common/dapl_ep_post_rdma_write.c       \
        dapl/common/dapl_ep_post_recv.c             \
        dapl/common/dapl_ep_post_send.c             \
        dapl/common/dapl_ep_query.c                 \
        dapl/common/dapl_ep_util.c                  \
        dapl/common/dapl_evd_dequeue.c              \
        dapl/common/dapl_evd_free.c                 \
        dapl/common/dapl_evd_post_se.c              \
        dapl/common/dapl_evd_resize.c               \
        dapl/common/dapl_evd_util.c                 \
        dapl/common/dapl_evd_cq_async_error_callb.c \
        dapl/common/dapl_evd_qp_async_error_callb.c \
        dapl/common/dapl_evd_un_async_error_callb.c \
        dapl/common/dapl_evd_connection_callb.c     \
        dapl/common/dapl_evd_dto_callb.c            \
        dapl/common/dapl_get_consumer_context.c     \
        dapl/common/dapl_get_handle_type.c          \
        dapl/common/dapl_hash.c                     \
        dapl/common/dapl_hca_util.c                 \
        dapl/common/dapl_ia_close.c                 \
        dapl/common/dapl_ia_open.c                  \
        dapl/common/dapl_ia_query.c                 \
        dapl/common/dapl_ia_util.c                  \
        dapl/common/dapl_llist.c                    \
        dapl/common/dapl_lmr_free.c                 \
        dapl/common/dapl_lmr_query.c                \
        dapl/common/dapl_lmr_util.c                 \
        dapl/common/dapl_lmr_sync_rdma_read.c       \
        dapl/common/dapl_lmr_sync_rdma_write.c      \
        dapl/common/dapl_mr_util.c                  \
        dapl/common/dapl_provider.c                 \
        dapl/common/dapl_sp_util.c                  \
        dapl/common/dapl_psp_create.c               \
        dapl/common/dapl_psp_create_any.c           \
        dapl/common/dapl_psp_free.c                 \
        dapl/common/dapl_psp_query.c                \
        dapl/common/dapl_pz_create.c                \
        dapl/common/dapl_pz_free.c                  \
        dapl/common/dapl_pz_query.c                 \
        dapl/common/dapl_pz_util.c                  \
        dapl/common/dapl_rmr_create.c               \
        dapl/common/dapl_rmr_free.c                 \
        dapl/common/dapl_rmr_bind.c                 \
        dapl/common/dapl_rmr_query.c                \
        dapl/common/dapl_rmr_util.c                 \
        dapl/common/dapl_rsp_create.c               \
        dapl/common/dapl_rsp_free.c                 \
        dapl/common/dapl_rsp_query.c                \
        dapl/common/dapl_cno_util.c                 \
        dapl/common/dapl_set_consumer_context.c     \
        dapl/common/dapl_ring_buffer_util.c         \
        dapl/common/dapl_name_service.c             \
        dapl/common/dapl_timer_util.c               \
        dapl/common/dapl_ep_create_with_srq.c       \
        dapl/common/dapl_ep_recv_query.c            \
        dapl/common/dapl_ep_set_watermark.c         \
        dapl/common/dapl_srq_create.c               \
        dapl/common/dapl_srq_free.c                 \
        dapl/common/dapl_srq_query.c                \
        dapl/common/dapl_srq_resize.c               \
        dapl/common/dapl_srq_post_recv.c            \
        dapl/common/dapl_srq_set_lw.c               \
        dapl/common/dapl_srq_util.c                 \
        dapl/common/dapl_debug.c                    \
        dapl/common/dapl_ia_ha.c                    \
        dapl/common/dapl_csp.c                      \
        dapl/common/dapl_ep_post_send_invalidate.c  \
        dapl/common/dapl_ep_post_rdma_read_to_rmr.c \
        dapl/openib_common/mem.c                    \
        dapl/openib_common/cq.c                     \
        dapl/openib_common/qp.c                     \
        dapl/openib_common/util.c                   \
        dapl/openib_ucm/cm.c                        \
        dapl/openib_ucm/device.c $(XPROGRAMS)

dapl_udapl_libdaploucm_la_LDFLAGS = -version-info 2:0:0 $(daploscm_version_script) \
                                   -Wl,-init,dapl_init -Wl,-fini,dapl_fini \
                                   -lpthread -libverbs

libdatincludedir = $(includedir)/dat2

libdatinclude_HEADERS = dat/include/dat2/dat.h \
			dat/include/dat2/dat_error.h \
			dat/include/dat2/dat_platform_specific.h \
			dat/include/dat2/dat_redirection.h \
			dat/include/dat2/dat_registry.h \
			dat/include/dat2/dat_vendor_specific.h \
			dat/include/dat2/udat_config.h \
			dat/include/dat2/udat.h \
			dat/include/dat2/udat_redirection.h \
			dat/include/dat2/udat_vendor_specific.h \
			dat/include/dat2/dat_ib_extensions.h 
		
man_MANS = man/dtest.1 man/dapltest.1 man/dat.conf.5 
    	
EXTRA_DIST = dat/common/dat_dictionary.h \
	     dat/common/dat_dr.h \
	     dat/common/dat_init.h \
	     dat/common/dat_sr.h \
	     dat/udat/udat_sr_parser.h \
	     dat/udat/linux/dat_osd.h \
	     dat/include/dat2/dat.h \
	     dat/include/dat2/dat_error.h \
	     dat/include/dat2/dat_platform_specific.h \
	     dat/include/dat2/dat_redirection.h \
	     dat/include/dat2/dat_registry.h \
	     dat/include/dat2/dat_vendor_specific.h \
	     dat/include/dat2/udat_config.h \
	     dat/include/dat2/udat.h \
	     dat/include/dat2/udat_redirection.h \
	     dat/include/dat2/udat_vendor_specific.h \
	     dapl/common/dapl_adapter_util.h \
	     dapl/common/dapl_cno_util.h \
	     dapl/common/dapl_cookie.h \
	     dapl/common/dapl_cr_util.h \
	     dapl/common/dapl_ep_util.h \
	     dapl/common/dapl_evd_util.h \
	     dapl/common/dapl_hash.h \
	     dapl/common/dapl_hca_util.h \
	     dapl/common/dapl_ia_util.h \
	     dapl/common/dapl_init.h \
	     dapl/common/dapl_lmr_util.h \
	     dapl/common/dapl_mr_util.h \
	     dapl/common/dapl_name_service.h \
	     dapl/common/dapl_provider.h \
	     dapl/common/dapl_pz_util.h \
	     dapl/common/dapl_ring_buffer_util.h \
	     dapl/common/dapl_rmr_util.h \
	     dapl/common/dapl_sp_util.h \
	     dapl/common/dapl_srq_util.h \
	     dapl/common/dapl_timer_util.h \
	     dapl/udapl/linux/dapl_osd.h \
	     dapl/include/dapl.h \
	     dapl/include/dapl_debug.h \
	     dapl/include/dapl_ipoib_names.h \
	     dapl/include/dapl_vendor.h \
	     dapl/openib_common/dapl_ib_dto.h \
	     dapl/openib_common/dapl_ib_common.h \
	     dapl/openib_cma/dapl_ib_util.h \
	     dapl/openib_cma/linux/openib_osd.h \
	     dapl/openib_scm/dapl_ib_util.h \
	     dapl/openib_scm/linux/openib_osd.h \
     	     dapl/openib_ucm/dapl_ib_util.h \
	     dapl/openib_ucm/linux/openib_osd.h \
	     dat/udat/libdat2.map \
	     dapl/udapl/libdaplofa.map \
	     dapl/udapl/libdaploscm.map \
	     dapl/udapl/libdaploucm.map \
	     dapl.spec.in \
	     $(man_MANS) \
	     test/dapltest/include/dapl_bpool.h \
	     test/dapltest/include/dapl_client_info.h \
	     test/dapltest/include/dapl_common.h \
	     test/dapltest/include/dapl_execute.h \
	     test/dapltest/include/dapl_fft_cmd.h \
	     test/dapltest/include/dapl_fft_util.h \
	     test/dapltest/include/dapl_getopt.h \
	     test/dapltest/include/dapl_global.h \
	     test/dapltest/include/dapl_limit_cmd.h \
	     test/dapltest/include/dapl_mdep.h \
	     test/dapltest/include/dapl_memlist.h \
	     test/dapltest/include/dapl_params.h \
	     test/dapltest/include/dapl_performance_cmd.h \
	     test/dapltest/include/dapl_performance_stats.h \
	     test/dapltest/include/dapl_performance_test.h \
	     test/dapltest/include/dapl_proto.h \
	     test/dapltest/include/dapl_quit_cmd.h \
	     test/dapltest/include/dapl_server_cmd.h \
	     test/dapltest/include/dapl_server_info.h \
	     test/dapltest/include/dapl_tdep.h \
	     test/dapltest/include/dapl_tdep_print.h \
	     test/dapltest/include/dapl_test_data.h \
	     test/dapltest/include/dapl_transaction_cmd.h \
	     test/dapltest/include/dapl_transaction_stats.h \
	     test/dapltest/include/dapl_transaction_test.h \
	     test/dapltest/include/dapl_version.h \
	     test/dapltest/mdep/linux/dapl_mdep_user.h
	 
dist-hook: dapl.spec 
	cp dapl.spec $(distdir)
	
install-exec-hook:
	if ! test -d $(DESTDIR)$(sysconfdir); then \
		mkdir -p $(DESTDIR)$(sysconfdir); \
	fi; \
	if test -e $(DESTDIR)$(sysconfdir)/dat.conf; then \
		sed -e '/ofa-v2-.* u2/d' < $(DESTDIR)$(sysconfdir)/dat.conf > /tmp/$$$$ofadapl; \
		cp /tmp/$$$$ofadapl $(DESTDIR)$(sysconfdir)/dat.conf; \
	fi; \
	echo ofa-v2-mlx4_0-1 u2.0 nonthreadsafe default libdaploscm.so.2 dapl.2.0 '"mlx4_0 1" ""' >> $(DESTDIR)$(sysconfdir)/dat.conf; \
	echo ofa-v2-mlx4_0-2 u2.0 nonthreadsafe default libdaploscm.so.2 dapl.2.0 '"mlx4_0 2" ""' >> $(DESTDIR)$(sysconfdir)/dat.conf; \
	echo ofa-v2-ib0 u2.0 nonthreadsafe default libdaplofa.so.2 dapl.2.0 '"ib0 0" ""' >> $(DESTDIR)$(sysconfdir)/dat.conf; \
	echo ofa-v2-ib1 u2.0 nonthreadsafe default libdaplofa.so.2 dapl.2.0 '"ib1 0" ""' >> $(DESTDIR)$(sysconfdir)/dat.conf; \
	echo ofa-v2-mthca0-1 u2.0 nonthreadsafe default libdaploscm.so.2 dapl.2.0 '"mthca0 1" ""' >> $(DESTDIR)$(sysconfdir)/dat.conf; \
	echo ofa-v2-mthca0-2 u2.0 nonthreadsafe default libdaploscm.so.2 dapl.2.0 '"mthca0 2" ""' >> $(DESTDIR)$(sysconfdir)/dat.conf; \
	echo ofa-v2-ipath0-1 u2.0 nonthreadsafe default libdaploscm.so.2 dapl.2.0 '"ipath0 1" ""' >> $(DESTDIR)$(sysconfdir)/dat.conf; \
	echo ofa-v2-ipath0-2 u2.0 nonthreadsafe default libdaploscm.so.2 dapl.2.0 '"ipath0 2" ""' >> $(DESTDIR)$(sysconfdir)/dat.conf; \
	echo ofa-v2-ehca0-2 u2.0 nonthreadsafe default libdaploscm.so.2 dapl.2.0 '"ehca0 1" ""' >> $(DESTDIR)$(sysconfdir)/dat.conf; \
	echo ofa-v2-iwarp u2.0 nonthreadsafe default libdaplofa.so.2 dapl.2.0 '"eth2 0" ""' >> $(DESTDIR)$(sysconfdir)/dat.conf; \
	echo ofa-v2-mlx4_0-1u u2.0 nonthreadsafe default libdaploucm.so.2 dapl.2.0 '"mlx4_0 1" ""' >> $(DESTDIR)$(sysconfdir)/dat.conf; \
	echo ofa-v2-mlx4_0-2u u2.0 nonthreadsafe default libdaploucm.so.2 dapl.2.0 '"mlx4_0 2" ""' >> $(DESTDIR)$(sysconfdir)/dat.conf; \
	echo ofa-v2-mthca0-1u u2.0 nonthreadsafe default libdaploucm.so.2 dapl.2.0 '"mthca0 1" ""' >> $(DESTDIR)$(sysconfdir)/dat.conf; \
	echo ofa-v2-mthca0-2u u2.0 nonthreadsafe default libdaploucm.so.2 dapl.2.0 '"mthca0 2" ""' >> $(DESTDIR)$(sysconfdir)/dat.conf;

uninstall-hook:
	if test -e $(DESTDIR)$(sysconfdir)/dat.conf; then \
		sed -e '/ofa-v2-.* u2/d' < $(DESTDIR)$(sysconfdir)/dat.conf > /tmp/$$$$ofadapl; \
		cp /tmp/$$$$ofadapl $(DESTDIR)$(sysconfdir)/dat.conf; \
	fi;

SUBDIRS = . test/dtest test/dapltest
