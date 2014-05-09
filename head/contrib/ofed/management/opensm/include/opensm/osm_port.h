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

/*
 * Abstract:
 * 	Declaration of port related objects.
 *	These objects comprise an IBA port.
 *	These objects are part of the OpenSM family of objects.
 */

#ifndef _OSM_PORT_H_
#define _OSM_PORT_H_

#include <complib/cl_qmap.h>
#include <iba/ib_types.h>
#include <opensm/osm_base.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_madw.h>
#include <opensm/osm_path.h>
#include <opensm/osm_pkey.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/*
	Forward references.
*/
struct osm_port;
struct osm_node;

/****h* OpenSM/Physical Port
* NAME
*	Physical Port
*
* DESCRIPTION
*	The Physical Port object encapsulates the information needed by the
*	OpenSM to manage physical ports.  The OpenSM allocates one Physical Port
*	per physical port in the IBA subnet.
*
*	In a switch, one multiple Physical Port objects share the same port GUID.
*	In an end-point, Physical Ports do not share GUID values.
*
*	The Physical Port is not thread safe, thus callers must provide
*	serialization.
*
*	These objects should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* AUTHOR
*	Steve King, Intel
*
*********/

/****s* OpenSM: Physical Port/osm_physp_t
* NAME
*	osm_physp_t
*
* DESCRIPTION
*	This object represents a physical port on a switch, router or end-point.
*
*	The osm_physp_t object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_physp {
	ib_port_info_t port_info;
	ib_net64_t port_guid;
	uint8_t port_num;
	struct osm_node *p_node;
	struct osm_physp *p_remote_physp;
	boolean_t healthy;
	uint8_t vl_high_limit;
	unsigned need_update;
	unsigned is_prof_ignored;
	osm_dr_path_t dr_path;
	osm_pkey_tbl_t pkeys;
	ib_vl_arb_table_t vl_arb[4];
	cl_ptr_vector_t slvl_by_port;
} osm_physp_t;
/*
* FIELDS
*	port_info
*		The IBA defined PortInfo data for this port.
*
*	port_guid
*		Port GUID value of this port.  For switches,
*		all ports share the same GUID value.
*
*	port_num
*		The port number of this port.  The PortInfo also
*		contains a port_number, but that number is not
*		the port number of this port, but rather the number
*		of the port that received the SMP during discovery.
*		Therefore, we must keep a separate record for this
*		port's port number.
*
*	p_node
*		Pointer to the parent Node object of this Physical Port.
*
*	p_remote_physp
*		Pointer to the Physical Port on the other side of the wire.
*		If this pointer is NULL no link exists at this port.
*
*	healthy
*		Tracks the health of the port. Normally should be TRUE but
*		might change as a result of incoming traps indicating the port
*		healthy is questionable.
*
*	vl_high_limit
*		PortInfo:VLHighLimit value which installed by QoS manager
*		and should be uploaded to port's PortInfo
*
*	need_update
*		When set indicates that port was probably reset and port
*		related tables (PKey, SL2VL, VLArb) require refreshing.
*
*	is_prof_ignored
*		When set indicates that switch port will be ignored by
*		the link load equalization algorithm.
*
*	dr_path
*		The directed route path to this port.
*
*	pkeys
*		osm_pkey_tbl_t object holding the port PKeys.
*
*	vl_arb[]
*		Each Physical Port has 4 sections of VL Arbitration table.
*
*	slvl_by_port
*		A vector of pointers to the sl2vl tables (ordered by input port).
*		Switches have an entry for every other input port (inc SMA=0).
*		On CAs only one per port.
*
* SEE ALSO
*	Port
*********/

/****f* OpenSM: Physical Port/osm_physp_construct
* NAME
*	osm_physp_construct
*
* DESCRIPTION
*	Constructs a Physical Port.
*
* SYNOPSIS
*/
void osm_physp_construct(IN osm_physp_t * const p_physp);
/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object to initialize.
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_init
* NAME
*	osm_physp_init
*
* DESCRIPTION
*	Initializes a Physical Port for use.
*
* SYNOPSIS
*/
void
osm_physp_init(IN osm_physp_t * const p_physp,
	       IN const ib_net64_t port_guid,
	       IN const uint8_t port_num,
	       IN const struct osm_node *const p_node,
	       IN const osm_bind_handle_t h_bind,
	       IN const uint8_t hop_count,
	       IN const uint8_t * const p_initial_path);
