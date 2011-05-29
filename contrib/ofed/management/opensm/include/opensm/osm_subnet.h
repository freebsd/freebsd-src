/*
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2008 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 * Copyright (c) 2008 Xsigo Systems Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*
 * Abstract:
 *	Declaration of osm_subn_t.
 *	This object represents an IBA subnet.
 *	This object is part of the OpenSM family of objects.
 */

#ifndef _OSM_SUBNET_H_
#define _OSM_SUBNET_H_

#include <iba/ib_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_map.h>
#include <complib/cl_ptr_vector.h>
#include <complib/cl_list.h>
#include <opensm/osm_base.h>
#include <opensm/osm_prefix_route.h>
#include <stdio.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
#define OSM_SUBNET_VECTOR_MIN_SIZE			0
#define OSM_SUBNET_VECTOR_GROW_SIZE			1
#define OSM_SUBNET_VECTOR_CAPACITY			256
struct osm_opensm;
struct osm_qos_policy;

/****h* OpenSM/Subnet
* NAME
*	Subnet
*
* DESCRIPTION
*	The Subnet object encapsulates the information needed by the
*	OpenSM to manage a subnet.  The OpenSM allocates one Subnet object
*	per IBA subnet.
*
*	The Subnet object is not thread safe, thus callers must provide
*	serialization.
*
*	This object is essentially a container for the various components
*	of a subnet.  Callers may directly access the member variables.
*
* AUTHOR
*	Steve King, Intel
*
*********/

/****s* OpenSM: Subnet/osm_qos_options_t
* NAME
*	osm_qos_options_t
*
* DESCRIPTION
*	Subnet QoS options structure.  This structure contains the various
*	QoS specific configuration parameters for the subnet.
*
* SYNOPSIS
*/
typedef struct osm_qos_options {
	unsigned max_vls;
	int high_limit;
	char *vlarb_high;
	char *vlarb_low;
	char *sl2vl;
} osm_qos_options_t;
/*
* FIELDS
*
*	max_vls
*		The number of maximum VLs on the Subnet (0 == use default)
*
*	high_limit
*		The limit of High Priority component of VL Arbitration
*		table (IBA 7.6.9) (-1 == use default)
*
*	vlarb_high
*		High priority VL Arbitration table template. (NULL == use default)
*
*	vlarb_low
*		Low priority VL Arbitration table template. (NULL == use default)
*
*	sl2vl
*		SL2VL Mapping table (IBA 7.6.6) template. (NULL == use default)
*
*********/

