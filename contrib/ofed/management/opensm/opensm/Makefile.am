
INCLUDES = $(OSMV_INCLUDES)

AM_CFLAGS = -Wall $(DBGFLAGS) -D_XOPEN_SOURCE=600 -D_BSD_SOURCE=1

lib_LTLIBRARIES = libopensm.la

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

if HAVE_LD_VERSION_SCRIPT
libopensm_version_script = -Wl,--version-script=$(srcdir)/libopensm.map
else
libopensm_version_script =
endif

opensm_api_version=$(shell grep LIBVERSION= $(srcdir)/libopensm.ver | sed 's/LIBVERSION=//')

libopensm_la_SOURCES = osm_log.c osm_mad_pool.c osm_helper.c
libopensm_la_LDFLAGS = -version-info $(opensm_api_version) \
	-export-dynamic $(libopensm_version_script)
libopensm_la_DEPENDENCIES = $(srcdir)/libopensm.map

sbin_PROGRAMS = opensm
opensm_DEPENDENCIES = libopensm.la
opensm_SOURCES = main.c osm_console_io.c osm_console.c osm_db_files.c \
		 osm_db_pack.c osm_drop_mgr.c \
		 osm_inform.c osm_lid_mgr.c osm_lin_fwd_rcv.c \
		 osm_link_mgr.c osm_mcast_fwd_rcv.c \
		 osm_mcast_mgr.c osm_mcast_tbl.c osm_mcm_info.c \
		 osm_mcm_port.c osm_mtree.c osm_multicast.c osm_node.c \
		 osm_node_desc_rcv.c osm_node_info_rcv.c \
		 osm_opensm.c osm_pkey.c osm_pkey_mgr.c osm_pkey_rcv.c \
		 osm_port.c osm_port_info_rcv.c \
		 osm_remote_sm.c osm_req.c \
		 osm_resp.c osm_sa.c osm_sa_class_port_info.c \
		 osm_sa_informinfo.c osm_sa_lft_record.c osm_sa_mft_record.c \
		 osm_sa_link_record.c osm_sa_mad_ctrl.c \
		 osm_sa_mcmember_record.c osm_sa_node_record.c \
		 osm_sa_path_record.c osm_sa_pkey_record.c \
		 osm_sa_portinfo_record.c osm_sa_guidinfo_record.c \
		 osm_sa_multipath_record.c \
		 osm_sa_service_record.c osm_sa_slvl_record.c \
		 osm_sa_sminfo_record.c osm_sa_vlarb_record.c \
		 osm_sa_sw_info_record.c osm_service.c \
		 osm_slvl_map_rcv.c osm_sm.c osm_sminfo_rcv.c \
		 osm_sm_mad_ctrl.c osm_sm_state_mgr.c osm_state_mgr.c \
		 osm_subnet.c osm_sw_info_rcv.c osm_switch.c \
		 osm_prtn.c osm_prtn_config.c osm_qos.c osm_router.c \
		 osm_trap_rcv.c osm_ucast_mgr.c osm_ucast_updn.c \
		 osm_ucast_lash.c osm_ucast_file.c osm_ucast_ftree.c \
		 osm_vl15intf.c osm_vl_arb_rcv.c \
		 st.c osm_perfmgr.c osm_perfmgr_db.c \
		 osm_event_plugin.c osm_dump.c osm_ucast_cache.c \
		 osm_qos_parser_y.y osm_qos_parser_l.l osm_qos_policy.c

AM_YFLAGS:= -d

# we need to be able to load libraries from local build subtree before make install
# we always give precedence to local tree libs and then use the pre-installed ones.
opensm_LDADD = -L../complib -losmcomp -L../libvendor -losmvendor -L. -lopensm $(OSMV_LDADD)

opensmincludedir = $(includedir)/infiniband/opensm

