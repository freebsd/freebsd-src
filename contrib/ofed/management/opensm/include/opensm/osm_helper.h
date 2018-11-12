/*
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
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

#ifndef _OSM_HELPER_H_
#define _OSM_HELPER_H_

#include <iba/ib_types.h>
#include <complib/cl_dispatcher.h>
#include <opensm/osm_base.h>
#include <opensm/osm_log.h>
#include <opensm/osm_msgdef.h>
#include <opensm/osm_path.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/*
 * Abstract:
 * 	Declaration of helpful functions.
 */
/****f* OpenSM: Helper/ib_get_sa_method_str
 * NAME
 *	ib_get_sa_method_str
 *
 * DESCRIPTION
 *	Returns a string for the specified SA Method value.
 *
 * SYNOPSIS
 */
const char *ib_get_sa_method_str(IN uint8_t method);
/*
 * PARAMETERS
 *	method
 *		[in] Network order METHOD ID value.
 *
 * RETURN VALUES
 *	Pointer to the method string.
 *
 * NOTES
 *
 * SEE ALSO
 *********/

/****f* OpenSM: Helper/ib_get_sm_method_str
* NAME
*	ib_get_sm_method_str
*
* DESCRIPTION
*	Returns a string for the specified SM Method value.
*
* SYNOPSIS
*/
const char *ib_get_sm_method_str(IN uint8_t method);
/*
* PARAMETERS
*	method
*		[in] Network order METHOD ID value.
*
* RETURN VALUES
*	Pointer to the method string.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/ib_get_sm_attr_str
* NAME
*	ib_get_sm_attr_str
*
* DESCRIPTION
*	Returns a string for the specified SM attribute value.
*
* SYNOPSIS
*/
const char *ib_get_sm_attr_str(IN ib_net16_t attr);
/*
* PARAMETERS
*	attr
*		[in] Network order attribute ID value.
*
* RETURN VALUES
*	Pointer to the attribute string.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/ib_get_sa_attr_str
* NAME
*	ib_get_sa_attr_str
*
* DESCRIPTION
*	Returns a string for the specified SA attribute value.
*
* SYNOPSIS
*/
const char *ib_get_sa_attr_str(IN ib_net16_t attr);
/*
* PARAMETERS
*	attr
*		[in] Network order attribute ID value.
*
* RETURN VALUES
*	Pointer to the attribute string.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/ib_get_trap_str
* NAME
*	ib_get_trap_str
*
* DESCRIPTION
*	Returns a name for the specified trap.
*
* SYNOPSIS
*/
const char *ib_get_trap_str(uint16_t trap_num);
/*
* PARAMETERS
*	trap_num
*		[in] Network order trap number.
*
* RETURN VALUES
*	Name of the trap.
*
*********/

/****f* OpenSM: Helper/osm_dump_port_info
* NAME
*	osm_dump_port_info
*
* DESCRIPTION
*	Dumps the PortInfo attribute to the log.
*
* SYNOPSIS
*/
void osm_dump_port_info(IN osm_log_t * const p_log,
			IN const ib_net64_t node_guid,
			IN const ib_net64_t port_guid,
			IN const uint8_t port_num,
			IN const ib_port_info_t * const p_pi,
			IN const osm_log_level_t log_level);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to the osm_log_t object
*
*	node_guid
*		[in] Node GUID that owns this port.
*
*	port_guid
*		[in] Port GUID for this port.
*
*	port_num
*		[in] Port number for this port.
*
*	p_pi
*		[in] Pointer to the PortInfo attribute
*
*	log_level
*		[in] Log verbosity level with which to dump the data.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

void
osm_dump_path_record(IN osm_log_t * const p_log,
		     IN const ib_path_rec_t * const p_pr,
		     IN const osm_log_level_t log_level);

void
osm_dump_multipath_record(IN osm_log_t * const p_log,
			  IN const ib_multipath_rec_t * const p_mpr,
			  IN const osm_log_level_t log_level);