/****s* OpenSM: Subnet/osm_subn_opt_t
* NAME
*	osm_subn_opt_t
*
* DESCRIPTION
*	Subnet options structure.  This structure contains the various
*	site specific configuration parameters for the subnet.
*
* SYNOPSIS
*/
typedef struct osm_subn_opt {
	char *config_file;
	ib_net64_t guid;
	ib_net64_t m_key;
	ib_net64_t sm_key;
	ib_net64_t sa_key;
	ib_net64_t subnet_prefix;
	ib_net16_t m_key_lease_period;
	uint32_t sweep_interval;
	uint32_t max_wire_smps;
	uint32_t transaction_timeout;
	uint8_t sm_priority;
	uint8_t lmc;
	boolean_t lmc_esp0;
	uint8_t max_op_vls;
	uint8_t force_link_speed;
	boolean_t reassign_lids;
	boolean_t ignore_other_sm;
	boolean_t single_thread;
	boolean_t disable_multicast;
	boolean_t force_log_flush;
	uint8_t subnet_timeout;
	uint8_t packet_life_time;
	uint8_t vl_stall_count;
	uint8_t leaf_vl_stall_count;
	uint8_t head_of_queue_lifetime;
	uint8_t leaf_head_of_queue_lifetime;
	uint8_t local_phy_errors_threshold;
	uint8_t overrun_errors_threshold;
	uint32_t sminfo_polling_timeout;
	uint32_t polling_retry_number;
	uint32_t max_msg_fifo_timeout;
	boolean_t force_heavy_sweep;
	uint8_t log_flags;
	char *dump_files_dir;
	char *log_file;
	unsigned long log_max_size;
	char *partition_config_file;
	boolean_t no_partition_enforcement;
	boolean_t qos;
	char *qos_policy_file;
	boolean_t accum_log_file;
	char *console;
	uint16_t console_port;
	char *port_prof_ignore_file;
	boolean_t port_profile_switch_nodes;
	boolean_t sweep_on_trap;
	char *routing_engine_names;
	boolean_t use_ucast_cache;
	boolean_t connect_roots;
	char *lid_matrix_dump_file;
	char *lfts_file;
	char *root_guid_file;
	char *cn_guid_file;
	char *ids_guid_file;
	char *guid_routing_order_file;
	char *sa_db_file;
	boolean_t exit_on_fatal;
	boolean_t honor_guid2lid_file;
	boolean_t daemon;
	boolean_t sm_inactive;
	boolean_t babbling_port_policy;
	osm_qos_options_t qos_options;
	osm_qos_options_t qos_ca_options;
	osm_qos_options_t qos_sw0_options;
	osm_qos_options_t qos_swe_options;
	osm_qos_options_t qos_rtr_options;
	boolean_t enable_quirks;
	boolean_t no_clients_rereg;
#ifdef ENABLE_OSM_PERF_MGR
	boolean_t perfmgr;
	boolean_t perfmgr_redir;
	uint16_t perfmgr_sweep_time_s;
	uint32_t perfmgr_max_outstanding_queries;
	char *event_db_dump_file;
#endif				/* ENABLE_OSM_PERF_MGR */
	char *event_plugin_name;
	char *node_name_map_name;
	char *prefix_routes_file;
	boolean_t consolidate_ipv6_snm_req;
} osm_subn_opt_t;
/*
* FIELDS
*
*	config_file
*		The name of the config file.
*
*	guid
*		The port guid that the SM is binding to.
*
*	m_key
*		M_Key value sent to all ports qualifying all Set(PortInfo).
*
*	sm_key
*		SM_Key value of the SM used for SM authentication.
*
*	sa_key
*		SM_Key value to qualify rcv SA queries as "trusted".
*
*	subnet_prefix
*		Subnet prefix used on this subnet.
*
*	m_key_lease_period
*		The lease period used for the M_Key on this subnet.
*
*	sweep_interval
*		The number of seconds between subnet sweeps.  A value of 0
*		disables sweeping.
*
*	max_wire_smps
*		The maximum number of SMPs sent in parallel.  Default is 4.
*
*	transaction_timeout
*		The maximum time in milliseconds allowed for a transaction
*		to complete.  Default is 200.
*
*	sm_priority
*		The priority of this SM as specified by the user.  This
*		value is made available in the SMInfo attribute.
*
*	lmc
*		The LMC value used on this subnet.
*
*	lmc_esp0
*		Whether LMC value used on subnet should be used for
*		enhanced switch port 0 or not.  If TRUE, it is used.
*		Otherwise (the default), LMC is set to 0 for ESP0.
*
*	max_op_vls
*		Limit the maximal operational VLs. default is 1.
*
*	reassign_lids
*		If TRUE cause all lids to be re-assigend.
*		Otherwise (the default),
*		OpenSM always tries to preserve as LIDs as much as possible.
*
*	ignore_other_sm_option
*		This flag is TRUE if other SMs on the subnet should be ignored.
*
*	disable_multicast
*		This flag is TRUE if OpenSM should disable multicast support.
*
*	max_msg_fifo_timeout
*		The maximal time a message can stay in the incoming message
*		queue. If there is more than one message in the queue and the
*		last message stayed in the queue more than this value the SA
*		request will be immediately returned with a BUSY status.
*
*	subnet_timeout
*		The subnet_timeout that will be set for all the ports in the
*		design SubnSet(PortInfo.vl_stall_life))
*
*	vl_stall_count
*		The number of sequential packets dropped that cause the port
*		to enter the VLStalled state.
*
*	leaf_vl_stall_count
*		The number of sequential packets dropped that cause the port
*		to enter the VLStalled state. This is for switch ports driving
*		a CA or router port.
*
*	head_of_queue_lifetime
*		The maximal time a packet can live at the head of a VL queue
*		on any port not driving a CA or router port.
*
*	leaf_head_of_queue_lifetime
*		The maximal time a packet can live at the head of a VL queue
*		on switch ports driving a CA or router.
*
*	local_phy_errors_threshold
*		Threshold of local phy errors for sending Trap 129
*
*	overrun_errors_threshold
*		Threshold of credits overrun errors for sending Trap 129
*
*	sminfo_polling_timeout
*		Specifies the polling timeout (in milliseconds) - the timeout
*		between one poll to another.
*
*	packet_life_time
*		The maximal time a packet can stay in a switch.
*		The value is send to all switches as
*		SubnSet(SwitchInfo.life_state)
*
*	dump_files_dir
*		The directory to be used for opensm-subnet.lst, opensm.fdbs,
*		opensm.mcfdbs, and default log file (the latter for Windows,
*		not Linux).
*
*	log_file
*		Name of the log file (or NULL) for stdout.
*
*	log_max_size
*		This option defines maximal log file size in MB. When
*		specified the log file will be truncated upon reaching
*		this limit.
*
*	qos
*		Boolean that specifies whether the OpenSM QoS functionality
*		should be off or on.
*
*	qos_policy_file
*		Name of the QoS policy file.
*
*	accum_log_file
*		If TRUE (default) - the log file will be accumulated.
*		If FALSE - the log file will be erased before starting
*		current opensm run.
*
*	port_prof_ignore_file
*		Name of file with port guids to be ignored by port profiling.
*
*	port_profile_switch_nodes
*		If TRUE will count the number of switch nodes routed through
*		the link. If FALSE - only CA/RT nodes are counted.
*
*	sweep_on_trap
*		Received traps will initiate a new sweep.
*
*	routing_engine_names
*		Name of routing engine(s) to use.
*
*	connect_roots
*		The option which will enforce root to root connectivity with
*		up/down routing engine (even if this violates "pure" deadlock
*		free up/down algorithm)
*
*	use_ucast_cache
*		When TRUE enables unicast routing cache.
*
*	lid_matrix_dump_file
*		Name of the lid matrix dump file from where switch
*		lid matrices (min hops tables) will be loaded
*
*	lfts_file
*		Name of the unicast LFTs routing file from where switch
*		forwarding tables will be loaded
*
*	root_guid_file
*		Name of the file that contains list of root guids that
*		will be used by fat-tree or up/dn routing (provided by User)
*
*	cn_guid_file
*		Name of the file that contains list of compute node guids that
*		will be used by fat-tree routing (provided by User)
*
*	ids_guid_file
*		Name of the file that contains list of ids which should be
*		used by Up/Down algorithm instead of node GUIDs
*
*	guid_routing_order_file
*		Name of the file that contains list of guids for routing order
*		that will be used by minhop and up/dn routing (provided by User).
*
*	sa_db_file
*		Name of the SA database file.
*
*	exit_on_fatal
*		If TRUE (default) - SM will exit on fatal subnet initialization
*		issues.
*		If FALSE - SM will not exit.
*		Fatal initialization issues:
*		a. SM recognizes 2 different nodes with the same guid, or
*		   12x link with lane reversal badly configured.
*
*	honor_guid2lid_file
*		Always honor the guid2lid file if it exists and is valid. This
*		means that the file will be honored when SM is coming out of
*		STANDBY. By default this is FALSE.
*
*	daemon
*		OpenSM will run in daemon mode.
*
*	sm_inactive
*		OpenSM will start with SM in not active state.
*
*	babbling_port_policy
*		OpenSM will enforce its "babbling" port policy.
*
*	perfmgr
*		Enable or disable the performance manager
*
*	perfmgr_redir
*		Enable or disable the saving of redirection by PerfMgr
*
*	perfmgr_sweep_time_s
*		Define the period (in seconds) of PerfMgr sweeps
*
*       event_db_dump_file
*               File to dump the event database to
*
*       event_db_plugin
*               Specify the name of the event plugin
*
*	qos_options
*		Default set of QoS options
*
*	qos_ca_options
*		QoS options for CA ports
*
*	qos_sw0_options
*		QoS options for switches' port 0
*
*	qos_swe_options
*		QoS options for switches' external ports
*
*	qos_rtr_options
*		QoS options for router ports
*
*	enable_quirks
*		Enable high risk new features and not fully qualified
*		hardware specific work arounds
*
*	no_clients_rereg
*		When TRUE disables clients reregistration request.
*
* SEE ALSO
*	Subnet object
*********/