/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object to initialize.
*
*	port_guid
*		[in] GUID value of this port.  Switch ports all share
*		the same value.
*		Caller should use 0 if the guid is unknown.
*
*	port_num
*		[in] The port number of this port.
*
*	p_node
*		[in] Pointer to the parent Node object of this Physical Port.
*
*	h_bind
*		[in] Bind handle on which this port is accessed.
*		Caller should use OSM_INVALID_BIND_HANDLE if the bind
*		handle to this port is unknown.
*
*	hop_count
*		[in] Directed route hop count to reach this port.
*		Caller should use 0 if the hop count is unknown.
*
*	p_initial_path
*		[in] Pointer to the directed route path to reach this node.
*		Caller should use NULL if the path is unknown.
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Port/void osm_physp_destroy
* NAME
*	osm_physp_destroy
*
* DESCRIPTION
*	This function destroys a Port object.
*
* SYNOPSIS
*/
void osm_physp_destroy(IN osm_physp_t * const p_physp);
/*
* PARAMETERS
*	p_port
*		[in] Pointer to a PhysPort object to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Performs any necessary cleanup of the specified PhysPort object.
*	Further operations should not be attempted on the destroyed object.
*	This function should only be called after a call to osm_physp_construct or
*	osm_physp_init.
*
* SEE ALSO
*	Port
*********/

