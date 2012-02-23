/*
 * Copyright (c) 2006 Voltaire, Inc. All rights reserved.
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
 *    Declaration of osmtest_t.
 * This object represents the OSMTest Test object.
 *
 */

#ifndef _OSMTEST_H_
#define _OSMTEST_H_

#include <complib/cl_qmap.h>
#include <opensm/osm_log.h>
#include <vendor/osm_vendor_api.h>
#include <vendor/osm_vendor_sa_api.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_helper.h>
#include "osmtest_base.h"
#include "osmtest_subnet.h"

enum OSMT_FLOWS {
	OSMT_FLOW_ALL = 0,
	OSMT_FLOW_CREATE_INVENTORY,
	OSMT_FLOW_VALIDATE_INVENTORY,
	OSMT_FLOW_SERVICE_REGISTRATION,
	OSMT_FLOW_EVENT_FORWARDING,
	OSMT_FLOW_STRESS_SA,
	OSMT_FLOW_MULTICAST,
	OSMT_FLOW_QOS,
	OSMT_FLOW_TRAP,
};

/****s* OpenSM: Subnet/osmtest_opt_t
 * NAME
 * osmtest_opt_t
 *
 * DESCRIPTION
 * Subnet options structure.  This structure contains the various
 * site specific configuration parameters for osmtest.
 *
 * SYNOPSIS
 */
typedef struct _osmtest_opt {
	uint32_t transaction_timeout;
	boolean_t force_log_flush;
	boolean_t create;
	uint32_t retry_count;
	uint32_t stress;
	uint32_t mmode;
	char file_name[OSMTEST_FILE_PATH_MAX];
	uint8_t flow;
	uint8_t wait_time;
	char *log_file;
	boolean_t ignore_path_records;
} osmtest_opt_t;

/*
 * FIELDS
 *
 * SEE ALSO
 *********/

/****h* OSMTest/OSMTest
 * NAME
 * OSMTest
 *
 * DESCRIPTION
 * The OSMTest object tests an SM/SA for conformance to a known
 * set of data about an Infiniband subnet.
 *
 * AUTHOR
 * Steve King, Intel
 *
 *********/

/****s* OSMTest/osmtest_t
 * NAME
 * osmtest_t
 *
 * DESCRIPTION
 * OSMTest structure.
 *
 * This object should be treated as opaque and should
 * be manipulated only through the provided functions.
 *
 * SYNOPSIS
 */
typedef struct _osmtest {
	osm_log_t log;
	struct _osm_vendor *p_vendor;
	osm_bind_handle_t h_bind;
	osm_mad_pool_t mad_pool;

	osmtest_opt_t opt;
	ib_port_attr_t local_port;
	subnet_t exp_subn;
	cl_qpool_t node_pool;
	cl_qpool_t port_pool;
	cl_qpool_t link_pool;

	uint16_t max_lid;
} osmtest_t;

/*
 * FIELDS
 * log
 *    Log facility used by all OSMTest components.
 *
 * p_vendor
 *    Pointer to the vendor transport layer.
 *
 *  h_bind
 *     The bind handle obtained by osm_vendor_sa_api/osmv_bind_sa
 *
 *  mad_pool
 *     The mad pool provided for teh vendor layer to allocate mad wrappers in
 *
 * opt
 *    osmtest options structure
 *
 * local_port
 *    Port attributes for the port over which osmtest is running.
 *
 * exp_subn
 *    Subnet object representing the expected subnet
 *
 * node_pool
 *    Pool of objects for use in populating the subnet databases.
 *
 * port_pool
 *    Pool of objects for use in populating the subnet databases.
 *
 * link_pool
 *    Pool of objects for use in populating the subnet databases.
 *
 * SEE ALSO
 *********/

/****s* OpenSM: Subnet/osmtest_req_context_t
 * NAME
 * osmtest_req_context_t
 *
 * DESCRIPTION
 * Query context for ib_query callback function.
 *
 * SYNOPSIS
 */
typedef struct _osmtest_req_context {
	osmtest_t *p_osmt;
	osmv_query_res_t result;
} osmtest_req_context_t;