/****s* OpenSM: Subnet/osm_subn_t
* NAME
*	osm_subn_t
*
* DESCRIPTION
*	Subnet structure.  Callers may directly access member components,
*	after grabbing a lock.
*
* TO DO
*	This structure should probably be volatile.
*
* SYNOPSIS
*/
typedef struct osm_subn {
	struct osm_opensm *p_osm;
	cl_qmap_t sw_guid_tbl;
	cl_qmap_t node_guid_tbl;
	cl_qmap_t port_guid_tbl;
	cl_qmap_t rtr_guid_tbl;
	cl_qlist_t prefix_routes_list;
	cl_qmap_t prtn_pkey_tbl;
	cl_qmap_t sm_guid_tbl;
	cl_qlist_t sa_sr_list;
	cl_qlist_t sa_infr_list;
	cl_ptr_vector_t port_lid_tbl;
	ib_net16_t master_sm_base_lid;
	ib_net16_t sm_base_lid;
	ib_net64_t sm_port_guid;
	uint8_t sm_state;
	osm_subn_opt_t opt;
	struct osm_qos_policy *p_qos_policy;
	uint16_t max_ucast_lid_ho;
	uint16_t max_mcast_lid_ho;
	uint8_t min_ca_mtu;
	uint8_t min_ca_rate;
	boolean_t ignore_existing_lfts;
	boolean_t subnet_initialization_error;
	boolean_t force_heavy_sweep;
	boolean_t force_reroute;
	boolean_t in_sweep_hop_0;
	boolean_t first_time_master_sweep;
	boolean_t coming_out_of_standby;
	unsigned need_update;
	void *mgroups[IB_LID_MCAST_END_HO - IB_LID_MCAST_START_HO + 1];
} osm_subn_t;
/*
* FIELDS
*	sw_guid_tbl
*		Container of pointers to all Switch objects in the subent.
*		Indexed by node GUID.
*
*	node_guid_tbl
*		Container of pointers to all Node objects in the subent.
*		Indexed by node GUID.
*
*	port_guid_tbl
*		Container of pointers to all Port objects in the subent.
*		Indexed by port GUID - network order!
*
*	rtr_guid_tbl
*		Container of pointers to all Router objects in the subent.
*		Indexed by node GUID.
*
*	prtn_pkey_tbl
*		Container of pointers to all Partition objects in the subnet.
*		Indexed by P_KEY.
*
*	sm_guid_tbl
*		Container of pointers to SM objects representing other SMs
*		on the subnet.
*
*	port_lid_tbl
*		Container of pointers to all Port objects in the subent.
*		Indexed by port LID.
*
*	master_sm_base_lid
*		The base LID owned by the subnet's master SM.
*
*	sm_base_lid
*		The base LID of the local port where the SM is.
*
*	sm_port_guid
*		This SM's own port GUID.
*
*	sm_state
*		The high-level state of the SM.  This value is made available
*		in the SMInfo attribute.
*
*	opt
*		Subnet options structure contains site specific configuration.
*
*	p_qos_policy
*		Subnet QoS policy structure.
*
*	max_ucast_lid_ho
*		The minimal max unicast lid reported by all switches
*
*	max_mcast_lid_ho
*		The minimal max multicast lid reported by all switches
*
*	min_ca_mtu
*		The minimal MTU reported by all CAs ports on the subnet
*
*	min_ca_rate
*		The minimal rate reported by all CA ports on the subnet
*
*	ignore_existing_lfts
*		This flag is a dynamic flag to instruct the LFT assignment to
*		ignore existing legal LFT settings.
*		The value will be set according to :
*		- Any change to the list of switches will set it to high
*		- Coming out of STANDBY it will be cleared (other SM worked)
*		- Set to FALSE upon end of all lft assignments.
*
*	subnet_initalization_error
*		Similar to the force_heavy_sweep flag. If TRUE - means that
*		we had errors during initialization (due to SubnSet requests
*		that failed). We want to declare the subnet as unhealthy, and
*		force another heavy sweep.
*
*	force_heavy_sweep
*		If TRUE - we want to force a heavy sweep. This can be done
*		either due to receiving of trap - meaning there is some change
*		on the subnet, or we received a handover from a remote sm.
*		In this case we want to sweep and reconfigure the entire
*		subnet. This will cause another heavy sweep to occure when
*		the current sweep is done.
*
*	force_reroute
*		If TRUE - we want to force switches in the fabric to be
*		rerouted.
*
*	in_sweep_hop_0
*		When in_sweep_hop_0 flag is set to TRUE - this means we are
*		in sweep_hop_0 - meaning we do not want to continue beyond
*		the current node.
*		This is relevant for the case of SM on switch, since in the
*		switch info we need to signal somehow not to continue
*		the sweeping.
*
*	first_time_master_sweep
*		This flag is used for the PortInfo setting. On the first
*		sweep as master (meaning after moving from Standby|Discovering
*		state), the SM must send a PortInfoSet to all ports. After
*		that - we want to minimize the number of PortInfoSet requests
*		sent, and to send only requests that change the value from
*		what is updated in the port (or send a first request if this
*		is a new port). We will set this flag to TRUE when entering
*		the master state, and set it back to FALSE at the end of the
*		drop manager. This is done since at the end of the drop manager
*		we have updated all the ports that are reachable, and from now
*		on these are the only ports we have data of. We don't want
*		to send extra set requests to these ports anymore.
*
*	coming_out_of_standby
*		TRUE on the first sweep after the SM was in standby.
*		Used for nulling any cache of LID and Routing.
*		The flag is set true if the SM state was standby and now
*		changed to MASTER it is reset at the end of the sweep.
*
*	need_update
*		This flag should be on during first non-master heavy
*		(including pre-master discovery stage)
*
*	mgroups
*		Array of pointers to all Multicast Group objects in the subnet.
*		Indexed by MLID offset from base MLID.
*
* SEE ALSO
*	Subnet object
*********/

