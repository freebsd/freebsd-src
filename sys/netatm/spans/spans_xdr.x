%/*
% *
% * ===================================
% * HARP  |  Host ATM Research Platform
% * ===================================
% *
% *
% * This Host ATM Research Platform ("HARP") file (the "Software") is
% * made available by Network Computing Services, Inc. ("NetworkCS")
% * "AS IS".  NetworkCS does not provide maintenance, improvements or
% * support of any kind.
% *
% * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
% * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
% * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
% * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
% * In no event shall NetworkCS be responsible for any damages, including
% * but not limited to consequential damages, arising from or relating to
% * any use of the Software or related support.
% *
% * Copyright 1994-1998 Network Computing Services, Inc.
% *
% * Copies of this Software may be made, however, the above copyright
% * notice must be reproduced on all copies.
% *
% *	@(#) $FreeBSD$
% *
% */
%
/*
 * SPANS Signalling Manager
 * ---------------------------
 *
 * SPANS Protocol Message XDR Specification
 *
 */

#ifdef RPC_HDR
%/*
% * SPANS Signalling Manager
% * ---------------------------
% *
% * SPANS Protocol Message Definitions
% *
% */
%
%#ifndef _SPANS_SPANS_XDR_H
%#define _SPANS_SPANS_XDR_H
%
%#include <rpc/types.h>
%
#endif

#ifdef RPC_XDR
%/*
% * SPANS Signalling Manager
% * ---------------------------
% *
% * SPANS Protocol Message XDR Routines
% *
% */
%
%#ifndef lint
%static const char RCSid[] = "@(#) $FreeBSD$";
%#endif
%
#endif


/*
 * SPANS Signalling
 */
const	SPANS_SIG_VPI = 0;		/* Signalling VPI */
const	SPANS_SIG_VCI = 15;		/* Signalling VCI */
const	SPANS_CLS_VPI = 0;		/* Connectionless VPI */
const	SPANS_CLS_VCI = 14;		/* Connectionless VCI */

const	SPANS_MIN_VCI = 32;		/* Lowest VCI to allocate */
const	SPANS_MAX_VCI = 1023;		/* Highest VCI to allocate */
const	SPANS_VPI = 0;			/* Only VPI to allocate */

/*
 * SPANS Protocol Version
 *
 *	Major_version * 256 + Minor_version
 */
typedef u_int	spans_version;

const	SPANS_VERS_1_0 = 0x0100;	/* Version 1.0 */


/*
 * VPI/VCI
 *
 * Format:
 *	4 bits - unused
 *	12 bits - VPI value
 *	16 bits - VCI value
 */
typedef	u_int	spans_vpvc;		/* VPI/VCI value */

#ifdef RPC_HDR
%#define	SPANS_EXTRACT_VPI(p)	(((p) >> 16) & 0x0FFF)
%#define	SPANS_EXTRACT_VCI(p)	((p) & 0x0FFFF)
%#define	SPANS_PACK_VPIVCI(p, c)	((((p) & 0x0FFF) << 16) | ((c) & 0x0FFFF))
#endif


/*
 * VPI/VCI Preference
 */
struct spans_vpvc_pref {
	bool		vpf_valid;	/* VPI/VCI values valid */
	spans_vpvc	vpf_vpvc;	/* VPI/VCI value */
};


/*
 * SPANS ATM Address
 */
struct spans_addr {
	opaque		addr[8];	/* SPANS ATM address */
};


/*
 * Service Access Point (SAP)
 */
typedef u_int	spans_sap;		/* SAP value */

const	SPANS_SAP_IP = 1025;		/* TCP/IP */
const	SPANS_SAP_EPHEMERAL = 2048;	/* Start of ephemeral SAPs*/


/*
 * ATM Connection Identifier
 */
struct spans_atm_conn {
	spans_addr	con_dst;	/* Destination ATM address */
	spans_addr	con_src;	/* Source ATM address */
	spans_sap	con_dsap;	/* Destination SAP */
	spans_sap	con_ssap;	/* Source SAP */
};


/*
 * Connection Resources
 */
struct spans_resrc {
	u_int		rsc_peak;	/* Peak bandwidth (Kbps) */
	u_int		rsc_mean;	/* Mean bandwidth (Kbps) */
	u_int		rsc_burst;	/* Mean burst (Kb) */
};


