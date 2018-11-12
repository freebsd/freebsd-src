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
 *    Implementation of inform record functions.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <complib/cl_debug.h>
#include <opensm/osm_helper.h>
#include <opensm/osm_inform.h>
#include <vendor/osm_vendor_api.h>
#include <opensm/osm_pkey.h>
#include <opensm/osm_sa.h>
#include <sys/socket.h>

typedef struct osm_infr_match_ctxt {
	cl_list_t *p_remove_infr_list;
	ib_mad_notice_attr_t *p_ntc;
} osm_infr_match_ctxt_t;

/**********************************************************************
 **********************************************************************/
void osm_infr_delete(IN osm_infr_t * const p_infr)
{
	free(p_infr);
}

/**********************************************************************
 **********************************************************************/
osm_infr_t *osm_infr_new(IN const osm_infr_t * p_infr_rec)
{
	osm_infr_t *p_infr;

	CL_ASSERT(p_infr_rec);

	p_infr = (osm_infr_t *) malloc(sizeof(osm_infr_t));
	if (p_infr)
		memcpy(p_infr, p_infr_rec, sizeof(osm_infr_t));

	return (p_infr);
}

/**********************************************************************
 **********************************************************************/
static void dump_all_informs(IN osm_subn_t const *p_subn, IN osm_log_t * p_log)
{
	cl_list_item_t *p_list_item;

	if (!osm_log_is_active(p_log, OSM_LOG_DEBUG))
		return;

	p_list_item = cl_qlist_head(&p_subn->sa_infr_list);
	while (p_list_item != cl_qlist_end(&p_subn->sa_infr_list)) {
		osm_dump_inform_info(p_log,
				     &((osm_infr_t *) p_list_item)->
				     inform_record.inform_info, OSM_LOG_DEBUG);
		p_list_item = cl_qlist_next(p_list_item);
	}
}

/**********************************************************************
 * Match an infr by the InformInfo and Address vector
 **********************************************************************/