void
osm_dump_node_record(IN osm_log_t * const p_log,
		     IN const ib_node_record_t * const p_nr,
		     IN const osm_log_level_t log_level);

void
osm_dump_mc_record(IN osm_log_t * const p_log,
		   IN const ib_member_rec_t * const p_mcmr,
		   IN const osm_log_level_t log_level);

void
osm_dump_link_record(IN osm_log_t * const p_log,
		     IN const ib_link_record_t * const p_lr,
		     IN const osm_log_level_t log_level);

void
osm_dump_service_record(IN osm_log_t * const p_log,
			IN const ib_service_record_t * const p_sr,
			IN const osm_log_level_t log_level);

void
osm_dump_portinfo_record(IN osm_log_t * const p_log,
			 IN const ib_portinfo_record_t * const p_pir,
			 IN const osm_log_level_t log_level);

void
osm_dump_guidinfo_record(IN osm_log_t * const p_log,
			 IN const ib_guidinfo_record_t * const p_gir,
			 IN const osm_log_level_t log_level);

void
osm_dump_inform_info(IN osm_log_t * const p_log,
		     IN const ib_inform_info_t * const p_ii,
		     IN const osm_log_level_t log_level);

void
osm_dump_inform_info_record(IN osm_log_t * const p_log,
			    IN const ib_inform_info_record_t * const p_iir,
			    IN const osm_log_level_t log_level);

void
osm_dump_switch_info_record(IN osm_log_t * const p_log,
			    IN const ib_switch_info_record_t * const p_sir,
			    IN const osm_log_level_t log_level);

void
osm_dump_sm_info_record(IN osm_log_t * const p_log,
			IN const ib_sminfo_record_t * const p_smir,
			IN const osm_log_level_t log_level);

void
osm_dump_pkey_block(IN osm_log_t * const p_log,
		    IN uint64_t port_guid,
		    IN uint16_t block_num,
		    IN uint8_t port_num,
		    IN const ib_pkey_table_t * const p_pkey_tbl,
		    IN const osm_log_level_t log_level);

void
osm_dump_slvl_map_table(IN osm_log_t * const p_log,
			IN uint64_t port_guid,
			IN uint8_t in_port_num,
			IN uint8_t out_port_num,
			IN const ib_slvl_table_t * const p_slvl_tbl,
			IN const osm_log_level_t log_level);

void
osm_dump_vl_arb_table(IN osm_log_t * const p_log,
		      IN uint64_t port_guid,
		      IN uint8_t block_num,
		      IN uint8_t port_num,
		      IN const ib_vl_arb_table_t * const p_vla_tbl,
		      IN const osm_log_level_t log_level);

/****f* OpenSM: Helper/osm_dump_port_info
* NAME
*	osm_dump_port_info
*
* DESCRIPTION
*	Dumps the PortInfo attribute to the log.
*
* SYNOPSIS
*/
void osm_dump_node_info(IN osm_log_t * const p_log,
			IN const ib_node_info_t * const p_ni,
			IN const osm_log_level_t log_level);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to the osm_log_t object
*
*	p_ni
*		[in] Pointer to the NodeInfo attribute
*
*	log_level
*		[in] Log verbosity level with which to dump the data.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_dump_sm_info
* NAME
*	osm_dump_sm_info
*
* DESCRIPTION
*	Dumps the SMInfo attribute to the log.
*
* SYNOPSIS
*/
void
osm_dump_sm_info(IN osm_log_t * const p_log,
		 IN const ib_sm_info_t * const p_smi,
		 IN const osm_log_level_t log_level);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to the osm_log_t object
*
*	p_smi
*		[in] Pointer to the SMInfo attribute
*
*	log_level
*		[in] Log verbosity level with which to dump the data.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_dump_switch_info
* NAME
*	osm_dump_switch_info
*
* DESCRIPTION
*	Dumps the SwitchInfo attribute to the log.
*
* SYNOPSIS
*/
void
osm_dump_switch_info(IN osm_log_t * const p_log,
		     IN const ib_switch_info_t * const p_si,
		     IN const osm_log_level_t log_level);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to the osm_log_t object
