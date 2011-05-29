.include <bsd.own.mk>

INCS=
IBINCS=		byteorder.h byteswap.h endian.h types.h
IBINCSDIR=	${INCLUDEDIR}/infiniband

IBVERBS=	${.CURDIR}/../../libibverbs/include/infiniband
VERBINCS=	${IBVERBS}/arch.h ${IBVERBS}/driver.h ${IBVERBS}/kern-abi.h
VERBINCS+=	${IBVERBS}/marshall.h ${IBVERBS}/opcode.h
VERBINCS+=	${IBVERBS}/sa-kern-abi.h ${IBVERBS}/sa.h ${IBVERBS}/verbs.h
VERBINCSDIR=	${INCLUDEDIR}/infiniband

IBCOMMON=	${.CURDIR}/../../management/libibcommon/include/infiniband
COMMONINCS=	${IBCOMMON}/common.h
COMMONINCSDIR=	${INCLUDEDIR}/infiniband

IBMAD=		${.CURDIR}/../../management/libibmad/include/infiniband
MADINCS=	${IBMAD}/mad.h
MADINCSDIR=	${INCLUDEDIR}/infiniband

IBUMAD=		${.CURDIR}/../../management/libibumad/include/infiniband
UMADINCS=	${IBUMAD}/umad.h
UMADINCSDIR=	${INCLUDEDIR}/infiniband

COMPLIB=	${.CURDIR}/../../management/opensm/include/complib
COMPLIBINCS=	${COMPLIB}/cl_atomic.h ${COMPLIB}/cl_atomic_osd.h
COMPLIBINCS+= 	${COMPLIB}/cl_byteswap.h ${COMPLIB}/cl_byteswap_osd.h
COMPLIBINCS+= 	${COMPLIB}/cl_comppool.h ${COMPLIB}/cl_debug.h
COMPLIBINCS+=	${COMPLIB}/cl_debug_osd.h ${COMPLIB}/cl_dispatcher.h
COMPLIBINCS+=	${COMPLIB}/cl_event.h ${COMPLIB}/cl_event_osd.h
COMPLIBINCS+=	${COMPLIB}/cl_event_wheel.h ${COMPLIB}/cl_fleximap.h
COMPLIBINCS+=	${COMPLIB}/cl_list.h ${COMPLIB}/cl_log.h
COMPLIBINCS+=	${COMPLIB}/cl_map.h ${COMPLIB}/cl_math.h
COMPLIBINCS+=	${COMPLIB}/cl_nodenamemap.h ${COMPLIB}/cl_packoff.h
COMPLIBINCS+=	${COMPLIB}/cl_packon.h ${COMPLIB}/cl_passivelock.h
COMPLIBINCS+=	${COMPLIB}/cl_pool.h ${COMPLIB}/cl_ptr_vector.h
COMPLIBINCS+=	${COMPLIB}/cl_qcomppool.h ${COMPLIB}/cl_qlist.h
COMPLIBINCS+=	${COMPLIB}/cl_qmap.h ${COMPLIB}/cl_qpool.h
COMPLIBINCS+=	${COMPLIB}/cl_spinlock.h ${COMPLIB}/cl_spinlock_osd.h
COMPLIBINCS+=	${COMPLIB}/cl_thread.h ${COMPLIB}/cl_thread_osd.h
COMPLIBINCS+=	${COMPLIB}/cl_threadpool.h ${COMPLIB}/cl_timer.h
COMPLIBINCS+=	${COMPLIB}/cl_timer_osd.h ${COMPLIB}/cl_types.h
COMPLIBINCS+=	${COMPLIB}/cl_types_osd.h ${COMPLIB}/cl_vector.h
COMPLIBINCSDIR=	${INCLUDEDIR}/infiniband/complib

IBADIR=		${.CURDIR}/../../management/opensm/include/iba
IBAINCS=	${IBADIR}/ib_cm_types.h ${IBADIR}/ib_types.h
IBAINCSDIR=	${INCLUDEDIR}/infiniband/iba

OPENSM=		${.CURDIR}/../../management/opensm/include/opensm