/****f* OpenSM: Subnet/osm_subn_construct
* NAME
*	osm_subn_construct
*
* DESCRIPTION
*	This function constructs a Subnet object.
*
* SYNOPSIS
*/
void osm_subn_construct(IN osm_subn_t * const p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a Subnet object to construct.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Allows calling osm_subn_init, and osm_subn_destroy.
*
*	Calling osm_subn_construct is a prerequisite to calling any other
*	method except osm_subn_init.
*
* SEE ALSO
*	Subnet object, osm_subn_init, osm_subn_destroy
*********/

/****f* OpenSM: Subnet/osm_subn_destroy
* NAME
*	osm_subn_destroy
*
* DESCRIPTION
*	The osm_subn_destroy function destroys a subnet, releasing
*	all resources.
*
* SYNOPSIS
*/
void osm_subn_destroy(IN osm_subn_t * const p_subn);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to a Subnet object to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Performs any necessary cleanup of the specified Subnet object.
*	Further operations should not be attempted on the destroyed object.
*	This function should only be called after a call to osm_subn_construct
*	or osm_subn_init.
*
* SEE ALSO
*	Subnet object, osm_subn_construct, osm_subn_init
*********/

/****f* OpenSM: Subnet/osm_subn_init
* NAME
*	osm_subn_init
*
* DESCRIPTION
*	The osm_subn_init function initializes a Subnet object for use.
*
* SYNOPSIS
*/
ib_api_status_t
osm_subn_init(IN osm_subn_t * const p_subn,
	      IN struct osm_opensm *const p_osm,
	      IN const osm_subn_opt_t * const p_opt);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object to initialize.
*
*	p_opt
*		[in] Pointer to the subnet options structure.
*
* RETURN VALUES
*	IB_SUCCESS if the Subnet object was initialized successfully.
*
* NOTES
*	Allows calling other Subnet methods.
*
* SEE ALSO
*	Subnet object, osm_subn_construct, osm_subn_destroy
*********/

/*
  Forward references.
*/
struct osm_mad_addr;
struct osm_log;
struct osm_switch;
struct osm_physp;
struct osm_port;
struct osm_mgrp;

/****f* OpenSM: Helper/osm_get_gid_by_mad_addr
* NAME
*	osm_get_gid_by_mad_addr
*
* DESCRIPTION
*	Looks for the requester gid in the mad address.
*
* Note: This code is not thread safe. Need to grab the lock before
* calling it.
*
* SYNOPSIS
*/
ib_api_status_t
osm_get_gid_by_mad_addr(IN struct osm_log *p_log,
			IN const osm_subn_t * p_subn,
			IN const struct osm_mad_addr *p_mad_addr,
			OUT ib_gid_t * p_gid);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to a log object.
*
*	p_subn
*		[in] Pointer to subnet object.
*
*	p_mad_addr
*		[in] Pointer to mad address object.
*
*	p_gid
*		[out] Pointer to the GID structure to fill in.
*
* RETURN VALUES
*     IB_SUCCESS if able to find the GID by address given.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_get_physp_by_mad_addr
* NAME
*	osm_get_physp_by_mad_addr
*
* DESCRIPTION
*	Looks for the requester physical port in the mad address.
*
* Note: This code is not thread safe. Need to grab the lock before
* calling it.
*
* SYNOPSIS
*/
struct osm_physp *osm_get_physp_by_mad_addr(IN struct osm_log *p_log,
					     IN const osm_subn_t * p_subn,
					     IN struct osm_mad_addr
					     *p_mad_addr);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to a log object.
*
*	p_subn
*		[in] Pointer to subnet object.
*
*	p_mad_addr
*		[in] Pointer to mad address object.
*
* RETURN VALUES
*	Pointer to requester physical port object if found. Null otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_get_port_by_mad_addr
* NAME
*	osm_get_port_by_mad_addr
*
* DESCRIPTION
*	Looks for the requester port in the mad address.
*
* Note: This code is not thread safe. Need to grab the lock before
* calling it.
*
* SYNOPSIS
*/
struct osm_port *osm_get_port_by_mad_addr(IN struct osm_log *p_log,
					   IN const osm_subn_t * p_subn,
					   IN struct osm_mad_addr *p_mad_addr);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to a log object.
*
*	p_subn
*		[in] Pointer to subnet object.
*
*	p_mad_addr
*		[in] Pointer to mad address object.
*
* RETURN VALUES
*	Pointer to requester port object if found. Null otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Subnet/osm_get_switch_by_guid
* NAME
*	osm_get_switch_by_guid
*
* DESCRIPTION
*	Looks for the given switch guid in the subnet table of switches by guid.
*  NOTE: this code is not thread safe. Need to grab the lock before
*  calling it.
*
* SYNOPSIS
*/
struct osm_switch *osm_get_switch_by_guid(IN const osm_subn_t * p_subn,
					   IN uint64_t guid);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
*	guid
*		[in] The node guid in host order
*
* RETURN VALUES
*	The switch structure pointer if found. NULL otherwise.
*
* SEE ALSO
*	Subnet object, osm_subn_construct, osm_subn_destroy,
*	osm_switch_t
*********/

/****f* OpenSM: Subnet/osm_get_node_by_guid
* NAME
*	osm_get_node_by_guid
*
* DESCRIPTION
*	The looks for the given node giud in the subnet table of nodes by guid.
*  NOTE: this code is not thread safe. Need to grab the lock before
*  calling it.
*
* SYNOPSIS
*/
struct osm_node *osm_get_node_by_guid(IN osm_subn_t const *p_subn,
				       IN uint64_t guid);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
*	guid
*		[in] The node guid in host order
*
* RETURN VALUES
*	The node structure pointer if found. NULL otherwise.
*
* SEE ALSO
*	Subnet object, osm_subn_construct, osm_subn_destroy,
*	osm_node_t
*********/

/****f* OpenSM: Subnet/osm_get_port_by_guid
* NAME
*	osm_get_port_by_guid
*
* DESCRIPTION
*	The looks for the given port guid in the subnet table of ports by guid.
*  NOTE: this code is not thread safe. Need to grab the lock before
*  calling it.
*
* SYNOPSIS
*/
struct osm_port *osm_get_port_by_guid(IN osm_subn_t const *p_subn,
				       IN ib_net64_t guid);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
*	guid
*		[in] The port guid in network order
*
* RETURN VALUES
*	The port structure pointer if found. NULL otherwise.
*
* SEE ALSO
*	Subnet object, osm_subn_construct, osm_subn_destroy,
*	osm_port_t
*********/

/****f* OpenSM: Subnet/osm_get_mgrp_by_mlid
* NAME
*	osm_get_mgrp_by_mlid
*
* DESCRIPTION
*	The looks for the given multicast group in the subnet table by mlid.
*	NOTE: this code is not thread safe. Need to grab the lock before
*	calling it.
*
* SYNOPSIS
*/
static inline
struct osm_mgrp *osm_get_mgrp_by_mlid(osm_subn_t const *p_subn, ib_net16_t mlid)
{
	return p_subn->mgroups[cl_ntoh16(mlid) - IB_LID_MCAST_START_HO];
}
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to an osm_subn_t object
*
*	mlid
*		[in] The multicast group mlid in network order
*
* RETURN VALUES
*	The multicast group structure pointer if found. NULL otherwise.
*********/

/****f* OpenSM: Helper/osm_get_physp_by_mad_addr
* NAME
*	osm_get_physp_by_mad_addr
*
* DESCRIPTION
*	Looks for the requester physical port in the mad address.
*
* Note: This code is not thread safe. Need to grab the lock before
* calling it.
*
* SYNOPSIS
*/
struct osm_physp *osm_get_physp_by_mad_addr(IN struct osm_log *p_log,
					     IN const osm_subn_t * p_subn,
					     IN struct osm_mad_addr
					     *p_mad_addr);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to a log object.
*
*	p_subn
*		[in] Pointer to subnet object.
*
*	p_mad_addr
*		[in] Pointer to mad address object.
*
* RETURN VALUES
*	Pointer to requester physical port object if found. Null otherwise.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Subnet/osm_subn_set_default_opt
* NAME
*	osm_subn_set_default_opt
*
* DESCRIPTION
*	The osm_subn_set_default_opt function sets the default options.
*
* SYNOPSIS
*/
void osm_subn_set_default_opt(IN osm_subn_opt_t * const p_opt);
/*
* PARAMETERS
*
*	p_opt
*		[in] Pointer to the subnet options structure.
*
* RETURN VALUES
*	None
*
* NOTES
*
* SEE ALSO
*	Subnet object, osm_subn_construct, osm_subn_destroy
*********/

/****f* OpenSM: Subnet/osm_subn_parse_conf_file
* NAME
*	osm_subn_parse_conf_file
*
* DESCRIPTION
*	The osm_subn_parse_conf_file function parses the configuration file
*	and sets the defaults accordingly.
*
* SYNOPSIS
*/
int osm_subn_parse_conf_file(char *conf_file, osm_subn_opt_t * const p_opt);
/*
* PARAMETERS
*
*	p_opt
*		[in] Pointer to the subnet options structure.
*
* RETURN VALUES
*	0 on success, positive value if file doesn't exist,
*	negative value otherwise
*********/

/****f* OpenSM: Subnet/osm_subn_rescan_conf_files
* NAME
*	osm_subn_rescan_conf_files
*
* DESCRIPTION
*	The osm_subn_rescan_conf_files function parses the configuration
*	files and update selected subnet options
*
* SYNOPSIS
*/
int osm_subn_rescan_conf_files(IN osm_subn_t * const p_subn);
/*
* PARAMETERS
*
*	p_subn
*		[in] Pointer to the subnet structure.
*
* RETURN VALUES
*	0 on success, positive value if file doesn't exist,
*	negative value otherwise
*
*********/

/****f* OpenSM: Subnet/osm_subn_output_conf
* NAME
*	osm_subn_output_conf
*
* DESCRIPTION
*	Output configuration info
*
* SYNOPSIS
*/
int osm_subn_output_conf(FILE *out, IN osm_subn_opt_t * const p_opt);
/*
* PARAMETERS
*
*	out
*		[in] File stream to output to.
*
*	p_opt
*		[in] Pointer to the subnet options structure.
*
* RETURN VALUES
*	0 on success, negative value otherwise
*********/

/****f* OpenSM: Subnet/osm_subn_write_conf_file
* NAME
*	osm_subn_write_conf_file
*
* DESCRIPTION
*	Write the configuration file into the cache
*
* SYNOPSIS
*/
int osm_subn_write_conf_file(char *file_name, IN osm_subn_opt_t * const p_opt);
/*
* PARAMETERS
*
*	p_opt
*		[in] Pointer to the subnet options structure.
*
* RETURN VALUES
*	0 on success, negative value otherwise
*
* NOTES
*	Assumes the conf file is part of the cache dir which defaults to
*	OSM_DEFAULT_CACHE_DIR or OSM_CACHE_DIR the name is opensm.opts
*********/
int osm_subn_verify_config(osm_subn_opt_t * const p_opt);

END_C_DECLS
#endif				/* _OSM_SUBNET_H_ */