/*
 * ATM Adaptation Layer (AAL) Types
 */
enum spans_aal {
	SPANS_AAL0 = 0,			/* NULL AAL */
	SPANS_AAL1 = 1,			/* AAL 1 */
	SPANS_AAL2 = 2,			/* AAL 2 */
	SPANS_AAL3 = 3,			/* AAL 3 */
	SPANS_AAL4 = 4,			/* AAL 4 */
	SPANS_AAL5 = 5			/* AAL 5 */
};


/*
 * Result Codes
 */
enum spans_result {
	SPANS_OK = 0,			/* Success */
	SPANS_FAIL = 1,			/* Failure */
	SPANS_NOVPVC = 2,		/* No VP/VC */
	SPANS_NORSC = 3,		/* No resources */
	SPANS_BADDEST = 4		/* Bad destination */
};


/*
 * Message Types
 */
enum spans_msgtype {
	/*
	 * SPANS UNI message types
	 */
	SPANS_STAT_REQ = 0,		/* Status request */
	SPANS_STAT_IND = 1,		/* Status indication */
	SPANS_STAT_RSP = 2,		/* Status response */
	SPANS_OPEN_REQ = 3,		/* Open request */
	SPANS_OPEN_IND = 4,		/* Open indication */
	SPANS_OPEN_RSP = 5,		/* Open response */
	SPANS_OPEN_CNF = 6,		/* Open confirmation */
	SPANS_CLOSE_REQ = 7,		/* Close request */
	SPANS_CLOSE_IND = 8,		/* Close indication */
	SPANS_CLOSE_RSP = 9,		/* Close response */
	SPANS_CLOSE_CNF = 10,		/* Close confirmation */
	SPANS_RCLOSE_REQ = 11,		/* Reverse close request */
	SPANS_RCLOSE_IND = 12,		/* Reverse close indication */
	SPANS_RCLOSE_RSP = 13,		/* Reverse close response */
	SPANS_RCLOSE_CNF = 14,		/* Reverse close confirmation */
	SPANS_MULTI_REQ = 15,		/* Multicast request */
	SPANS_MULTI_IND = 16,		/* Multicast indication */
	SPANS_MULTI_RSP = 17,		/* Multicast response */
	SPANS_MULTI_CNF = 18,		/* Multicast confirmation */
	SPANS_ADD_REQ = 19,		/* Add request */
	SPANS_ADD_IND = 20,		/* Add indication */
	SPANS_ADD_RSP = 21,		/* Add response */
	SPANS_ADD_CNF = 22,		/* Add confirmation */
	SPANS_JOIN_REQ = 23,		/* Join request */
	SPANS_JOIN_CNF = 24,		/* Join confirmation */
	SPANS_LEAVE_REQ = 25,		/* Leave request */
	SPANS_LEAVE_CNF = 26,		/* Leave confirmation */

	/*
	 * SPANS NNI message types
	 */
	SPANS_NSAP_IND = 99,		/* NSAP routing message */
	SPANS_MAP_IND = 100,		/* Topology message */
	SPANS_SETUP_REQ = 101,		/* Setup request */
	SPANS_SETUP_RSP = 102,		/* Setup response */
	SPANS_CHANGE_REQ = 103,		/* Change request */
	SPANS_CHANGE_RSP = 104,		/* Change response */
	SPANS_RELOC_REQ = 105,		/* Relocation request */
	SPANS_RELOC_RSP = 106,		/* Relocation response */
	SPANS_HELLO_IND = 107,		/* Hello message */

	SPANS_VCIR_IND = 108,		/* VCI range indication */
	SPANS_QUERY_REQ = 110,		/* Conn. state query request */
	SPANS_QUERY_RSP = 111		/* Conn. state query response */
};


/*
 * Query types
 */
enum spans_query_type {
	SPANS_QUERY_NORMAL,		/* Normal--respond */
	SPANS_QUERY_DEBUG,		/* Debug--respond with state */
	SPANS_QUERY_END_TO_END		/* Not implemented */
};


/*
 * SPANS connection states
 */
enum spans_conn_state {
	SPANS_CONN_OPEN,		/* Connection is open */
	SPANS_CONN_OPEN_PEND,		/* Connection is being opened */
	SPANS_CONN_CLOSE_PEND,		/* Connection is being closed */
	SPANS_CONN_CLOSED		/* Connection does not exist */
};