OPENSMINCS=	${OPENSM}/osm_attrib_req.h ${OPENSM}/osm_base.h
OPENSMINCS+=	${OPENSM}/osm_config.h ${OPENSM}/osm_console.h
OPENSMINCS+=	${OPENSM}/osm_console_io.h ${OPENSM}/osm_db.h
OPENSMINCS+=	${OPENSM}/osm_db_pack.h ${OPENSM}/osm_errors.h
OPENSMINCS+=	${OPENSM}/osm_event_plugin.h ${OPENSM}/osm_helper.h
OPENSMINCS+=	${OPENSM}/osm_inform.h ${OPENSM}/osm_lid_mgr.h
OPENSMINCS+=	${OPENSM}/osm_log.h ${OPENSM}/osm_mad_pool.h
OPENSMINCS+=	${OPENSM}/osm_madw.h ${OPENSM}/osm_mcast_tbl.h
OPENSMINCS+=	${OPENSM}/osm_mcm_info.h ${OPENSM}/osm_mcm_port.h
OPENSMINCS+=	${OPENSM}/osm_msgdef.h ${OPENSM}/osm_mtree.h
OPENSMINCS+=	${OPENSM}/osm_multicast.h ${OPENSM}/osm_node.h
OPENSMINCS+=	${OPENSM}/osm_opensm.h ${OPENSM}/osm_partition.h
OPENSMINCS+=	${OPENSM}/osm_path.h ${OPENSM}/osm_perfmgr.h
OPENSMINCS+=	${OPENSM}/osm_perfmgr_db.h ${OPENSM}/osm_pkey.h
OPENSMINCS+=	${OPENSM}/osm_pkey_mgr.h ${OPENSM}/osm_port.h
OPENSMINCS+=	${OPENSM}/osm_port_profile.h ${OPENSM}/osm_prefix_route.h
OPENSMINCS+=	${OPENSM}/osm_qos_policy.h ${OPENSM}/osm_remote_sm.h
OPENSMINCS+=	${OPENSM}/osm_router.h ${OPENSM}/osm_sa.h
OPENSMINCS+=	${OPENSM}/osm_sa_mad_ctrl.h ${OPENSM}/osm_service.h
OPENSMINCS+=	${OPENSM}/osm_sm.h ${OPENSM}/osm_sm_mad_ctrl.h
OPENSMINCS+=	${OPENSM}/osm_stats.h ${OPENSM}/osm_subnet.h
OPENSMINCS+=	${OPENSM}/osm_switch.h ${OPENSM}/osm_ucast_cache.h
OPENSMINCS+=	${OPENSM}/osm_ucast_mgr.h ${OPENSM}/osm_version.h
OPENSMINCS+=	${OPENSM}/osm_vl15intf.h ${OPENSM}/st.h
OPENSMINCSDIR=	${INCLUDEDIR}/infiniband/opensm

VENDOR=		${.CURDIR}/../../management/opensm/include/vendor
VENDORINCS=	${VENDOR}/osm_mtl_bind.h ${VENDOR}/osm_pkt_randomizer.h
VENDORINCS+=	${VENDOR}/osm_ts_useraccess.h ${VENDOR}/osm_umadt.h
VENDORINCS+=	${VENDOR}/osm_vendor.h ${VENDOR}/osm_vendor_al.h
VENDORINCS+=	${VENDOR}/osm_vendor_api.h ${VENDOR}/osm_vendor_ibumad.h
VENDORINCS+=	${VENDOR}/osm_vendor_mlx.h ${VENDOR}/osm_vendor_mlx_defs.h
VENDORINCS+=	${VENDOR}/osm_vendor_mlx_dispatcher.h
VENDORINCS+=	${VENDOR}/osm_vendor_mlx_hca.h
VENDORINCS+=	${VENDOR}/osm_vendor_mlx_inout.h
VENDORINCS+=	${VENDOR}/osm_vendor_mlx_rmpp_ctx.h
VENDORINCS+=	${VENDOR}/osm_vendor_mlx_sar.h ${VENDOR}/osm_vendor_mlx_sender.h
VENDORINCS+=	${VENDOR}/osm_vendor_mlx_svc.h
VENDORINCS+=	${VENDOR}/osm_vendor_mlx_transport.h
VENDORINCS+=	${VENDOR}/osm_vendor_mlx_transport_anafa.h
VENDORINCS+=	${VENDOR}/osm_vendor_mlx_txn.h
VENDORINCS+=	${VENDOR}/osm_vendor_mtl.h ${VENDOR}/osm_vendor_mtl_hca_guid.h
VENDORINCS+=	${VENDOR}/osm_vendor_mtl_transaction_mgr.h
VENDORINCS+=	${VENDOR}/osm_vendor_sa_api.h
VENDORINCS+=	${VENDOR}/osm_vendor_test.h ${VENDOR}/osm_vendor_ts.h
VENDORINCS+=	${VENDOR}/osm_vendor_umadt.h
VENDORINCSDIR=	${INCLUDEDIR}/infiniband/vendor

IBCM=		${.CURDIR}/../../libibcm/include/infiniband
IBCMINCS=	${IBCM}/cm.h ${IBCM}/cm_abi.h
IBCMINCSDIR=	${INCLUDEDIR}/infiniband

INCSGROUPS=	INCS VERBINCS COMMONINCS MADINCS UMADINCS COMPLIBINCS IBAINCS
INCSGROUPS+=	OPENSMINCS VENDORINCS IBCMINCS IBINCS

.include <bsd.prog.mk>