typedef struct _osmtest_mgrp_t {
	cl_map_item_t map_item;
	ib_member_rec_t mcmember_rec;
} osmtest_mgrp_t;

/*
 * FIELDS
 *
 * SEE ALSO
 *********/

/****f* OSMTest/osmtest_construct
 * NAME
 * osmtest_construct
 *
 * DESCRIPTION
 * This function constructs an OSMTest object.
 *
 * SYNOPSIS
 */
void osmtest_construct(IN osmtest_t * const p_osmt);

/*
 * PARAMETERS
 * p_osmt
 *    [in] Pointer to a OSMTest object to construct.
 *
 * RETURN VALUE
 * This function does not return a value.
 *
 * NOTES
 * Allows calling osmtest_init, osmtest_destroy.
 *
 * Calling osmtest_construct is a prerequisite to calling any other
 * method except osmtest_init.
 *
 * SEE ALSO
 * SM object, osmtest_init, osmtest_destroy
 *********/

/****f* OSMTest/osmtest_destroy
 * NAME
 * osmtest_destroy
 *
 * DESCRIPTION
 * The osmtest_destroy function destroys an osmtest object, releasing
 * all resources.
 *
 * SYNOPSIS
 */
void osmtest_destroy(IN osmtest_t * const p_osmt);

/*
 * PARAMETERS
 * p_osmt
 *    [in] Pointer to a OSMTest object to destroy.
 *
 * RETURN VALUE
 * This function does not return a value.
 *
 * NOTES
 * Performs any necessary cleanup of the specified OSMTest object.
 * Further operations should not be attempted on the destroyed object.
 * This function should only be called after a call to osmtest_construct or
 * osmtest_init.
 *
 * SEE ALSO
 * SM object, osmtest_construct, osmtest_init
 *********/

/****f* OSMTest/osmtest_init
 * NAME
 * osmtest_init
 *
 * DESCRIPTION
 * The osmtest_init function initializes a OSMTest object for use.
 *
 * SYNOPSIS
 */
ib_api_status_t osmtest_init(IN osmtest_t * const p_osmt,
			     IN const osmtest_opt_t * const p_opt,
			     IN const osm_log_level_t log_flags);

/*
 * PARAMETERS
 * p_osmt
 *    [in] Pointer to an osmtest_t object to initialize.
 *
 * p_opt
 *    [in] Pointer to the options structure.
 *
 * log_flags
 *    [in] Log level flags to set.
 *
 * RETURN VALUES
 * IB_SUCCESS if the OSMTest object was initialized successfully.
 *
 * NOTES
 * Allows calling other OSMTest methods.
 *
 * SEE ALSO
 * SM object, osmtest_construct, osmtest_destroy
 *********/

/****f* OSMTest/osmtest_run
 * NAME
 * osmtest_run
 *
 * DESCRIPTION
 * Runs the osmtest suite.
 *
 * SYNOPSIS
 */
ib_api_status_t osmtest_run(IN osmtest_t * const p_osmt);

/*
 * PARAMETERS
 * p_osmt
 *    [in] Pointer to an osmtest_t object.
 *
 * guid
 *    [in] Port GUID over which to run the test suite.
 *
 * RETURN VALUES
 * IB_SUCCESS
 *
 * NOTES
 *
 * SEE ALSO
 *********/

/****f* OSMTest/osmtest_bind
 * NAME
 * osmtest_bind
 *
 * DESCRIPTION
 * Binds osmtest to a local port.
 *
 * SYNOPSIS
 */
ib_api_status_t osmtest_bind(IN osmtest_t * p_osmt,
			     IN uint16_t max_lid, IN ib_net64_t guid OPTIONAL);

/*
 * PARAMETERS
 * p_osmt
 *    [in] Pointer to an osmtest_t object.
 *
 *  max_lid
 *     [in] The maximal lid to query about (if RMPP is not supported)
 *
 * guid
 *    [in] Port GUID over which to run the test suite.
 *    If zero, the bind function will display a menu of local
 *    port guids and wait for user input.
 *
 * RETURN VALUES
 * IB_SUCCESS
 *
 * NOTES
 *
 * SEE ALSO
 *********/