/*
 * Message Parameters
 *
 * There is a separate message parameter structure for each 
 * message type.
 */
struct spans_parm_stat_req {
	u_long		streq_es_epoch;	/* End system epoch */
};

struct spans_parm_stat_ind {
	u_long		stind_sw_epoch;	/* Switch epoch */
	spans_addr	stind_es_addr;	/* End system ATM address */
	spans_addr	stind_sw_addr;	/* Switch ATM address */
};

struct spans_parm_stat_rsp {
	u_long		strsp_es_epoch;	/* End system epoch */
	spans_addr	strsp_es_addr;	/* End system ATM address */
};

struct spans_parm_open_req {
	spans_atm_conn	opreq_conn;	/* Connection identity */
	spans_aal	opreq_aal;	/* AAL type */
	spans_resrc	opreq_desrsrc;	/* Desired resources */
	spans_resrc	opreq_minrsrc;	/* Minimum resources */
	spans_vpvc_pref	opreq_vpvc;	/* VPI/VCI preference */
};

struct spans_parm_open_ind {
	spans_atm_conn	opind_conn;	/* Connection identity */
	spans_aal	opind_aal;	/* AAL type */
	spans_resrc	opind_desrsrc;	/* Desired resources */
	spans_resrc	opind_minrsrc;	/* Minimum resources */
	spans_vpvc_pref	opind_vpvc;	/* VPI/VCI preference */
};

struct spans_parm_open_rsp {
	spans_atm_conn	oprsp_conn;	/* Connection identity */
	spans_result	oprsp_result;	/* Open result */
	spans_resrc	oprsp_rsrc;	/* Allocated resources */
	spans_vpvc	oprsp_vpvc;	/* Allocated VPI/VCI */
};

struct spans_parm_open_cnf {
	spans_atm_conn	opcnf_conn;	/* Connection identity */
	spans_result	opcnf_result;	/* Open result */
	spans_resrc	opcnf_rsrc;	/* Allocated resources */
	spans_vpvc	opcnf_vpvc;	/* Allocated VPI/VCI */
};

struct spans_parm_close_req {
	spans_atm_conn	clreq_conn;	/* Connection identity */
};

struct spans_parm_close_ind {
	spans_atm_conn	clind_conn;	/* Connection identity */
};

struct spans_parm_close_rsp {
	spans_atm_conn	clrsp_conn;	/* Connection identity */
	spans_result	clrsp_result;	/* Close result */
};

struct spans_parm_close_cnf {
	spans_atm_conn	clcnf_conn;	/* Connection identity */
	spans_result	clcnf_result;	/* Close result */
};

struct spans_parm_rclose_req {
	spans_atm_conn	rcreq_conn;	/* Connection identity */
};

struct spans_parm_rclose_ind {
	spans_atm_conn	rcind_conn;	/* Connection identity */
};

struct spans_parm_rclose_rsp {
	spans_atm_conn	rcrsp_conn;	/* Connection identity */
	spans_result	rcrsp_result;	/* Rclose result */
};

struct spans_parm_rclose_cnf {
	spans_atm_conn	rccnf_conn;	/* Connection identity */
	spans_result	rccnf_result;	/* Rclose result */
};

struct spans_parm_multi_req {
	spans_atm_conn	mureq_conn;	/* Connection identity */
	spans_aal	mureq_aal;	/* AAL type */
	spans_resrc	mureq_desrsrc;	/* Desired resources */
	spans_resrc	mureq_minrsrc;	/* Minimum resources */
	spans_vpvc	mureq_vpvc;	/* VPI/VCI preference */
};

struct spans_parm_multi_ind {
	spans_atm_conn	muind_conn;	/* Connection identity */
	spans_aal	muind_aal;	/* AAL type */
	spans_resrc	muind_desrsrc;	/* Desired resources */
	spans_resrc	muind_minrsrc;	/* Minimum resources */
	spans_vpvc	muind_vpvc;	/* VPI/VCI preference */
};