static cl_status_t
__match_inf_rec(IN const cl_list_item_t * const p_list_item, IN void *context)
{
	osm_infr_t *p_infr_rec = (osm_infr_t *) context;
	osm_infr_t *p_infr = (osm_infr_t *) p_list_item;
	osm_log_t *p_log = p_infr_rec->sa->p_log;
	cl_status_t status = CL_NOT_FOUND;
	ib_gid_t all_zero_gid;

	OSM_LOG_ENTER(p_log);

	if (memcmp(&p_infr->report_addr, &p_infr_rec->report_addr,
		   sizeof(p_infr_rec->report_addr))) {
		OSM_LOG(p_log, OSM_LOG_DEBUG, "Differ by Address\n");
		goto Exit;
	}

	memset(&all_zero_gid, 0, sizeof(ib_gid_t));

	/* if inform_info.gid is not zero, ignore lid range */
	if (!memcmp(&p_infr_rec->inform_record.inform_info.gid, &all_zero_gid,
		    sizeof(p_infr_rec->inform_record.inform_info.gid))) {
		if (memcmp(&p_infr->inform_record.inform_info.gid,
			   &p_infr_rec->inform_record.inform_info.gid,
			   sizeof(p_infr->inform_record.inform_info.gid))) {
			OSM_LOG(p_log, OSM_LOG_DEBUG,
				"Differ by InformInfo.gid\n");
			goto Exit;
		}
	} else {
		if ((p_infr->inform_record.inform_info.lid_range_begin !=
		     p_infr_rec->inform_record.inform_info.lid_range_begin) ||
		    (p_infr->inform_record.inform_info.lid_range_end !=
		     p_infr_rec->inform_record.inform_info.lid_range_end)) {
			OSM_LOG(p_log, OSM_LOG_DEBUG,
				"Differ by InformInfo.LIDRange\n");
			goto Exit;
		}
	}

	if (p_infr->inform_record.inform_info.trap_type !=
	    p_infr_rec->inform_record.inform_info.trap_type) {
		OSM_LOG(p_log, OSM_LOG_DEBUG,
			"Differ by InformInfo.TrapType\n");
		goto Exit;
	}

	if (p_infr->inform_record.inform_info.is_generic !=
	    p_infr_rec->inform_record.inform_info.is_generic) {
		OSM_LOG(p_log, OSM_LOG_DEBUG,
			"Differ by InformInfo.IsGeneric\n");
		goto Exit;
	}

	if (p_infr->inform_record.inform_info.is_generic) {
		if (p_infr->inform_record.inform_info.g_or_v.generic.trap_num !=
		    p_infr_rec->inform_record.inform_info.g_or_v.generic.
		    trap_num)
			OSM_LOG(p_log, OSM_LOG_DEBUG,
				"Differ by InformInfo.Generic.TrapNumber\n");
		else if (p_infr->inform_record.inform_info.g_or_v.generic.
			 qpn_resp_time_val !=
			 p_infr_rec->inform_record.inform_info.g_or_v.generic.
			 qpn_resp_time_val)
			OSM_LOG(p_log, OSM_LOG_DEBUG,
				"Differ by InformInfo.Generic.QPNRespTimeVal\n");
		else if (p_infr->inform_record.inform_info.g_or_v.generic.
			 node_type_msb !=
			 p_infr_rec->inform_record.inform_info.g_or_v.generic.
			 node_type_msb)
			OSM_LOG(p_log, OSM_LOG_DEBUG,
				"Differ by InformInfo.Generic.NodeTypeMSB\n");
		else if (p_infr->inform_record.inform_info.g_or_v.generic.
			 node_type_lsb !=
			 p_infr_rec->inform_record.inform_info.g_or_v.generic.
			 node_type_lsb)
			OSM_LOG(p_log, OSM_LOG_DEBUG,
				"Differ by InformInfo.Generic.NodeTypeLSB\n");
		else
			status = CL_SUCCESS;
	} else {
		if (p_infr->inform_record.inform_info.g_or_v.vend.dev_id !=
		    p_infr_rec->inform_record.inform_info.g_or_v.vend.dev_id)
			OSM_LOG(p_log, OSM_LOG_DEBUG,
				"Differ by InformInfo.Vendor.DeviceID\n");
		else if (p_infr->inform_record.inform_info.g_or_v.vend.
			 qpn_resp_time_val !=
			 p_infr_rec->inform_record.inform_info.g_or_v.vend.
			 qpn_resp_time_val)
			OSM_LOG(p_log, OSM_LOG_DEBUG,
				"Differ by InformInfo.Vendor.QPNRespTimeVal\n");
		else if (p_infr->inform_record.inform_info.g_or_v.vend.
			 vendor_id_msb !=
			 p_infr_rec->inform_record.inform_info.g_or_v.vend.
			 vendor_id_msb)
			OSM_LOG(p_log, OSM_LOG_DEBUG,
				"Differ by InformInfo.Vendor.VendorIdMSB\n");
		else if (p_infr->inform_record.inform_info.g_or_v.vend.
			 vendor_id_lsb !=
			 p_infr_rec->inform_record.inform_info.g_or_v.vend.
			 vendor_id_lsb)
			OSM_LOG(p_log, OSM_LOG_DEBUG,
				"Differ by InformInfo.Vendor.VendorIdLSB\n");
		else
			status = CL_SUCCESS;
	}

Exit:
	OSM_LOG_EXIT(p_log);
	return status;
}

/**********************************************************************
 **********************************************************************/
osm_infr_t *osm_infr_get_by_rec(IN osm_subn_t const *p_subn,
				IN osm_log_t * p_log,
				IN osm_infr_t * const p_infr_rec)
{
	cl_list_item_t *p_list_item;

	OSM_LOG_ENTER(p_log);

	dump_all_informs(p_subn, p_log);

	OSM_LOG(p_log, OSM_LOG_DEBUG, "Looking for Inform Record\n");
	osm_dump_inform_info(p_log, &(p_infr_rec->inform_record.inform_info),
			     OSM_LOG_DEBUG);
	OSM_LOG(p_log, OSM_LOG_DEBUG, "InformInfo list size %d\n",
		cl_qlist_count(&p_subn->sa_infr_list));

	p_list_item = cl_qlist_find_from_head(&p_subn->sa_infr_list,
					      __match_inf_rec, p_infr_rec);

	if (p_list_item == cl_qlist_end(&p_subn->sa_infr_list))
		p_list_item = NULL;

	OSM_LOG_EXIT(p_log);
	return (osm_infr_t *) p_list_item;
}

/**********************************************************************
 **********************************************************************/