/****f* OSMTest/osmtest_query_res_cb
 * NAME
 * osmtest_query_res_cb
 *
 * DESCRIPTION
 * A Callback for the query to invoke on completion
 *
 * SYNOPSIS
 */
void osmtest_query_res_cb(IN osmv_query_res_t * p_rec);
/*
 * PARAMETERS
 * p_rec
 *    [in] Pointer to an ib_query_rec_t object used for the query.
 *
 * RETURN VALUES
 * NONE
 *
 * NOTES
 *
 * SEE ALSO
 *********/

/****f* OSMTest/ib_get_mad_status_str
 * NAME
 * ib_get_mad_status_str
 *
 * DESCRIPTION
 * return the string representing the given  mad status
 *
 * SYNOPSIS
 */
const char *ib_get_mad_status_str(IN const ib_mad_t * const p_mad);
/*
 * PARAMETERS
 * p_mad
 *    [in] Pointer to the mad payload
 *
 * RETURN VALUES
 * NONE
 *
 * NOTES
 *
 * SEE ALSO
 *********/

/****f* OSMTest/osmt_run_service_records_flow
 * NAME
 * osmt_run_service_records_flow
 *
 * DESCRIPTION
 * Run the service record testing flow.
 *
 * SYNOPSIS
 */
ib_api_status_t osmt_run_service_records_flow(IN osmtest_t * const p_osmt);
/*
 * PARAMETERS
 *  p_osmt
 *    [in] Pointer to the osmtest obj
 *
 * RETURN VALUES
 * IB_SUCCESS if PASS
 *
 * NOTES
 *
 * SEE ALSO
 *********/

ib_api_status_t osmt_run_inform_info_flow(IN osmtest_t * const p_osmt);

/****f* OSMTest/osmt_run_slvl_and_vlarb_records_flow
 * NAME
 * osmt_run_slvl_and_vlarb_records_flow
 *
 * DESCRIPTION
 * Run the sl2vl and vlarb tables testing flow.
 *
 * SYNOPSIS
 */
ib_api_status_t
osmt_run_slvl_and_vlarb_records_flow(IN osmtest_t * const p_osmt);
/*
 * PARAMETERS
 *  p_osmt
 *    [in] Pointer to the osmtest obj
 *
 * RETURN VALUES
 * IB_SUCCESS if PASS
 *
 * NOTES
 *
 * SEE ALSO
 *********/

/****f* OSMTest/osmt_run_mcast_flow
 * NAME
 * osmt_run_mcast_flow
 *
 * DESCRIPTION
 * Run the multicast test flow
 *
 * SYNOPSIS
 */
ib_api_status_t osmt_run_mcast_flow(IN osmtest_t * const p_osmt);
/*
 * PARAMETERS
 *  p_osmt
 *    [in] Pointer to the osmtest obj
 *
 * RETURN VALUES
 * IB_SUCCESS if PASS
 *
 * NOTES
 *
 * SEE ALSO
 *********/

/****f* OSMTest/osmt_run_trap64_65_flow
 * NAME
 * osmt_run_trap64_65_flow
 *
 * DESCRIPTION
 * Run the trap 64/65 test flow. This test is ran with
 * an outside tool.
 *
 * SYNOPSIS
 */
ib_api_status_t osmt_run_trap64_65_flow(IN osmtest_t * const p_osmt);
/*
 * PARAMETERS
 *  p_osmt
 *    [in] Pointer to the osmtest obj
 *
 * RETURN VALUES
 * IB_SUCCESS if PASS
 *
 * NOTES
 *
 * SEE ALSO
 *********/

ib_api_status_t
osmtest_get_all_recs(IN osmtest_t * const p_osmt,
		     IN ib_net16_t const attr_id,
		     IN size_t const attr_size,
		     IN OUT osmtest_req_context_t * const p_context);

ib_api_status_t
osmtest_get_local_port_lmc(IN osmtest_t * const p_osmt,
			   IN ib_net16_t lid, OUT uint8_t * const p_lmc);

/*
 * A few auxiliary macros for logging
 */

#define EXPECTING_ERRORS_START "[[ ===== Expecting Errors - START ===== "
#define EXPECTING_ERRORS_END   "   ===== Expecting Errors  -  END ===== ]]"

#endif				/* _OSMTEST_H_ */