struct spans_parm_multi_rsp {
	spans_atm_conn	mursp_conn;	/* Connection identity */
	spans_result	mursp_result;	/* Multi result */
	spans_resrc	mursp_rsrc;	/* Allocated resources */
	spans_vpvc	mursp_vpvc;	/* Allocated VPI/VCI */
};

struct spans_parm_multi_cnf {
	spans_atm_conn	mucnf_conn;	/* Connection identity */
	spans_result	mucnf_result;	/* Multi result */
	spans_resrc	mucnf_rsrc;	/* Allocated resources */
	spans_vpvc	mucnf_vpvc;	/* Allocated VPI/VCI */
};

struct spans_parm_add_req {
	spans_atm_conn	adreq_desconn;	/* Desired connection identity */
	spans_atm_conn	adreq_xstconn;	/* Existing connection identity */
};

struct spans_parm_add_ind {
	spans_atm_conn	adind_desconn;	/* Desired connection identity */
	spans_atm_conn	adind_xstconn;	/* Existing connection identity */
};

struct spans_parm_add_rsp {
	spans_atm_conn	adrsp_conn;	/* Connection identity */
	spans_result	adrsp_result;	/* Add result */
	spans_resrc	adrsp_rsrc;	/* Allocated resources */
};

struct spans_parm_add_cnf {
	spans_atm_conn	adcnf_conn;	/* Connection identity */
	spans_result	adcnf_result;	/* Add result */
	spans_resrc	adcnf_rsrc;	/* Allocated resources */
};

struct spans_parm_join_req {
	spans_addr	jnreq_addr;	/* Group address */
};

struct spans_parm_join_cnf {
	spans_addr	jncnf_addr;	/* Group address */
	spans_result	jncnf_result;	/* Join result */
};

struct spans_parm_leave_req {
	spans_addr	lvreq_addr;	/* Group address */
};

struct spans_parm_leave_cnf {
	spans_addr	lvcnf_addr;	/* Group address */
	spans_result	lvcnf_result;	/* Leave result */
};

struct spans_parm_vcir_ind {
	u_int		vrind_min;	/* Lowest VCI available */
	u_int		vrind_max;	/* Highest VCI available */
};

struct spans_parm_query_req {
	spans_atm_conn	qyreq_conn;	/* Conn. being queried */
	spans_query_type	qyreq_type;	/* Query type */
};

struct spans_parm_query_rsp {
	spans_atm_conn	qyrsp_conn;	/* Conn. being queried */
	spans_query_type	qyrsp_type;	/* Query type */
	spans_conn_state	qyrsp_state;	/* Conn. state */
	u_int		qyrsp_data;	/* Extra state data */
};


/*
 * Message Body
 */
union spans_msgbody switch (spans_msgtype mb_type) {

case SPANS_STAT_REQ:		spans_parm_stat_req	mb_stat_req;
case SPANS_STAT_IND:		spans_parm_stat_ind	mb_stat_ind;
case SPANS_STAT_RSP:		spans_parm_stat_rsp	mb_stat_rsp;
case SPANS_OPEN_REQ:		spans_parm_open_req	mb_open_req;
case SPANS_OPEN_IND:		spans_parm_open_ind	mb_open_ind;
case SPANS_OPEN_RSP:		spans_parm_open_rsp	mb_open_rsp;
case SPANS_OPEN_CNF:		spans_parm_open_cnf	mb_open_cnf;
case SPANS_CLOSE_REQ:		spans_parm_close_req	mb_close_req;
case SPANS_CLOSE_IND:		spans_parm_close_ind	mb_close_ind;
case SPANS_CLOSE_RSP:		spans_parm_close_rsp	mb_close_rsp;
case SPANS_CLOSE_CNF:		spans_parm_close_cnf	mb_close_cnf;
case SPANS_RCLOSE_REQ:		spans_parm_rclose_req	mb_rclose_req;
case SPANS_RCLOSE_IND:		spans_parm_rclose_ind	mb_rclose_ind;
case SPANS_RCLOSE_RSP:		spans_parm_rclose_rsp	mb_rclose_rsp;
case SPANS_RCLOSE_CNF:		spans_parm_rclose_cnf	mb_rclose_cnf;
case SPANS_MULTI_REQ:		spans_parm_multi_req	mb_multi_req;
case SPANS_MULTI_IND:		spans_parm_multi_ind	mb_multi_ind;
case SPANS_MULTI_RSP:		spans_parm_multi_rsp	mb_multi_rsp;
case SPANS_MULTI_CNF:		spans_parm_multi_cnf	mb_multi_cnf;
case SPANS_ADD_REQ:		spans_parm_add_req	mb_add_req;
case SPANS_ADD_IND:		spans_parm_add_ind	mb_add_ind;
case SPANS_ADD_RSP:		spans_parm_add_rsp	mb_add_rsp;
case SPANS_ADD_CNF:		spans_parm_add_cnf	mb_add_cnf;
case SPANS_JOIN_REQ:		spans_parm_join_req	mb_join_req;
case SPANS_JOIN_CNF:		spans_parm_join_cnf	mb_join_cnf;
case SPANS_LEAVE_REQ:		spans_parm_leave_req	mb_leave_req;
case SPANS_LEAVE_CNF:		spans_parm_leave_cnf	mb_leave_cnf;
case SPANS_VCIR_IND:		spans_parm_vcir_ind	mb_vcir_ind;
case SPANS_QUERY_REQ:		spans_parm_query_req	mb_query_req;
case SPANS_QUERY_RSP:		spans_parm_query_rsp	mb_query_rsp;
};