void
osm_infr_insert_to_db(IN osm_subn_t * p_subn,
		      IN osm_log_t * p_log, IN osm_infr_t * p_infr)
{
	OSM_LOG_ENTER(p_log);

	OSM_LOG(p_log, OSM_LOG_DEBUG,
		"Inserting new InformInfo Record into Database\n");
	OSM_LOG(p_log, OSM_LOG_DEBUG, "Dump before insertion (size %d)\n",
		cl_qlist_count(&p_subn->sa_infr_list));
	dump_all_informs(p_subn, p_log);

#if 0
	osm_dump_inform_info(p_log,
			     &(p_infr->inform_record.inform_info),
			     OSM_LOG_DEBUG);
#endif

	cl_qlist_insert_head(&p_subn->sa_infr_list, &p_infr->list_item);

	OSM_LOG(p_log, OSM_LOG_DEBUG, "Dump after insertion (size %d)\n",
		cl_qlist_count(&p_subn->sa_infr_list));
	dump_all_informs(p_subn, p_log);
	OSM_LOG_EXIT(p_log);
}

/**********************************************************************
 **********************************************************************/
void
osm_infr_remove_from_db(IN osm_subn_t * p_subn,
			IN osm_log_t * p_log, IN osm_infr_t * p_infr)
{
	char gid_str[INET6_ADDRSTRLEN];
	OSM_LOG_ENTER(p_log);

	OSM_LOG(p_log, OSM_LOG_DEBUG, "Removing InformInfo Subscribing GID:%s"
		" Enum:0x%X from Database\n",
		inet_ntop(AF_INET6, p_infr->inform_record.subscriber_gid.raw,
			gid_str, sizeof gid_str),
		p_infr->inform_record.subscriber_enum);

	osm_dump_inform_info(p_log, &(p_infr->inform_record.inform_info),
			     OSM_LOG_DEBUG);

	cl_qlist_remove_item(&p_subn->sa_infr_list, &p_infr->list_item);

	osm_infr_delete(p_infr);

	OSM_LOG_EXIT(p_log);
}

/**********************************************************************
 * Send a report:
 * Given a target address to send to and the notice.
 * We need to send SubnAdmReport
 **********************************************************************/
static ib_api_status_t __osm_send_report(IN osm_infr_t * p_infr_rec,	/* the informinfo */
					 IN ib_mad_notice_attr_t * p_ntc	/* notice to send */
    )
{
	osm_madw_t *p_report_madw;
	ib_mad_notice_attr_t *p_report_ntc;
	ib_mad_t *p_mad;
	ib_sa_mad_t *p_sa_mad;
	static atomic32_t trap_fwd_trans_id = 0x02DAB000;
	ib_api_status_t status = IB_SUCCESS;
	osm_log_t *p_log = p_infr_rec->sa->p_log;

	OSM_LOG_ENTER(p_log);

	/* HACK: who switches or uses the src and dest GIDs in the grh_info ?? */

	/* it is better to use LIDs since the GIDs might not be there for SMI traps */
	OSM_LOG(p_log, OSM_LOG_DEBUG, "Forwarding Notice Event from LID:%u"
		" to InformInfo LID: %u TID:0x%X\n",
		cl_ntoh16(p_ntc->issuer_lid),
		cl_ntoh16(p_infr_rec->report_addr.dest_lid), trap_fwd_trans_id);

	/* get the MAD to send */
	p_report_madw = osm_mad_pool_get(p_infr_rec->sa->p_mad_pool,
					 p_infr_rec->h_bind, MAD_BLOCK_SIZE,
					 &(p_infr_rec->report_addr));

	p_report_madw->resp_expected = TRUE;

	if (!p_report_madw) {
		OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0203"
			"osm_mad_pool_get failed\n");
		status = IB_ERROR;
		goto Exit;
	}

	/* advance trap trans id (cant simply ++ on some systems inside ntoh) */
	p_mad = osm_madw_get_mad_ptr(p_report_madw);
	ib_mad_init_new(p_mad, IB_MCLASS_SUBN_ADM, 2, IB_MAD_METHOD_REPORT,
			cl_hton64((uint64_t) cl_atomic_inc(&trap_fwd_trans_id)),
			IB_MAD_ATTR_NOTICE, 0);

	p_sa_mad = osm_madw_get_sa_mad_ptr(p_report_madw);

	p_report_ntc = (ib_mad_notice_attr_t *) & (p_sa_mad->data);

	/* copy the notice */
	*p_report_ntc = *p_ntc;

	/* The TRUE is for: response is expected */
	osm_sa_send(p_infr_rec->sa, p_report_madw, TRUE);