/****f* OpenSM: Physical Port/osm_physp_is_valid
* NAME
*	osm_physp_is_valid
*
* DESCRIPTION
*	Returns TRUE if the Physical Port has been successfully initialized.
*	FALSE otherwise.
*
* SYNOPSIS
*/
static inline boolean_t osm_physp_is_valid(IN const osm_physp_t * const p_physp)
{
	CL_ASSERT(p_physp);
	return (p_physp->port_guid != 0);
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
* RETURN VALUES
*	Returns TRUE if the Physical Port has been successfully initialized.
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_is_healthy
* NAME
*	osm_physp_is_healthy
*
* DESCRIPTION
*	Returns TRUE if the Physical Port has been maked as healthy
*	FALSE otherwise.
*
* SYNOPSIS
*/
static inline boolean_t
osm_physp_is_healthy(IN const osm_physp_t * const p_physp)
{
	CL_ASSERT(p_physp);
	return (p_physp->healthy);
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
* RETURN VALUES
*	Returns TRUE if the Physical Port has been maked as healthy
*	FALSE otherwise.
*  All physical ports are initialized as "healthy" but may be marked
*  otherwise if a received trap claims otherwise.
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_link_is_healthy
* NAME
*	osm_link_is_healthy
*
* DESCRIPTION
*	Returns TRUE if the link given by the physical port is health,
*  and FALSE otherwise. Link is healthy if both its physical ports are
*  healthy
*
* SYNOPSIS
*/
boolean_t osm_link_is_healthy(IN const osm_physp_t * const p_physp);
/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
* RETURN VALUES
*	TRUE if both physical ports on the link are healthy, and FALSE otherwise.
*  All physical ports are initialized as "healthy" but may be marked
*  otherwise if a received trap claiming otherwise.
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_set_health
* NAME
*	osm_physp_set_health
*
* DESCRIPTION
*	Sets the port health flag. TRUE means the port is healthy and
*  should be used for packet routing. FALSE means it should be avoided.
*
* SYNOPSIS
*/
static inline void
osm_physp_set_health(IN osm_physp_t * const p_physp, IN boolean_t is_healthy)
{
	CL_ASSERT(p_physp);
	p_physp->healthy = is_healthy;
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
*	is_healthy
*		[in] The health value to be assigned to the port.
*		     TRUE if the Physical Port should been maked as healthy
*		     FALSE otherwise.
*
* RETURN VALUES
*  NONE
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_set_port_info
* NAME
*	osm_physp_set_port_info
*
* DESCRIPTION
*	Copies the PortInfo attribute into the Physical Port object
*	based on the PortState.
*
* SYNOPSIS
*/
static inline void
osm_physp_set_port_info(IN osm_physp_t * const p_physp,
			IN const ib_port_info_t * const p_pi)
{
	CL_ASSERT(p_pi);
	CL_ASSERT(osm_physp_is_valid(p_physp));

	if (ib_port_info_get_port_state(p_pi) == IB_LINK_DOWN) {
		/* If PortState is down, only copy PortState */
		/* and PortPhysicalState per C14-24-2.1 */
		ib_port_info_set_port_state(&p_physp->port_info, IB_LINK_DOWN);
		ib_port_info_set_port_phys_state
		    (ib_port_info_get_port_phys_state(p_pi),
		     &p_physp->port_info);
	} else {
		p_physp->port_info = *p_pi;
	}
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
*	p_pi
*		[in] Pointer to the IBA defined PortInfo at this port number.
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_set_pkey_tbl
* NAME
*  osm_physp_set_pkey_tbl
*
* DESCRIPTION
*  Copies the P_Key table into the Physical Port object.
*
* SYNOPSIS
*/
void
osm_physp_set_pkey_tbl(IN osm_log_t * p_log,
		       IN const osm_subn_t * p_subn,
		       IN osm_physp_t * const p_physp,
		       IN ib_pkey_table_t * p_pkey_tbl, IN uint16_t block_num);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to a log object.
*
*	p_subn
*		[in] Pointer to the subnet data structure.
*
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
*	p_pkey_tbl
*		[in] Pointer to the IBA defined P_Key table for this port
*		     number.
*
*	block_num
*		[in] The part of the P_Key table as defined in the IBA
*		     (valid values 0-2047, and is further limited by the
*		     partitionCap).
*
* RETURN VALUES
*  This function does not return a value.
*
* NOTES
*
* SEE ALSO
*  Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_get_pkey_tbl
* NAME
*  osm_physp_get_pkey_tbl
*
* DESCRIPTION
*  Returns a pointer to the P_Key table object of the Physical Port object.
*
* SYNOPSIS
*/
static inline const osm_pkey_tbl_t *osm_physp_get_pkey_tbl(IN const osm_physp_t
							   * const p_physp)
{
	CL_ASSERT(osm_physp_is_valid(p_physp));
	/*
	   (14.2.5.7) - the block number valid values are 0-2047, and are
	   further limited by the size of the P_Key table specified by the
	   PartitionCap on the node.
	 */
	return (&p_physp->pkeys);
};

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
* RETURN VALUES
*  The pointer to the P_Key table object.
*
* NOTES
*
* SEE ALSO
*  Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_set_slvl_tbl
* NAME
*	osm_physp_set_slvl_tbl
*
* DESCRIPTION
*	Copies the SLtoVL attribute into the Physical Port object.
*
* SYNOPSIS
*/
static inline void
osm_physp_set_slvl_tbl(IN osm_physp_t * const p_physp,
		       IN ib_slvl_table_t * p_slvl_tbl, IN uint8_t in_port_num)
{
	ib_slvl_table_t *p_tbl;

	CL_ASSERT(p_slvl_tbl);
	CL_ASSERT(osm_physp_is_valid(p_physp));
	p_tbl = cl_ptr_vector_get(&p_physp->slvl_by_port, in_port_num);
	*p_tbl = *p_slvl_tbl;
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
*	p_slvl_tbl
*		[in] Pointer to the IBA defined SLtoVL map table for this
*		     port number.
*
*	in_port_num
*		[in] Input Port Number for this SLtoVL.
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_get_slvl_tbl
* NAME
*	osm_physp_get_slvl_tbl
*
* DESCRIPTION
*	Returns a pointer to the SLtoVL attribute of the Physical Port object.
*
* SYNOPSIS
*/
static inline ib_slvl_table_t *osm_physp_get_slvl_tbl(IN const osm_physp_t *
						      const p_physp,
						      IN uint8_t in_port_num)
{
	ib_slvl_table_t *p_tbl;

	CL_ASSERT(osm_physp_is_valid(p_physp));
	p_tbl = cl_ptr_vector_get(&p_physp->slvl_by_port, in_port_num);
	return (p_tbl);
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
*	in_port_num
*		[in] Input Port Number for this SLtoVL.
*
* RETURN VALUES
*	The pointer to the slvl table
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_set_vla_tbl
* NAME
*	osm_physp_set_vla_tbl
*
* DESCRIPTION
*	Copies the VL Arbitration attribute into the Physical Port object.
*
* SYNOPSIS
*/
static inline void
osm_physp_set_vla_tbl(IN osm_physp_t * const p_physp,
		      IN ib_vl_arb_table_t * p_vla_tbl, IN uint8_t block_num)
{
	CL_ASSERT(p_vla_tbl);
	CL_ASSERT(osm_physp_is_valid(p_physp));
	CL_ASSERT((1 <= block_num) && (block_num <= 4));
	p_physp->vl_arb[block_num - 1] = *p_vla_tbl;
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
*	p_vla_tbl
*		[in] Pointer to the IBA defined VL Arbitration table for this
*		     port number.
*
*	block_num
*		[in] The part of the VL arbitration as defined in the IBA
*		     (valid values 1-4)
*
* RETURN VALUES
*	This function does not return a value.
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_get_vla_tbl
* NAME
*	osm_physp_get_vla_tbl
*
* DESCRIPTION
*	Returns a pointer to the VL Arbitration table of the Physical Port object.
*
* SYNOPSIS
*/
static inline ib_vl_arb_table_t *osm_physp_get_vla_tbl(IN osm_physp_t *
						       const p_physp,
						       IN uint8_t block_num)
{
	CL_ASSERT(osm_physp_is_valid(p_physp));
	CL_ASSERT((1 <= block_num) && (block_num <= 4));
	return (&(p_physp->vl_arb[block_num - 1]));
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
*	block_num
*		[in] The part of the VL arbitration as defined in the IBA
*		     (valid values 1-4)
*
* RETURN VALUES
*  The pointer to the VL Arbitration table
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_get_remote
* NAME
*	osm_physp_get_remote
*
* DESCRIPTION
*	Returns a pointer to the Physical Port on the other side the wire.
*
* SYNOPSIS
*/
static inline osm_physp_t *osm_physp_get_remote(IN const osm_physp_t *
						const p_physp)
{
	CL_ASSERT(osm_physp_is_valid(p_physp));
	return (p_physp->p_remote_physp);
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
* RETURN VALUES
*	Returns a pointer to the Physical Port on the other side of
*	the wire.  A return value of NULL means there is no link at this port.
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_get_port_guid
* NAME
*	osm_physp_get_port_guid
*
* DESCRIPTION
*	Returns the port guid of this physical port.
*
* SYNOPSIS
*/
static inline ib_net64_t
osm_physp_get_port_guid(IN const osm_physp_t * const p_physp)
{
	CL_ASSERT(osm_physp_is_valid(p_physp));
	return (p_physp->port_guid);
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
* RETURN VALUES
*	Returns the port guid of this physical port.
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_get_subnet_prefix
* NAME
*	osm_physp_get_subnet_prefix
*
* DESCRIPTION
*	Returns the subnet prefix for this physical port.
*
* SYNOPSIS
*/
static inline ib_net64_t
osm_physp_get_subnet_prefix(IN const osm_physp_t * const p_physp)
{
	CL_ASSERT(osm_physp_is_valid(p_physp));
	return (p_physp->port_info.subnet_prefix);
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
* RETURN VALUES
*	Returns the subnet prefix for this physical port.
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_link_exists
* NAME
*	osm_physp_link_exists
*
* DESCRIPTION
*	Returns TRUE if the Physical Port has a link to the specified port.
*	FALSE otherwise.
*
* SYNOPSIS
*/
static inline boolean_t
osm_physp_link_exists(IN const osm_physp_t * const p_physp,
		      IN const osm_physp_t * const p_remote_physp)
{
	CL_ASSERT(p_physp);
	CL_ASSERT(osm_physp_is_valid(p_physp));
	CL_ASSERT(p_remote_physp);
	CL_ASSERT(osm_physp_is_valid(p_remote_physp));
	return ((p_physp->p_remote_physp == p_remote_physp) &&
		(p_remote_physp->p_remote_physp == p_physp));
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
*	p_remote_physp
*		[in] Pointer to an osm_physp_t object.
*
* RETURN VALUES
*	Returns TRUE if the Physical Port has a link to another port.
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_link
* NAME
*	osm_physp_link
*
* DESCRIPTION
*	Sets the pointers to the Physical Ports on the other side the wire.
*
* SYNOPSIS
*/
static inline void
osm_physp_link(IN osm_physp_t * const p_physp,
	       IN osm_physp_t * const p_remote_physp)
{
	CL_ASSERT(p_physp);
	CL_ASSERT(p_remote_physp);
	p_physp->p_remote_physp = p_remote_physp;
	p_remote_physp->p_remote_physp = p_physp;
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object to link.
*
*	p_remote_physp
*		[in] Pointer to the adjacent osm_physp_t object to link.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_unlink
* NAME
*	osm_physp_unlink
*
* DESCRIPTION
*	Clears the pointers to the Physical Port on the other side the wire.
*
* SYNOPSIS
*/
static inline void
osm_physp_unlink(IN osm_physp_t * const p_physp,
		 IN osm_physp_t * const p_remote_physp)
{
	CL_ASSERT(p_physp);
	CL_ASSERT(p_remote_physp);
	CL_ASSERT(osm_physp_link_exists(p_physp, p_remote_physp));
	p_physp->p_remote_physp = NULL;
	p_remote_physp->p_remote_physp = NULL;
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object to link.
*
*	p_remote_physp
*		[in] Pointer to the adjacent osm_physp_t object to link.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_has_any_link
* NAME
*	osm_physp_has_any_link
*
* DESCRIPTION
*	Returns TRUE if the Physical Port has a link to another port.
*	FALSE otherwise.
*
* SYNOPSIS
*/
static inline boolean_t
osm_physp_has_any_link(IN const osm_physp_t * const p_physp)
{
	CL_ASSERT(p_physp);
	if (osm_physp_is_valid(p_physp))
		return (p_physp->p_remote_physp != NULL);
	else
		return (FALSE);
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
* RETURN VALUES
*	Returns TRUE if the Physical Port has a link to another port.
*	FALSE otherwise.
*
* NOTES
*
* SEE ALSO
*	Port, Physical Port
*********/

/****f* OpenSM: Physical Port/osm_physp_get_port_num
* NAME
*	osm_physp_get_port_num
*
* DESCRIPTION
*	Returns the local port number of this Physical Port.
*
* SYNOPSIS
*/
static inline uint8_t
osm_physp_get_port_num(IN const osm_physp_t * const p_physp)
{
	CL_ASSERT(p_physp);
	CL_ASSERT(osm_physp_is_valid(p_physp));
	return (p_physp->port_num);
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
* RETURN VALUES
*	Returns the local port number of this Physical Port.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Physical Port/osm_physp_get_node_ptr
* NAME
*	osm_physp_get_node_ptr
*
* DESCRIPTION
*	Returns a pointer to the parent Node object for this port.
*
* SYNOPSIS
*/
static inline struct osm_node *osm_physp_get_node_ptr(IN const osm_physp_t *
						       const p_physp)
{
	CL_ASSERT(p_physp);
	CL_ASSERT(osm_physp_is_valid(p_physp));
	return ((struct osm_node *)p_physp->p_node);
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
* RETURN VALUES
*	Returns a pointer to the parent Node object for this port.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Physical Port/osm_physp_get_port_state
* NAME
*	osm_physp_get_port_state
*
* DESCRIPTION
*	Returns the port state of this Physical Port.
*
* SYNOPSIS
*/
static inline uint8_t
osm_physp_get_port_state(IN const osm_physp_t * const p_physp)
{
	CL_ASSERT(p_physp);
	CL_ASSERT(osm_physp_is_valid(p_physp));
	return (ib_port_info_get_port_state(&p_physp->port_info));
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
* RETURN VALUES
*	Returns the local port number of this Physical Port.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Physical Port/osm_physp_get_base_lid
* NAME
*	osm_physp_get_base_lid
*
* DESCRIPTION
*	Returns the base lid of this Physical Port.
*
* SYNOPSIS
*/
static inline ib_net16_t
osm_physp_get_base_lid(IN const osm_physp_t * const p_physp)
{
	CL_ASSERT(p_physp);
	CL_ASSERT(osm_physp_is_valid(p_physp));
	return (p_physp->port_info.base_lid);
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
* RETURN VALUES
*	Returns the base lid of this Physical Port.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Physical Port/osm_physp_get_lmc
* NAME
*	osm_physp_get_lmc
*
* DESCRIPTION
*	Returns the LMC value of this Physical Port.
*
* SYNOPSIS
*/
static inline uint8_t osm_physp_get_lmc(IN const osm_physp_t * const p_physp)
{
	CL_ASSERT(p_physp);
	CL_ASSERT(osm_physp_is_valid(p_physp));
	return (ib_port_info_get_lmc(&p_physp->port_info));
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
* RETURN VALUES
*	Returns the LMC value of this Physical Port.
*
* NOTES
*
* SEE ALSO
*********/

/****f* OpenSM: Physical Port/osm_physp_get_dr_path_ptr
* NAME
*	osm_physp_get_dr_path_ptr
*
* DESCRIPTION
*	Returns a pointer to the directed route path for this port.
*
* SYNOPSIS
*/
static inline osm_dr_path_t *osm_physp_get_dr_path_ptr(IN const osm_physp_t *
						       const p_physp)
{
	CL_ASSERT(p_physp);
	CL_ASSERT(osm_physp_is_valid(p_physp));
	return ((osm_dr_path_t *) & p_physp->dr_path);
}

/*
* PARAMETERS
*	p_physp
*		[in] Pointer to a Physical Port object.
*
* RETURN VALUES
*	Returns a pointer to the directed route path for this port.
*
* NOTES
*
* SEE ALSO
*	Physical Port object
*********/

/****h* OpenSM/Port
* NAME
*	Port
*
* DESCRIPTION
*	The Port object encapsulates the information needed by the
*	OpenSM to manage ports.  The OpenSM allocates one Port object
*	per port in the IBA subnet.
*
*	Each Port object is associated with a single port GUID.  A Port object
*	contains 1 or more Physical Port objects.  An end point node has
*	one Physical Port per Port.  A switch node has more than
*	one Physical Port per Port.
*
*	The Port object is not thread safe, thus callers must provide
*	serialization.
*
*	These objects should be treated as opaque and should be
*	manipulated only through the provided functions.
*
* AUTHOR
*	Steve King, Intel
*
*********/

/****s* OpenSM: Port/osm_port_t
* NAME
*	osm_port_t
*
* DESCRIPTION
*	This object represents a logical port on a switch, router or end-point.
*
*	The osm_port_t object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_port {
	cl_map_item_t map_item;
	cl_list_item_t list_item;
	struct osm_node *p_node;
	ib_net64_t guid;
	uint32_t discovery_count;
	unsigned is_new;
	osm_physp_t *p_physp;
	cl_qlist_t mcm_list;
	int flag;
	void *priv;
} osm_port_t;
/*
* FIELDS
*	map_item
*		Linkage structure for cl_qmap.  MUST BE FIRST MEMBER!
*
*	list_item
*		Linkage structure for cl_qlist. Used by ucast mgr during LFT calculation.
*
*	p_node
*		Points to the Node object that owns this port.
*
*	guid
*		Manufacturer assigned GUID for this port.
*
*	discovery_count
*		The number of times this port has been discovered
*		during the current fabric sweep.  This number is reset
*		to zero at the start of a sweep.
*
*	p_physp
*		The pointer to physical port used when physical
*		characteristics contained in the Physical Port are needed.
*
*	mcm_list
*		Multicast member list
*
*	flag
*		Utility flag for port management
*
* SEE ALSO
*	Port, Physical Port, Physical Port Table
*********/

/****f* OpenSM: Port/osm_port_delete
* NAME
*	osm_port_delete
*
* DESCRIPTION
*	This function destroys and deallocates a Port object.
*
* SYNOPSIS
*/
void osm_port_delete(IN OUT osm_port_t ** const pp_port);
/*
* PARAMETERS
*	pp_port
*		[in][out] Pointer to a pointer to a Port object to delete.
*		On return, this pointer is NULL.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Performs any necessary cleanup of the specified Port object.
*
* SEE ALSO
*	Port
*********/

/****f* OpenSM: Port/osm_port_new
* NAME
*	osm_port_new
*
* DESCRIPTION
*	This function allocates and initializes a Port object.
*
* SYNOPSIS
*/
osm_port_t *osm_port_new(IN const ib_node_info_t * p_ni,
			 IN struct osm_node *const p_parent_node);
/*
* PARAMETERS
*	p_ni
*		[in] Pointer to the NodeInfo attribute relavent for this port.
*
*	p_parent_node
*		[in] Pointer to the initialized parent osm_node_t object
*		that owns this port.
*
* RETURN VALUE
*	Pointer to the initialize Port object.
*
* NOTES
*	Allows calling other port methods.
*
* SEE ALSO
*	Port
*********/

/****f* OpenSM: Port/osm_port_get_base_lid
* NAME
*	osm_port_get_base_lid
*
* DESCRIPTION
*	Gets the base LID of a port.
*
* SYNOPSIS
*/
static inline ib_net16_t
osm_port_get_base_lid(IN const osm_port_t * const p_port)
{
	CL_ASSERT(p_port->p_physp && osm_physp_is_valid(p_port->p_physp));
	return (osm_physp_get_base_lid(p_port->p_physp));
}

/*
* PARAMETERS
*	p_port
*		[in] Pointer to a Port object.
*
* RETURN VALUE
*	Base LID of the port.
*	If the return value is 0, then this port has no assigned LID.
*
* NOTES
*
* SEE ALSO
*	Port
*********/

/****f* OpenSM: Port/osm_port_get_lmc
* NAME
*	osm_port_get_lmc
*
* DESCRIPTION
*	Gets the LMC value of a port.
*
* SYNOPSIS
*/
static inline uint8_t osm_port_get_lmc(IN const osm_port_t * const p_port)
{
	CL_ASSERT(p_port->p_physp && osm_physp_is_valid(p_port->p_physp));
	return (osm_physp_get_lmc(p_port->p_physp));
}

/*
* PARAMETERS
*	p_port
*		[in] Pointer to a Port object.
*
* RETURN VALUE
*	Gets the LMC value of a port.
*
* NOTES
*
* SEE ALSO
*	Port
*********/

/****f* OpenSM: Port/osm_port_get_guid
* NAME
*	osm_port_get_guid
*
* DESCRIPTION
*	Gets the GUID of a port.
*
* SYNOPSIS
*/
static inline ib_net64_t osm_port_get_guid(IN const osm_port_t * const p_port)
{
	return (p_port->guid);
}

/*
* PARAMETERS
*	p_port
*		[in] Pointer to a Port object.
*
* RETURN VALUE
*	Manufacturer assigned GUID of the port.
*
* NOTES
*
* SEE ALSO
*	Port
*********/

/****f* OpenSM: Port/osm_port_get_lid_range_ho
* NAME
*	osm_port_get_lid_range_ho
*
* DESCRIPTION
*	Returns the HOST ORDER lid min and max values for this port,
*	based on the lmc value.
*
* SYNOPSIS
*/
void
osm_port_get_lid_range_ho(IN const osm_port_t * const p_port,
			  OUT uint16_t * const p_min_lid,
			  OUT uint16_t * const p_max_lid);
/*
* PARAMETERS
*	p_port
*		[in] Pointer to a Port object.
*
*	p_min_lid
*		[out] Pointer to the minimum LID value occupied by this port.
*
*	p_max_lid
*		[out] Pointer to the maximum LID value occupied by this port.
*
* RETURN VALUE
*	None.
*
* NOTES
*
* SEE ALSO
*	Port
*********/

/****f* OpenSM: Port/osm_get_port_by_base_lid
* NAME
*	osm_get_port_by_base_lid
*
* DESCRIPTION
*	Returns a status on whether a Port was able to be
*	determined based on the LID supplied and if so, return the Port.
*
* SYNOPSIS
*/
ib_api_status_t
osm_get_port_by_base_lid(IN const osm_subn_t * const p_subn,
			 IN const ib_net16_t lid,
			 IN OUT const osm_port_t ** const pp_port);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet data structure.
*
*	lid
*		[in] LID requested.
*
*	pp_port
*		[in][out] Pointer to pointer to Port object.
*
* RETURN VALUES
*	IB_SUCCESS
*	IB_NOT_FOUND
*
* NOTES
*
* SEE ALSO
*       Port
*********/

/****f* OpenSM: Port/osm_port_add_mgrp
* NAME
*	osm_port_add_mgrp
*
* DESCRIPTION
*	Logically connects a port to a multicast group.
*
* SYNOPSIS
*/
ib_api_status_t
osm_port_add_mgrp(IN osm_port_t * const p_port, IN const ib_net16_t mlid);
/*
* PARAMETERS
*	p_port
*		[in] Pointer to an osm_port_t object.
*
*	mlid
*		[in] MLID of the multicast group.
*
* RETURN VALUES
*	IB_SUCCESS
*	IB_INSUFFICIENT_MEMORY
*
* NOTES
*
* SEE ALSO
*	Port object
*********/

/****f* OpenSM: Port/osm_port_remove_mgrp
* NAME
*	osm_port_remove_mgrp
*
* DESCRIPTION
*	Logically disconnects a port from a multicast group.
*
* SYNOPSIS
*/
void
osm_port_remove_mgrp(IN osm_port_t * const p_port, IN const ib_net16_t mlid);
/*
* PARAMETERS
*	p_port
*		[in] Pointer to an osm_port_t object.
*
*	mlid
*		[in] MLID of the multicast group.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	Port object
*********/

/****f* OpenSM: Port/osm_port_remove_all_mgrp
* NAME
*	osm_port_remove_all_mgrp
*
* DESCRIPTION
*	Logically disconnects a port from all its multicast groups.
*
* SYNOPSIS
*/
void osm_port_remove_all_mgrp(IN osm_port_t * const p_port);
/*
* PARAMETERS
*	p_port
*		[in] Pointer to an osm_port_t object.
*
* RETURN VALUES
*	None.
*
* NOTES
*
* SEE ALSO
*	Port object
*********/

/****f* OpenSM: Physical Port/osm_physp_calc_link_mtu
* NAME
*	osm_physp_calc_link_mtu
*
* DESCRIPTION
*	Calculate the Port MTU based on current and remote
*  physical ports MTU CAP values.
*
* SYNOPSIS
*/
uint8_t
osm_physp_calc_link_mtu(IN osm_log_t * p_log, IN const osm_physp_t * p_physp);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to a log object.
*
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
* RETURN VALUES
*	The MTU of the link to be used.
*
* NOTES
*
* SEE ALSO
*	PhysPort object
*********/

/****f* OpenSM: Physical Port/osm_physp_calc_link_op_vls
* NAME
*	osm_physp_calc_link_op_vls
*
* DESCRIPTION
*	Calculate the Port OP_VLS based on current and remote
*  physical ports VL CAP values. Allowing user option for a max limit.
*
* SYNOPSIS
*/
uint8_t
osm_physp_calc_link_op_vls(IN osm_log_t * p_log,
			   IN const osm_subn_t * p_subn,
			   IN const osm_physp_t * p_physp);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to a log object.
*
*	p_subn
*		[in] Pointer to the subnet object for accessing of the options.
*
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
* RETURN VALUES
*	The OP_VLS of the link to be used.
*
* NOTES
*
* SEE ALSO
*  PhysPort object
*********/

/****f* OpenSM: Physical Port/osm_physp_replace_dr_path_with_alternate_dr_path
* NAME
*	osm_physp_replace_dr_path_with_alternate_dr_path
*
* DESCRIPTION
*	Replace the direct route path for the given phys port with an
*  alternate path going through forien set of phys port.
*
* SYNOPSIS
*/
void
osm_physp_replace_dr_path_with_alternate_dr_path(IN osm_log_t * p_log,
						 IN osm_subn_t const *p_subn,
						 IN osm_physp_t const *p_physp,
						 IN osm_bind_handle_t * h_bind);
/*
* PARAMETERS
*	p_log
*		[in] Pointer to a log object.
*
*	p_subn
*		[in] Pointer to the subnet object for accessing of the options.
*
*	p_physp
*		[in] Pointer to an osm_physp_t object.
*
*	h_bind
*		[in] Pointer to osm_bind_handle_t object.
*
* RETURN VALUES
*	NONE
*
* NOTES
*
* SEE ALSO
*	PhysPort object
*********/

END_C_DECLS
#endif				/* _OSM_PORT_H_ */