/*
 * Message Format
 */
struct spans_msg {
	spans_version		sm_vers;
	spans_msgbody		sm_body;
};

#ifdef RPC_HDR
%#define	sm_type		sm_body.mb_type
%#define	sm_stat_req	sm_body.spans_msgbody_u.mb_stat_req
%#define	sm_stat_ind	sm_body.spans_msgbody_u.mb_stat_ind
%#define	sm_stat_rsp	sm_body.spans_msgbody_u.mb_stat_rsp
%#define	sm_open_req	sm_body.spans_msgbody_u.mb_open_req
%#define	sm_open_ind	sm_body.spans_msgbody_u.mb_open_ind
%#define	sm_open_rsp	sm_body.spans_msgbody_u.mb_open_rsp
%#define	sm_open_cnf	sm_body.spans_msgbody_u.mb_open_cnf
%#define	sm_close_req	sm_body.spans_msgbody_u.mb_close_req
%#define	sm_close_ind	sm_body.spans_msgbody_u.mb_close_ind
%#define	sm_close_rsp	sm_body.spans_msgbody_u.mb_close_rsp
%#define	sm_close_cnf	sm_body.spans_msgbody_u.mb_close_cnf
%#define	sm_rclose_req	sm_body.spans_msgbody_u.mb_rclose_req
%#define	sm_rclose_ind	sm_body.spans_msgbody_u.mb_rclose_ind
%#define	sm_rclose_rsp	sm_body.spans_msgbody_u.mb_rclose_rsp
%#define	sm_rclose_cnf	sm_body.spans_msgbody_u.mb_rclose_cnf
%#define	sm_multi_req	sm_body.spans_msgbody_u.mb_multi_req
%#define	sm_multi_ind	sm_body.spans_msgbody_u.mb_multi_ind
%#define	sm_multi_rsp	sm_body.spans_msgbody_u.mb_multi_rsp
%#define	sm_multi_cnf	sm_body.spans_msgbody_u.mb_multi_cnf
%#define	sm_add_req	sm_body.spans_msgbody_u.mb_add_req
%#define	sm_add_ind	sm_body.spans_msgbody_u.mb_add_ind
%#define	sm_add_rsp	sm_body.spans_msgbody_u.mb_add_rsp
%#define	sm_add_cnf	sm_body.spans_msgbody_u.mb_add_cnf
%#define	sm_join_req	sm_body.spans_msgbody_u.mb_join_req
%#define	sm_join_cnf	sm_body.spans_msgbody_u.mb_join_cnf
%#define	sm_leave_req	sm_body.spans_msgbody_u.mb_leave_req
%#define	sm_leave_cnf	sm_body.spans_msgbody_u.mb_leave_cnf
%#define	sm_vcir_ind	sm_body.spans_msgbody_u.mb_vcir_ind
%#define	sm_query_req	sm_body.spans_msgbody_u.mb_query_req
%#define	sm_query_rsp	sm_body.spans_msgbody_u.mb_query_rsp
#endif

#ifdef RPC_HDR
%#endif	/* _SPANS_SPANS_XDR_H */
#endif