Exit:
	OSM_LOG_EXIT(p_log);
	return (status);
}

/**********************************************************************
 * This routine compares a given Notice and a ListItem of InformInfo type.
 * PREREQUISITE:
 * The Notice.GID should be pre-filled with the trap generator GID
 **********************************************************************/
static void
__match_notice_to_inf_rec(IN cl_list_item_t * const p_list_item,
			  IN void *context)
{
	osm_infr_match_ctxt_t *p_infr_match = (osm_infr_match_ctxt_t *) context;
	ib_mad_notice_attr_t *p_ntc = p_infr_match->p_ntc;
	cl_list_t *p_infr_to_remove_list = p_infr_match->p_remove_infr_list;
	osm_infr_t *p_infr_rec = (osm_infr_t *) p_list_item;
	ib_inform_info_t *p_ii = &(p_infr_rec->inform_record.inform_info);
	cl_status_t status = CL_NOT_FOUND;
	osm_log_t *p_log = p_infr_rec->sa->p_log;
	osm_subn_t *p_subn = p_infr_rec->sa->p_subn;
	ib_gid_t source_gid;
	osm_port_t *p_src_port;
	osm_port_t *p_dest_port;

	OSM_LOG_ENTER(p_log);

	/* matching rules
	 * InformInfo   Notice
	 * GID          IssuerGID    if non zero must match the trap
	 * LIDRange     IssuerLID    apply only if GID=0
	 * IsGeneric    IsGeneric    is compulsory and must match the trap
	 * Type         Type         if not 0xFFFF must match
	 * TrapNumber   TrapNumber   if not 0xFFFF must match
	 * DeviceId     DeviceID     if not 0xFFFF must match
	 * QPN dont care
	 * ProducerType ProducerType match or 0xFFFFFF // EZ: actually my interpretation
	 * VendorID     VendorID     match or 0xFFFFFF
	 */

	/* GID          IssuerGID    if non zero must match the trap  */
	if (p_ii->gid.unicast.prefix != 0
	    || p_ii->gid.unicast.interface_id != 0) {
		/* match by GID */
		if (memcmp(&(p_ii->gid), &(p_ntc->issuer_gid),
			   sizeof(ib_gid_t))) {
			OSM_LOG(p_log, OSM_LOG_DEBUG, "Mismatch by GID\n");
			goto Exit;
		}
	} else {
		/* LIDRange     IssuerLID    apply only if GID=0 */
		/* If lid_range_begin of the informInfo is 0xFFFF - then it should be ignored. */
		if (p_ii->lid_range_begin != 0xFFFF) {
			/* a real lid range is given - check it */
			if ((cl_hton16(p_ii->lid_range_begin) >
			     cl_hton16(p_ntc->issuer_lid))
			    || (cl_hton16(p_ntc->issuer_lid) >
				cl_hton16(p_ii->lid_range_end))) {
				OSM_LOG(p_log, OSM_LOG_DEBUG,
					"Mismatch by LID Range. Needed: %u <= %u <= %u\n",
					cl_hton16(p_ii->lid_range_begin),
					cl_hton16(p_ntc->issuer_lid),
					cl_hton16(p_ii->lid_range_end));
				goto Exit;
			}
		}
	}

	/* IsGeneric    IsGeneric    is compulsory and must match the trap  */
	if ((p_ii->is_generic && !ib_notice_is_generic(p_ntc)) ||
	    (!p_ii->is_generic && ib_notice_is_generic(p_ntc))) {
		OSM_LOG(p_log, OSM_LOG_DEBUG, "Mismatch by Generic/Vendor\n");
		goto Exit;
	}

	/* Type         Type         if not 0xFFFF must match */
	if ((p_ii->trap_type != 0xFFFF) &&
	    (cl_ntoh16(p_ii->trap_type) != ib_notice_get_type(p_ntc))) {
		OSM_LOG(p_log, OSM_LOG_DEBUG, "Mismatch by Type\n");
		goto Exit;
	}

	/* based on generic type */
	if (p_ii->is_generic) {
		/* TrapNumber   TrapNumber   if not 0xFFFF must match */
		if ((p_ii->g_or_v.generic.trap_num != 0xFFFF) &&
		    (p_ii->g_or_v.generic.trap_num !=
		     p_ntc->g_or_v.generic.trap_num)) {
			OSM_LOG(p_log, OSM_LOG_DEBUG, "Mismatch by Trap Num\n");
			goto Exit;
		}

		/* ProducerType ProducerType match or 0xFFFFFF  */
		if ((cl_ntoh32(ib_inform_info_get_prod_type(p_ii)) != 0xFFFFFF)
		    && (ib_inform_info_get_prod_type(p_ii) !=
			ib_notice_get_prod_type(p_ntc))) {
			OSM_LOG(p_log, OSM_LOG_DEBUG,
				"Mismatch by Node Type: II=0x%06X (%s) Trap=0x%06X (%s)\n",
				cl_ntoh32(ib_inform_info_get_prod_type(p_ii)),
				ib_get_producer_type_str
				(ib_inform_info_get_prod_type(p_ii)),
				cl_ntoh32(ib_notice_get_prod_type(p_ntc)),
				ib_get_producer_type_str(ib_notice_get_prod_type
							 (p_ntc)));
			goto Exit;
		}
	} else {
		/* DeviceId     DeviceID     if not 0xFFFF must match */
		if ((p_ii->g_or_v.vend.dev_id != 0xFFFF) &&
		    (p_ii->g_or_v.vend.dev_id != p_ntc->g_or_v.vend.dev_id)) {
			OSM_LOG(p_log, OSM_LOG_DEBUG, "Mismatch by Dev Id\n");
			goto Exit;
		}

		/* VendorID     VendorID     match or 0xFFFFFF  */
		if ((ib_inform_info_get_vend_id(p_ii) != CL_HTON32(0xFFFFFF)) &&
		    (ib_inform_info_get_vend_id(p_ii) !=
		     ib_notice_get_vend_id(p_ntc))) {
			OSM_LOG(p_log, OSM_LOG_DEBUG, "Mismatch by Vendor ID\n");
			goto Exit;
		}
	}

	/* Check if there is a pkey match. o13-17.1.1 */
	/* Check if the issuer of the trap is the SM. If it is, then the gid
	   comparison should be done on the trap source (saved as the gid in the
	   data details field).
	   If the issuer gid is not the SM - then it is the guid of the trap
	   source */
	if ((cl_ntoh64(p_ntc->issuer_gid.unicast.prefix) ==
	     p_subn->opt.subnet_prefix)
	    && (cl_ntoh64(p_ntc->issuer_gid.unicast.interface_id) ==
		p_subn->sm_port_guid))
		/* The issuer is the SM then this is trap 64-67 - compare the gid
		   with the gid saved on the data details */
		source_gid = p_ntc->data_details.ntc_64_67.gid;
	else
		source_gid = p_ntc->issuer_gid;

	p_src_port =
	    osm_get_port_by_guid(p_subn, source_gid.unicast.interface_id);
	if (!p_src_port) {
		OSM_LOG(p_log, OSM_LOG_INFO,
			"Cannot find source port with GUID:0x%016" PRIx64 "\n",
			cl_ntoh64(source_gid.unicast.interface_id));
		goto Exit;
	}

	p_dest_port =
	    cl_ptr_vector_get(&p_subn->port_lid_tbl,
			      cl_ntoh16(p_infr_rec->report_addr.dest_lid));
	if (!p_dest_port) {
		OSM_LOG(p_log, OSM_LOG_INFO,
			"Cannot find destination port with LID:%u\n",
			cl_ntoh16(p_infr_rec->report_addr.dest_lid));
		goto Exit;
	}

	if (osm_port_share_pkey(p_log, p_src_port, p_dest_port) == FALSE) {
		OSM_LOG(p_log, OSM_LOG_DEBUG, "Mismatch by Pkey\n");
		/* According to o13-17.1.2 - If this informInfo does not have
		   lid_range_begin of 0xFFFF, then this informInfo request
		   should be removed from database */
		if (p_ii->lid_range_begin != 0xFFFF) {
			OSM_LOG(p_log, OSM_LOG_VERBOSE,
				"Pkey mismatch on lid_range_begin != 0xFFFF. "
				"Need to remove this informInfo from db\n");
			/* add the informInfo record to the remove_infr list */
			cl_list_insert_tail(p_infr_to_remove_list, p_infr_rec);
		}
		goto Exit;
	}

	/* send the report to the address provided in the inform record */
	OSM_LOG(p_log, OSM_LOG_DEBUG, "MATCH! Sending Report...\n");
	__osm_send_report(p_infr_rec, p_ntc);
	status = CL_SUCCESS;

Exit:
	OSM_LOG_EXIT(p_log);
}

