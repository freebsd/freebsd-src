# $FreeBSD$

.include "../Makefile.inc"

OPENSM	= ${.CURDIR}/../../management/opensm
.PATH: ${OPENSM}/opensm ${OPENSM}/man

PROG=   opensm

SRCS=	main.c osm_console_io.c osm_console.c osm_db_files.c
SRCS+=	osm_db_pack.c osm_drop_mgr.c osm_inform.c osm_lid_mgr.c
SRCS+=	osm_lin_fwd_rcv.c osm_link_mgr.c osm_mcast_fwd_rcv.c osm_mcast_mgr.c
SRCS+=	osm_mcast_tbl.c osm_mcm_info.c osm_mcm_port.c osm_mtree.c
SRCS+=	osm_multicast.c osm_node.c osm_node_desc_rcv.c osm_node_info_rcv.c
SRCS+=	osm_opensm.c osm_pkey.c osm_pkey_mgr.c osm_pkey_rcv.c osm_port.c
SRCS+=	osm_port_info_rcv.c osm_remote_sm.c osm_req.c osm_resp.c osm_sa.c
SRCS+=	osm_sa_class_port_info.c osm_sa_informinfo.c osm_sa_lft_record.c
SRCS+=	osm_sa_mft_record.c osm_sa_link_record.c osm_sa_mad_ctrl.c
SRCS+=	osm_sa_mcmember_record.c osm_sa_node_record.c osm_sa_path_record.c
SRCS+=	osm_sa_pkey_record.c osm_sa_portinfo_record.c osm_sa_guidinfo_record.c
SRCS+=	osm_sa_multipath_record.c osm_sa_service_record.c osm_sa_slvl_record.c
SRCS+=	osm_sa_sminfo_record.c osm_sa_vlarb_record.c osm_sa_sw_info_record.c
SRCS+=	osm_service.c osm_slvl_map_rcv.c osm_sm.c osm_sminfo_rcv.c
SRCS+=	osm_sm_mad_ctrl.c osm_sm_state_mgr.c osm_state_mgr.c osm_subnet.c
SRCS+=	osm_sw_info_rcv.c osm_switch.c osm_prtn.c osm_prtn_config.c osm_qos.c
SRCS+=	osm_router.c osm_trap_rcv.c osm_ucast_mgr.c osm_ucast_updn.c
SRCS+=	osm_ucast_lash.c osm_ucast_file.c osm_ucast_ftree.c osm_vl15intf.c
SRCS+=	osm_vl_arb_rcv.c st.c osm_perfmgr.c osm_perfmgr_db.c osm_event_plugin.c
SRCS+=	osm_dump.c osm_ucast_cache.c osm_qos_parser_y.y osm_qos_parser_l.l
SRCS+=	osm_qos_policy.c

LDADD=	-lopensm -losmvendor -losmcomp -libmad -libumad -libcommon
CFLAGS+= -I/usr/include/infiniband
CFLAGS+= -pthread
CFLAGS+= -DVENDOR_RMPP_SUPPORT -DDUAL_SIDED_RMPP

MAN=	opensm.8

WARNS?= 1

.include <bsd.prog.mk>