*
*	p_si
*		[in] Pointer to the SwitchInfo attribute
*
*	log_level
*		[in] Log verbosity level with which to dump the data.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Helper/osm_dump_notice
* NAME
*	osm_dump_notice
*
* DESCRIPTION
*	Dumps the Notice attribute to the log.
*
* SYNOPSIS
*/
void
osm_dump_notice(IN osm_log_t * const p_log,
		IN const ib_mad_notice_attr_t * p_ntci,
		IN const osm_log_level_t log_level);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to the osm_log_t object
*
*	p_ntci
*		[in] Pointer to the Notice attribute
*
*	log_level
*		[in] Log verbosity level with which to dump the data.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/osm_get_disp_msg_str
* NAME
*	osm_get_disp_msg_str
*
* DESCRIPTION
*	Returns a string for the specified Dispatcher message.
*
* SYNOPSIS
*/
const char *osm_get_disp_msg_str(IN cl_disp_msgid_t msg);
/*
* PARAMETERS
*	msg
*		[in] Dispatcher message ID value.
*
* RETURN VALUES
*	Pointer to the message discription string.
*
* NOTES
*
* SEE ALSO
*********/

void osm_dump_dr_path(IN osm_log_t * const p_log,
		      IN const osm_dr_path_t * const p_path,
		      IN const osm_log_level_t level);

void osm_dump_smp_dr_path(IN osm_log_t * const p_log,
			  IN const ib_smp_t * const p_smp,
			  IN const osm_log_level_t level);

void osm_dump_dr_smp(IN osm_log_t * const p_log,
		     IN const ib_smp_t * const p_smp,
		     IN const osm_log_level_t level);

void osm_dump_sa_mad(IN osm_log_t * const p_log,
		     IN const ib_sa_mad_t * const p_smp,
		     IN const osm_log_level_t level);

/****f* IBA Base: Types/osm_get_sm_signal_str
* NAME
*	osm_get_sm_signal_str
*
* DESCRIPTION
*	Returns a string for the specified SM state.
*
* SYNOPSIS
*/
const char *osm_get_sm_signal_str(IN osm_signal_t signal);
/*
* PARAMETERS
*	state
*		[in] Signal value
*
* RETURN VALUES
*	Pointer to the signal discription string.
*
* NOTES
*
* SEE ALSO
*********/

const char *osm_get_port_state_str_fixed_width(IN uint8_t port_state);

const char *osm_get_node_type_str_fixed_width(IN uint8_t node_type);

const char *osm_get_manufacturer_str(IN uint64_t const guid_ho);

const char *osm_get_mtu_str(IN uint8_t const mtu);

const char *osm_get_lwa_str(IN uint8_t const lwa);

const char *osm_get_mtu_str(IN uint8_t const mtu);

const char *osm_get_lwa_str(IN uint8_t const lwa);

const char *osm_get_lsa_str(IN uint8_t const lsa);

/****f* IBA Base: Types/osm_get_sm_mgr_signal_str
* NAME
*	osm_get_sm_mgr_signal_str
*
* DESCRIPTION
*	Returns a string for the specified SM manager signal.
*
* SYNOPSIS
*/
const char *osm_get_sm_mgr_signal_str(IN osm_sm_signal_t signal);
/*
* PARAMETERS
*	signal
*		[in] SM manager signal
*
* RETURN VALUES
*	Pointer to the signal discription string.
*
* NOTES
*
* SEE ALSO
*********/

/****f* IBA Base: Types/osm_get_sm_mgr_state_str
* NAME
*	osm_get_sm_mgr_state_str
*
* DESCRIPTION
*	Returns a string for the specified SM manager state.
*
* SYNOPSIS
*/
const char *osm_get_sm_mgr_state_str(IN uint16_t state);
/*
* PARAMETERS
*	state
*		[in] SM manager state
*
* RETURN VALUES
*	Pointer to the state discription string.
*
* NOTES
*
* SEE ALSO
*********/

END_C_DECLS
#endif				/* _OSM_HELPER_H_ */