/**********************************************************************
 * Once a Trap was received by osm_trap_rcv, or a Trap sourced by
 * the SM was sent (Traps 64-67), this routine is called with a copy of
 * the notice data.
 * Given a notice attribute - compare and see if it matches the InformInfo
 * element and if it does - call the Report(Notice) for the
 * target QP registered by the address stored in the InformInfo element
 **********************************************************************/
ib_api_status_t
osm_report_notice(IN osm_log_t * const p_log,
		  IN osm_subn_t * p_subn, IN ib_mad_notice_attr_t * p_ntc)
{
	char gid_str[INET6_ADDRSTRLEN];
	osm_infr_match_ctxt_t context;
	cl_list_t infr_to_remove_list;
	osm_infr_t *p_infr_rec;
	osm_infr_t *p_next_infr_rec;

	OSM_LOG_ENTER(p_log);

	/*
	 * we must make sure we are ready for this...
	 * note that the trap receivers might be initialized before
	 * the osm_infr_init call is performed.
	 */
	if (p_subn->sa_infr_list.state != CL_INITIALIZED) {
		OSM_LOG(p_log, OSM_LOG_DEBUG,
			"Ignoring Notice Reports since Inform List is not initialized yet!\n");
		return (IB_ERROR);
	}

	/* an official Event information log */
	if (ib_notice_is_generic(p_ntc))
		OSM_LOG(p_log, OSM_LOG_INFO,
			"Reporting Generic Notice type:%u num:%u (%s)"
			" from LID:%u GID:%s\n",
			ib_notice_get_type(p_ntc),
			cl_ntoh16(p_ntc->g_or_v.generic.trap_num),
			ib_get_trap_str(p_ntc->g_or_v.generic.trap_num),
			cl_ntoh16(p_ntc->issuer_lid),
			inet_ntop(AF_INET6, p_ntc->issuer_gid.raw, gid_str,
				sizeof gid_str));
	else
		OSM_LOG(p_log, OSM_LOG_INFO,
			"Reporting Vendor Notice type:%u vend:%u dev:%u"
			" from LID:%u GID:%s\n",
			ib_notice_get_type(p_ntc),
			cl_ntoh32(ib_notice_get_vend_id(p_ntc)),
			cl_ntoh16(p_ntc->g_or_v.vend.dev_id),
			cl_ntoh16(p_ntc->issuer_lid),
			inet_ntop(AF_INET6, p_ntc->issuer_gid.raw, gid_str,
				sizeof gid_str));

	/* Create a list that will hold all the infr records that should
	   be removed due to violation. o13-17.1.2 */
	cl_list_construct(&infr_to_remove_list);
	cl_list_init(&infr_to_remove_list, 5);
	context.p_remove_infr_list = &infr_to_remove_list;
	context.p_ntc = p_ntc;

	/* go over all inform info available at the subnet */
	/* try match to the given notice and send if match */
	cl_qlist_apply_func(&(p_subn->sa_infr_list),
			    __match_notice_to_inf_rec, &context);

	/* If we inserted items into the infr_to_remove_list - we need to
	   remove them */
	p_infr_rec = (osm_infr_t *) cl_list_remove_head(&infr_to_remove_list);
	while (p_infr_rec != NULL) {
		p_next_infr_rec =
		    (osm_infr_t *) cl_list_remove_head(&infr_to_remove_list);
		osm_infr_remove_from_db(p_subn, p_log, p_infr_rec);
		p_infr_rec = p_next_infr_rec;
	}
	cl_list_destroy(&infr_to_remove_list);

	OSM_LOG_EXIT(p_log);

	return (IB_SUCCESS);
}