opensminclude_HEADERS = \
	$(srcdir)/../include/opensm/osm_attrib_req.h \
	$(srcdir)/../include/opensm/osm_base.h \
	$(srcdir)/../include/opensm/osm_console.h \
	$(srcdir)/../include/opensm/osm_console_io.h \
	$(srcdir)/../include/opensm/osm_db.h \
	$(srcdir)/../include/opensm/osm_db_pack.h \
	$(srcdir)/../include/opensm/osm_event_plugin.h \
	$(srcdir)/../include/opensm/osm_errors.h \
	$(srcdir)/../include/opensm/osm_helper.h \
	$(srcdir)/../include/opensm/osm_inform.h \
	$(srcdir)/../include/opensm/osm_lid_mgr.h \
	$(srcdir)/../include/opensm/osm_log.h \
	$(srcdir)/../include/opensm/osm_mad_pool.h \
	$(srcdir)/../include/opensm/osm_madw.h \
	$(srcdir)/../include/opensm/osm_mcast_tbl.h \
	$(srcdir)/../include/opensm/osm_mcm_info.h \
	$(srcdir)/../include/opensm/osm_mcm_port.h \
	$(srcdir)/../include/opensm/osm_mtree.h \
	$(srcdir)/../include/opensm/osm_multicast.h \
	$(srcdir)/../include/opensm/osm_msgdef.h \
	$(srcdir)/../include/opensm/osm_node.h \
	$(srcdir)/../include/opensm/osm_opensm.h \
	$(srcdir)/../include/opensm/osm_partition.h \
	$(srcdir)/../include/opensm/osm_path.h \
	$(srcdir)/../include/opensm/osm_perfmgr.h \
	$(srcdir)/../include/opensm/osm_perfmgr_db.h \
	$(srcdir)/../include/opensm/osm_pkey.h \
	$(srcdir)/../include/opensm/osm_port.h \
	$(srcdir)/../include/opensm/osm_port_profile.h \
	$(srcdir)/../include/opensm/osm_prefix_route.h \
	$(srcdir)/../include/opensm/osm_qos_policy.h \
	$(srcdir)/../include/opensm/osm_remote_sm.h \
	$(srcdir)/../include/opensm/osm_router.h \
	$(srcdir)/../include/opensm/osm_sa.h \
	$(srcdir)/../include/opensm/osm_sa_mad_ctrl.h \
	$(srcdir)/../include/opensm/osm_service.h \
	$(srcdir)/../include/opensm/osm_sm.h \
	$(srcdir)/../include/opensm/osm_sm_mad_ctrl.h \
	$(srcdir)/../include/opensm/st.h \
	$(srcdir)/../include/opensm/osm_stats.h \
	$(srcdir)/../include/opensm/osm_subnet.h \
	$(srcdir)/../include/opensm/osm_switch.h \
	$(srcdir)/../include/opensm/osm_ucast_mgr.h \
	$(srcdir)/../include/opensm/osm_ucast_cache.h \
	$(srcdir)/../include/opensm/osm_vl15intf.h \
	$(top_builddir)/include/opensm/osm_version.h \
	$(top_builddir)/include/opensm/osm_config.h

BUILT_SOURCES = osm_version osm_qos_parser_y.h
osm_version:
	if [ -x $(top_srcdir)/../gen_ver.sh ] ; then \
		ver_file=$(top_builddir)/include/opensm/osm_version.h ; \
		osm_ver=`cat $$ver_file | sed -ne '/#define OSM_VERSION /s/^.*\"OpenSM \(.*\)\"$$/\1/p'` ; \
		ver=`$(top_srcdir)/../gen_ver.sh $(PACKAGE)` ; \
		if [ $$ver != $$osm_ver ] ; then \
			cat $$ver_file | sed -e '/#define OSM_VERSION /s/\"OpenSM .*\"/\"OpenSM '$$ver'\"/' > tmp_new_version ; \
			cat tmp_new_version > $$ver_file && rm -f tmp_new_version ; \
		fi ; \
	fi

# files distributed as part of the srcdir
EXTRA_DIST = $(srcdir)/libopensm.map $(srcdir)/libopensm.ver \
	     $(srcdir)/ChangeLog
