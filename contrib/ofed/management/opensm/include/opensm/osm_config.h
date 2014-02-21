/* include/opensm/osm_config.h.  Generated from osm_config.h.in by configure.  */
/* include/osm_config.h.in
 *
 * Defines various OpenSM configuration parameters to be used by various
 * plugins and third party tools.
 *
 * NOTE: Defines used in header files MUST be included here to ensure plugin
 * compatibility.
 */

#ifndef _OSM_CONFIG_H_
#define _OSM_CONFIG_H_ 1

/* define 1 if OpenSM build is in a debug mode */
/* #undef OSM_DEBUG */

/* Define as 1 if you want Dual Sided RMPP Support */
#define DUAL_SIDED_RMPP 1

/* Define as 1 if you want to enable a console on a socket connection */
/* #undef ENABLE_OSM_CONSOLE_SOCKET */

/* Define as 1 if you want to enable the event plugin */
/* #undef ENABLE_OSM_DEFAULT_EVENT_PLUGIN */

/* Define as 1 if you want to enable the performance manager */
/* #undef ENABLE_OSM_PERF_MGR */

/* Define as 1 if you want to enable the performance manager profiling code */
/* #undef ENABLE_OSM_PERF_MGR_PROFILE */

/* Define a default node name map file */
#define HAVE_DEFAULT_NODENAME_MAP "/usr/local/etc/opensm/ib-node-name-map"

/* Define a default OpenSM config file */
#define HAVE_DEFAULT_OPENSM_CONFIG_FILE "/usr/local/etc/opensm/opensm.conf"

/* Define a Partition config file */
#define HAVE_DEFAULT_PARTITION_CONFIG_FILE "/usr/local/etc/opensm/partitions.conf"

/* Define a Prefix Routes config file */
#define HAVE_DEFAULT_PREFIX_ROUTES_FILE "/usr/local/etc/opensm/prefix-routes.conf"

/* Define a QOS policy config file */
#define HAVE_DEFAULT_QOS_POLICY_FILE "/usr/local/etc/opensm/qos-policy.conf"

/* Define OpenSM config directory */
#define OPENSM_CONFIG_DIR "/usr/local/etc/opensm"

/* Define as 1 for vapi vendor */
/* #undef OSM_VENDOR_INTF_MTL */

/* Define as 1 for OpenIB vendor */
#define OSM_VENDOR_INTF_OPENIB 1

/* Define as 1 for sim vendor */
/* #undef OSM_VENDOR_INTF_SIM */

/* Define as 1 for ts vendor */
/* #undef OSM_VENDOR_INTF_TS */

/* Define as 1 if you want Vendor RMPP Support */
#define VENDOR_RMPP_SUPPORT 1

#endif /* _OSM_CONFIG_H_ */
