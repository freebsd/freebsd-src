/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * SPANS Signalling Manager
 * ---------------------------
 *
 * SPANS Print Routines.
 *
 */

#include <netatm/kern_include.h>

#include "spans_xdr.h"
#include <netatm/spans/spans_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif

/*
 * If LONGPRINT is defined, every field of the SPANS message will be
 * printed.  If not, a shorter summary (useful for debugging without
 * swamping the console) is printed.
 */
/* #define LONGPRINT */

/*
 * Local functions
 */
static void	spans_msgtype_str __P((spans_msgtype *, char *, int));
static void	spans_print_msgbody __P((spans_msgbody *));
static void	spans_result_str __P((spans_result *, char *, int));

#ifdef LONGPRINT

static void	inc_indent __P((void));
static void	dec_indent __P((void));
static void	spans_aal_str __P((spans_aal *, char *, int));
static void	spans_query_type_str __P((spans_query_type *, char *, int));
static void	spans_state_str __P((spans_query_type *, char *, int));
static void	spans_print_version __P((spans_version *));
static void	spans_print_vpvc __P((spans_vpvc *));
static void	spans_print_vpvc_pref __P((spans_vpvc_pref *));
static void	spans_print_addr __P((spans_addr *));
static void	spans_print_sap __P((spans_sap *));
static void	spans_print_atm_conn __P((spans_atm_conn *));
static void	spans_print_resrc __P((spans_resrc *));
static void	spans_print_aal __P((spans_aal *));
static void	spans_print_result __P((spans_result *));
static void	spans_print_msgtype __P((spans_msgtype *));
static void	spans_print_parm_stat_req __P((spans_parm_stat_req *));
static void	spans_print_parm_stat_ind __P((spans_parm_stat_ind *));
static void	spans_print_parm_stat_rsp __P((spans_parm_stat_rsp *));
static void	spans_print_parm_open_req __P((spans_parm_open_req *));
static void	spans_print_parm_open_ind __P((spans_parm_open_ind *));
static void	spans_print_parm_open_rsp __P((spans_parm_open_rsp *));
static void	spans_print_parm_open_cnf __P((spans_parm_open_cnf *));
static void	spans_print_parm_close_req __P((spans_parm_close_req *));
static void	spans_print_parm_close_ind __P((spans_parm_close_ind *));
static void	spans_print_parm_close_rsp __P((spans_parm_close_rsp *));
static void	spans_print_parm_close_cnf __P((spans_parm_close_cnf *));
static void	spans_print_parm_rclose_req __P((spans_parm_rclose_req *));
static void	spans_print_parm_rclose_ind __P((spans_parm_rclose_ind *));
static void	spans_print_parm_rclose_rsp __P((spans_parm_rclose_rsp *));
static void	spans_print_parm_rclose_cnf __P((spans_parm_rclose_cnf *));
static void	spans_print_parm_multi_req __P((spans_parm_multi_req *));
static void	spans_print_parm_multi_ind __P((spans_parm_multi_ind *));
static void	spans_print_parm_multi_rsp __P((spans_parm_multi_rsp *));
static void	spans_print_parm_multi_cnf __P((spans_parm_multi_cnf *));
static void	spans_print_parm_add_req __P((spans_parm_add_req *));
static void	spans_print_parm_add_ind __P((spans_parm_add_ind *));
static void	spans_print_parm_add_rsp __P((spans_parm_add_rsp *));
static void	spans_print_parm_add_cnf __P((spans_parm_add_cnf *));
static void	spans_print_parm_join_req __P((spans_parm_join_req *));
static void	spans_print_parm_join_cnf __P((spans_parm_join_cnf *));
static void	spans_print_parm_leave_req __P((spans_parm_leave_req *));
static void	spans_print_parm_leave_cnf __P((spans_parm_leave_cnf *));
static void	spans_print_parm_vcir_ind __P((spans_parm_vcir_ind *));
static void	spans_print_parm_query_req __P((spans_parm_query_req *));
static void	spans_print_parm_query_rsp __P((spans_parm_query_rsp *));


/*
 * Local variables
 */
#define	MAX_INDENT	10
#define	INIT_INDENT	&indent_str[MAX_INDENT]
static char *spans_indent;
static char indent_str[11] = "          ";

static void
inc_indent()
{
	if (spans_indent != &indent_str[0]) {
		spans_indent--;
	}
}

static void
dec_indent()
{
	if (spans_indent != INIT_INDENT) {
		spans_indent++;
	}
}

static void
spans_aal_str(objp, dest, len)
	spans_aal *objp;
	char *dest;
	int len;
{
	static char	*aal_names[] = {
		"SPANS_AAL0",
		"SPANS_AAL1",
		"SPANS_AAL2",
		"SPANS_AAL3",
		"SPANS_AAL4",
		"SPANS_AAL5"
	};

	if (*objp < SPANS_AAL0 || *objp > SPANS_AAL5) {
		snprintf(dest, len, "Invalid (%d)", (int)*objp);
	} else {
		snprintf(dest, len, "%s (%d)", aal_names[(int)*objp],
				(int)*objp);
	}
}

#endif 

static void
spans_result_str(objp, dest, len)
	spans_result *objp;
	char *dest;
	int len;
{
	static char	*result_names[] = {
		"SPANS_OK",
		"SPANS_FAIL",
		"SPANS_NOVPVC",
		"SPANS_NORSC",
		"SPANS_BADDEST"
	};

	if (*objp < SPANS_OK || *objp > SPANS_BADDEST) {
		snprintf(dest, len, "Invalid (%d)", (int)*objp);
	} else {
		snprintf(dest, len, "%s (%d)",
				result_names[(int)*objp], (int)*objp);
	}
}

static void
spans_msgtype_str(objp, dest, len)
	spans_msgtype *objp;
	char *dest;
	int len;
{
	int	i;

	static struct {
		spans_msgtype	type;
		char		*name;
	} msgtype_names[] = {
		{ SPANS_STAT_REQ,	"SPANS_STAT_REQ" },
		{ SPANS_STAT_IND,	"SPANS_STAT_IND" },
		{ SPANS_STAT_RSP,	"SPANS_STAT_RSP" },
		{ SPANS_OPEN_REQ,	"SPANS_OPEN_REQ" },
		{ SPANS_OPEN_IND,	"SPANS_OPEN_IND" },
		{ SPANS_OPEN_RSP,	"SPANS_OPEN_RSP" },
		{ SPANS_OPEN_CNF,	"SPANS_OPEN_CNF" },
		{ SPANS_CLOSE_REQ,	"SPANS_CLOSE_REQ" },
		{ SPANS_CLOSE_IND,	"SPANS_CLOSE_IND" },
		{ SPANS_CLOSE_RSP,	"SPANS_CLOSE_RSP" },
		{ SPANS_CLOSE_CNF,	"SPANS_CLOSE_CNF" },
		{ SPANS_RCLOSE_REQ,	"SPANS_RCLOSE_REQ" },
		{ SPANS_RCLOSE_IND,	"SPANS_RCLOSE_IND" },
		{ SPANS_RCLOSE_RSP,	"SPANS_RCLOSE_RSP" },
		{ SPANS_RCLOSE_CNF,	"SPANS_RCLOSE_CNF" },
		{ SPANS_MULTI_REQ,	"SPANS_MULTI_REQ" },
		{ SPANS_MULTI_IND,	"SPANS_MULTI_IND" },
		{ SPANS_MULTI_RSP,	"SPANS_MULTI_RSP" },
		{ SPANS_MULTI_CNF,	"SPANS_MULTI_CNF" },
		{ SPANS_ADD_REQ,	"SPANS_ADD_REQ" },
		{ SPANS_ADD_IND,	"SPANS_ADD_IND" },
		{ SPANS_ADD_RSP,	"SPANS_ADD_RSP" },
		{ SPANS_ADD_CNF,	"SPANS_ADD_CNF" },
		{ SPANS_JOIN_REQ,	"SPANS_JOIN_REQ" },
		{ SPANS_JOIN_CNF,	"SPANS_JOIN_CNF" },
		{ SPANS_LEAVE_REQ,	"SPANS_LEAVE_REQ" },
		{ SPANS_LEAVE_CNF,	"SPANS_LEAVE_CNF" },
		{ SPANS_VCIR_IND,	"SPANS_VCIR_IND" },
		{ SPANS_QUERY_REQ,	"SPANS_QUERY_REQ" },
		{ SPANS_QUERY_RSP,	"SPANS_QUERY_RSP" },
		{ 0,			(char *) 0 }
	};

	/*
	 * Search the name table for the specified type
	 */
	for (i=0; msgtype_names[i].name; i++) {
		if (*objp == msgtype_names[i].type) {
			snprintf(dest, len, "%s (%d)",
					msgtype_names[i].name,
					(int)*objp);
			return;
		}
	}

	/*
	 * Type was not found--return an error indicator
	 */
	snprintf(dest, len, "Invalid (%d)", (int)*objp);
}

#ifdef LONGPRINT 

static void
spans_query_type_str(objp, dest, len)
	spans_query_type *objp;
	char *dest;
	int len;
{
	static char	*query_names[] = {
		"SPANS_QUERY_NORMAL",
		"SPANS_QUERY_DEBUG",
		"SPANS_QUERY_END_TO_END"
	};

	if (*objp < SPANS_QUERY_NORMAL ||
			*objp > SPANS_QUERY_END_TO_END) {
		snprintf(dest, len, "Invalid (%d)", (int)*objp);
	} else {
		snprintf(dest, len, "%s (%d)", query_names[(int)*objp],
				(int)*objp);
	}
}

static void
spans_state_str(objp, dest, len)
	spans_query_type *objp;
	char *dest;
	int len;
{
	static char	*state_names[] = {
		"SPANS_CONN_OPEN",
		"SPANS_CONN_OPEN_PEND",
		"SPANS_CONN_CLOSE_PEND",
		"SPANS_CONN_CLOSED"
	};

	if (*objp < SPANS_CONN_OPEN || *objp > SPANS_CONN_CLOSED) {
		snprintf(dest, len, "Invalid (%d)", (int)*objp);
	} else {
		snprintf(dest, len, "%s (%d)", state_names[(int)*objp],
				(int)*objp);
	}
}


static void
spans_print_version(objp)
	spans_version *objp;
{
	printf("%sspans_version        0x%x\n", spans_indent, *objp);
}

static void
spans_print_vpvc(objp)
	spans_vpvc *objp;
{
	printf("%sVP/VC                %d/%d\n", spans_indent,
			SPANS_EXTRACT_VPI(*objp),
			SPANS_EXTRACT_VCI(*objp));
}

static void
spans_print_vpvc_pref(objp)
	spans_vpvc_pref *objp;
{
	printf("%sspans_vpvc_pref\n", spans_indent);
	inc_indent();
	printf("%s%s\n", spans_indent,
			(objp->vpf_valid ? "Valid" : "Not valid"));
	spans_print_vpvc(&objp->vpf_vpvc);
	dec_indent();
}

static void
spans_print_addr(objp)
	spans_addr *objp;
{
	char	addr_str[80];

	strncpy(addr_str, spans_addr_print(objp), sizeof(addr_str));
	printf("%sspans_addr           %s\n", spans_indent, addr_str);
}

static void
spans_print_sap(objp)
	spans_sap *objp;
{
	printf("%sSAP                  %d\n", spans_indent, *objp);
}

static void
spans_print_atm_conn(objp)
	spans_atm_conn *objp;
{
	printf("%sspans_atm_conn\n", spans_indent);
	inc_indent();
	spans_print_addr(&objp->con_dst);
	spans_print_addr(&objp->con_src);
	spans_print_sap(&objp->con_dsap);
	spans_print_sap(&objp->con_ssap);
	dec_indent();
}

static void
spans_print_resrc(objp)
	spans_resrc *objp;
{
	printf("%sspans_resrc\n", spans_indent);
	inc_indent();
	printf("%srsc_peak             %d\n", spans_indent, objp->rsc_peak);
	printf("%srsc_mean             %d\n", spans_indent, objp->rsc_mean);
	printf("%srsc_burst            %d\n", spans_indent, objp->rsc_burst);
	dec_indent();
}

static void
spans_print_aal(objp)
	spans_aal *objp;
{
	char		aal_str[80];

	spans_aal_str(objp, aal_str, sizeof(aal_str));
	printf("%sspans_aal            %s\n", spans_indent, aal_str);
}

static void
spans_print_result(objp)
	spans_result *objp;
{
	char		result_str[80];

	spans_result_str(objp, result_str, sizeof(result_str));
	printf("%sspans_result         %s\n", spans_indent, result_str);
}

static void
spans_print_msgtype(objp)
	spans_msgtype *objp;
{
	char		msgtype_str[80];

	spans_msgtype_str(objp, msgtype_str, sizeof(msgtype_str));
	printf("%sspans_msgtype        %s\n", spans_indent, msgtype_str);
}

static void
spans_print_parm_stat_req(objp)
	spans_parm_stat_req *objp;
{
	printf("%sspans_parm_stat_req\n", spans_indent);
	inc_indent();
	printf("%sstreq_es_epoch       %d\n", spans_indent,
			objp->streq_es_epoch);
	dec_indent();
}

static void
spans_print_parm_stat_ind(objp)
	spans_parm_stat_ind *objp;
{
	printf("%sspans_parm_stat_ind\n", spans_indent);
	inc_indent();
	printf("%sstind_sw_epoch       %d\n", spans_indent,
			objp->stind_sw_epoch);
	spans_print_addr(&objp->stind_es_addr);
	spans_print_addr(&objp->stind_sw_addr);
	dec_indent();
}

static void
spans_print_parm_stat_rsp(objp)
	spans_parm_stat_rsp *objp;
{
	printf("%sspans_parm_stat_rsp\n", spans_indent);
	inc_indent();
	printf("%sstrsp_es_epoch       %d\n", spans_indent,
			objp->strsp_es_epoch);
	spans_print_addr(&objp->strsp_es_addr);
	dec_indent();
}

static void
spans_print_parm_open_req(objp)
	spans_parm_open_req *objp;
{
	printf("%sspans_parm_open_req\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->opreq_conn);
	spans_print_aal(&objp->opreq_aal);
	spans_print_resrc(&objp->opreq_desrsrc);
	spans_print_resrc(&objp->opreq_minrsrc);
	spans_print_vpvc_pref(&objp->opreq_vpvc);
	dec_indent();
}

static void
spans_print_parm_open_ind(objp)
	spans_parm_open_ind *objp;
{
	printf("%sspans_parm_open_ind\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->opind_conn);
	spans_print_aal(&objp->opind_aal);
	spans_print_resrc(&objp->opind_desrsrc);
	spans_print_resrc(&objp->opind_minrsrc);
	spans_print_vpvc_pref(&objp->opind_vpvc);
	dec_indent();
}

static void
spans_print_parm_open_rsp(objp)
	spans_parm_open_rsp *objp;
{
	printf("%sspans_parm_open_rsp\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->oprsp_conn);
	spans_print_result(&objp->oprsp_result);
	spans_print_resrc(&objp->oprsp_rsrc);
	spans_print_vpvc(&objp->oprsp_vpvc);
	dec_indent();
}

static void
spans_print_parm_open_cnf(objp)
	spans_parm_open_cnf *objp;
{
	printf("%sspans_parm_open_cnf\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->opcnf_conn);
	spans_print_result(&objp->opcnf_result);
	spans_print_resrc(&objp->opcnf_rsrc);
	spans_print_vpvc(&objp->opcnf_vpvc);
	dec_indent();
}

static void
spans_print_parm_close_req(objp)
	spans_parm_close_req *objp;
{
	printf("%sspans_parm_close_req\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->clreq_conn);
	dec_indent();
}

static void
spans_print_parm_close_ind(objp)
	spans_parm_close_ind *objp;
{
	printf("%sspans_parm_close_ind\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->clind_conn);
	dec_indent();
}

static void
spans_print_parm_close_rsp(objp)
	spans_parm_close_rsp *objp;
{
	printf("%sspans_parm_close_rsp\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->clrsp_conn);
	spans_print_result(&objp->clrsp_result);
	dec_indent();
}

static void
spans_print_parm_close_cnf(objp)
	spans_parm_close_cnf *objp;
{
	printf("%sspans_parm_close_cnf\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->clcnf_conn);
	spans_print_result(&objp->clcnf_result);
	dec_indent();
}

static void
spans_print_parm_rclose_req(objp)
	spans_parm_rclose_req *objp;
{
	printf("%sspans_parm_rclose_req\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->rcreq_conn);
	dec_indent();
}

static void
spans_print_parm_rclose_ind(objp)
	spans_parm_rclose_ind *objp;
{
	printf("%sspans_parm_rclose_ind\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->rcind_conn);
	dec_indent();
}

static void
spans_print_parm_rclose_rsp(objp)
	spans_parm_rclose_rsp *objp;
{
	printf("%sspans_parm_rclose_rsp\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->rcrsp_conn);
	spans_print_result(&objp->rcrsp_result);
	dec_indent();
}

static void
spans_print_parm_rclose_cnf(objp)
	spans_parm_rclose_cnf *objp;
{
	printf("%sspans_parm_rclose_cnf\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->rccnf_conn);
	spans_print_result(&objp->rccnf_result);
	dec_indent();
}

static void
spans_print_parm_multi_req(objp)
	spans_parm_multi_req *objp;
{
	printf("%sspans_parm_multi_req\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->mureq_conn);
	spans_print_aal(&objp->mureq_aal);
	spans_print_resrc(&objp->mureq_desrsrc);
	spans_print_resrc(&objp->mureq_minrsrc);
	spans_print_vpvc(&objp->mureq_vpvc);
	dec_indent();
}

static void
spans_print_parm_multi_ind(objp)
	spans_parm_multi_ind *objp;
{
	printf("%sspans_parm_multi_ind\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->muind_conn);
	spans_print_aal(&objp->muind_aal);
	spans_print_resrc(&objp->muind_desrsrc);
	spans_print_resrc(&objp->muind_minrsrc);
	spans_print_vpvc(&objp->muind_vpvc);
	dec_indent();
}

static void
spans_print_parm_multi_rsp(objp)
	spans_parm_multi_rsp *objp;
{
	printf("%sspans_parm_multi_rsp\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->mursp_conn);
	spans_print_result(&objp->mursp_result);
	spans_print_resrc(&objp->mursp_rsrc);
	spans_print_vpvc(&objp->mursp_vpvc);
	dec_indent();
}

static void
spans_print_parm_multi_cnf(objp)
	spans_parm_multi_cnf *objp;
{
	printf("%sspans_parm_multi_cnf\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->mucnf_conn);
	spans_print_result(&objp->mucnf_result);
	spans_print_resrc(&objp->mucnf_rsrc);
	spans_print_vpvc(&objp->mucnf_vpvc);
	dec_indent();
}

static void
spans_print_parm_add_req(objp)
	spans_parm_add_req *objp;
{
	printf("%sspans_parm_add_req\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->adreq_desconn);
	spans_print_atm_conn(&objp->adreq_xstconn);
	dec_indent();
}

static void
spans_print_parm_add_ind(objp)
	spans_parm_add_ind *objp;
{
	printf("%sspans_parm_add_ind\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->adind_desconn);
	spans_print_atm_conn(&objp->adind_xstconn);
	dec_indent();
}

static void
spans_print_parm_add_rsp(objp)
	spans_parm_add_rsp *objp;
{
	printf("%sspans_parm_add_rsp\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->adrsp_conn);
	spans_print_result(&objp->adrsp_result);
	spans_print_resrc(&objp->adrsp_rsrc);
	dec_indent();
}

static void
spans_print_parm_add_cnf(objp)
	spans_parm_add_cnf *objp;
{
	printf("%sspans_parm_add_cnf\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->adcnf_conn);
	spans_print_result(&objp->adcnf_result);
	spans_print_resrc(&objp->adcnf_rsrc);
	dec_indent();
}

static void
spans_print_parm_join_req(objp)
	spans_parm_join_req *objp;
{
	printf("%sspans_parm_join_req\n", spans_indent);
	inc_indent();
	spans_print_addr(&objp->jnreq_addr);
	dec_indent();
}

static void
spans_print_parm_join_cnf(objp)
	spans_parm_join_cnf *objp;
{
	printf("%sspans_print_parm_join_cnf\n", spans_indent);
	inc_indent();
	spans_print_addr(&objp->jncnf_addr);
	spans_print_result(&objp->jncnf_result);
	dec_indent();
}

static void
spans_print_parm_leave_req(objp)
	spans_parm_leave_req *objp;
{
	printf("%sspans_print_parm_leave_req\n", spans_indent);
	inc_indent();
	spans_print_addr(&objp->lvreq_addr);
	dec_indent();
}

static void
spans_print_parm_leave_cnf(objp)
	spans_parm_leave_cnf *objp;
{
	printf("%sspans_parm_leave_cnf\n", spans_indent);
	inc_indent();
	spans_print_addr(&objp->lvcnf_addr);
	spans_print_result(&objp->lvcnf_result);
	dec_indent();
}

static void
spans_print_parm_vcir_ind(objp)
	spans_parm_vcir_ind *objp;
{
	printf("%sspans_parm_vcir_ind\n", spans_indent);
	inc_indent();
	printf("%svrind_min            %d\n", spans_indent, objp->vrind_min);
	printf("%svrind_max            %d\n", spans_indent, objp->vrind_max);
	dec_indent();
}

static void
spans_print_parm_query_req(objp)
	spans_parm_query_req *objp;
{
	char query_type_str[80];

	printf("%sspans_parm_query_req\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->qyreq_conn);
	spans_query_type_str(&objp->qyreq_type,
		query_type_str, sizeof(query_type_str));
	printf("%sqyreq_type           %s\n", spans_indent, query_type_str);
	dec_indent();
}

static void
spans_print_parm_query_rsp(objp)
	spans_parm_query_rsp *objp;
{
	char query_type_str[80], state_type_str[80];

	printf("%sspans_parm_query_rsp\n", spans_indent);
	inc_indent();
	spans_print_atm_conn(&objp->qyrsp_conn);
	spans_query_type_str(&objp->qyrsp_type,
		query_type_str, sizeof(query_type_str));
	printf("%sqyrsp_type           %s\n", spans_indent, query_type_str);
	spans_state_str(&objp->qyrsp_state,
		state_type_str, sizeof(state_type_str));
	printf("%sqyrsp_state          %s\n", spans_indent, state_type_str);
	printf("%sqyrsp_data           0x%x\n", spans_indent,
			objp->qyrsp_data);
	dec_indent();
}

static void
spans_print_msgbody(objp)
	spans_msgbody *objp;
{
	printf("%sspans_msgbody\n", spans_indent);
	inc_indent();
	spans_print_msgtype(&objp->mb_type);
	switch (objp->mb_type) {
	case SPANS_STAT_REQ:
		spans_print_parm_stat_req(&objp->spans_msgbody_u.mb_stat_req);
		break;
	case SPANS_STAT_IND:
		spans_print_parm_stat_ind(&objp->spans_msgbody_u.mb_stat_ind);
		break;
	case SPANS_STAT_RSP:
		spans_print_parm_stat_rsp(&objp->spans_msgbody_u.mb_stat_rsp);
		break;
	case SPANS_OPEN_REQ:
		spans_print_parm_open_req(&objp->spans_msgbody_u.mb_open_req);
		break;
	case SPANS_OPEN_IND:
		spans_print_parm_open_ind(&objp->spans_msgbody_u.mb_open_ind);
		break;
	case SPANS_OPEN_RSP:
		spans_print_parm_open_rsp(&objp->spans_msgbody_u.mb_open_rsp);
		break;
	case SPANS_OPEN_CNF:
		spans_print_parm_open_cnf(&objp->spans_msgbody_u.mb_open_cnf);
		break;
	case SPANS_CLOSE_REQ:
		spans_print_parm_close_req(&objp->spans_msgbody_u.mb_close_req);
		break;
	case SPANS_CLOSE_IND:
		spans_print_parm_close_ind(&objp->spans_msgbody_u.mb_close_ind);
		break;
	case SPANS_CLOSE_RSP:
		spans_print_parm_close_rsp(&objp->spans_msgbody_u.mb_close_rsp);
		break;
	case SPANS_CLOSE_CNF:
		spans_print_parm_close_cnf(&objp->spans_msgbody_u.mb_close_cnf);
		break;
	case SPANS_RCLOSE_REQ:
		spans_print_parm_rclose_req(&objp->spans_msgbody_u.mb_rclose_req);
		break;
	case SPANS_RCLOSE_IND:
		spans_print_parm_rclose_ind(&objp->spans_msgbody_u.mb_rclose_ind);
		break;
	case SPANS_RCLOSE_RSP:
		spans_print_parm_rclose_rsp(&objp->spans_msgbody_u.mb_rclose_rsp);
		break;
	case SPANS_RCLOSE_CNF:
		spans_print_parm_rclose_cnf(&objp->spans_msgbody_u.mb_rclose_cnf);
		break;
	case SPANS_MULTI_REQ:
		spans_print_parm_multi_req(&objp->spans_msgbody_u.mb_multi_req);
		break;
	case SPANS_MULTI_IND:
		spans_print_parm_multi_ind(&objp->spans_msgbody_u.mb_multi_ind);
		break;
	case SPANS_MULTI_RSP:
		spans_print_parm_multi_rsp(&objp->spans_msgbody_u.mb_multi_rsp);
		break;
	case SPANS_MULTI_CNF:
		spans_print_parm_multi_cnf(&objp->spans_msgbody_u.mb_multi_cnf);
		break;
	case SPANS_ADD_REQ:
		spans_print_parm_add_req(&objp->spans_msgbody_u.mb_add_req);
		break;
	case SPANS_ADD_IND:
		spans_print_parm_add_ind(&objp->spans_msgbody_u.mb_add_ind);
		break;
	case SPANS_ADD_RSP:
		spans_print_parm_add_rsp(&objp->spans_msgbody_u.mb_add_rsp);
		break;
	case SPANS_ADD_CNF:
		spans_print_parm_add_cnf(&objp->spans_msgbody_u.mb_add_cnf);
		break;
	case SPANS_JOIN_REQ:
		spans_print_parm_join_req(&objp->spans_msgbody_u.mb_join_req);
		break;
	case SPANS_JOIN_CNF:
		spans_print_parm_join_cnf(&objp->spans_msgbody_u.mb_join_cnf);
		break;
	case SPANS_LEAVE_REQ:
		spans_print_parm_leave_req(&objp->spans_msgbody_u.mb_leave_req);
		break;
	case SPANS_LEAVE_CNF:
		spans_print_parm_leave_cnf(&objp->spans_msgbody_u.mb_leave_cnf);
		break;
	case SPANS_VCIR_IND:
		spans_print_parm_vcir_ind(&objp->spans_msgbody_u.mb_vcir_ind);
		break;
	case SPANS_QUERY_REQ:
		spans_print_parm_query_req(&objp->spans_msgbody_u.mb_query_req);
		break;
	case SPANS_QUERY_RSP:
		spans_print_parm_query_rsp(&objp->spans_msgbody_u.mb_query_rsp);
		break;
	}
	dec_indent();
}

void
spans_print_msg(objp)
	spans_msg *objp;
{
	spans_indent = INIT_INDENT;
	printf("%sspans_msg\n", spans_indent);
	inc_indent();
	spans_print_version(&objp->sm_vers);
	spans_print_msgbody(&objp->sm_body);
	dec_indent();
}

#else	/* ifdef LONGPRINT */

static void
spans_print_msgbody(objp)
	spans_msgbody *objp;
{
	char	daddr[80], msgtype_str[80], result_str[80], saddr[80];
	spans_parm_stat_req	*streq_p;
	spans_parm_stat_ind	*stind_p;
	spans_parm_stat_rsp	*strsp_p;
	spans_parm_open_req	*opreq_p;
	spans_parm_open_ind	*opind_p;
	spans_parm_open_rsp	*oprsp_p;
	spans_parm_open_cnf	*opcnf_p;
	spans_parm_close_req	*clreq_p;
	spans_parm_close_ind	*clind_p;
	spans_parm_close_rsp	*clrsp_p;
	spans_parm_close_cnf	*clcnf_p;
	spans_parm_rclose_req	*rcreq_p;
	spans_parm_rclose_ind	*rcind_p;
	spans_parm_rclose_rsp	*rcrsp_p;
	spans_parm_rclose_cnf	*rccnf_p;

	spans_msgtype_str(&objp->mb_type, msgtype_str, sizeof(msgtype_str));
	printf("%s: ", msgtype_str);
	switch (objp->mb_type) {
	case SPANS_STAT_REQ:
		streq_p = &objp->spans_msgbody_u.mb_stat_req;
		printf("es_epoch=0x%lx", streq_p->streq_es_epoch);
		break;
	case SPANS_STAT_IND:
		stind_p = &objp->spans_msgbody_u.mb_stat_ind;
		strncpy(daddr, spans_addr_print(&stind_p->stind_es_addr),
				sizeof(daddr));
		strncpy(saddr, spans_addr_print(&stind_p->stind_sw_addr),
				sizeof(saddr));
		printf("sw_epoch=0x%lx, es_addr=%s, sw_addr=0x%s",
				stind_p->stind_sw_epoch,
				daddr, saddr);
		break;
	case SPANS_STAT_RSP:
		strsp_p = &objp->spans_msgbody_u.mb_stat_rsp;
		strncpy(daddr, spans_addr_print(&strsp_p->strsp_es_addr),
				sizeof(daddr));
		printf("es_epoch=0x%lx, es_addr=%s",
				strsp_p->strsp_es_epoch, daddr);
		break;
	case SPANS_OPEN_REQ:
		opreq_p = &objp->spans_msgbody_u.mb_open_req;
		strncpy(daddr, spans_addr_print(&opreq_p->opreq_conn.con_dst),
				sizeof(daddr));
		strncpy(saddr, spans_addr_print(&opreq_p->opreq_conn.con_src),
				sizeof(saddr));
		printf("daddr=%s, saddr=%s, dsap=%d, ssap=%d, aal=%d",
				daddr, saddr,
				opreq_p->opreq_conn.con_dsap,
				opreq_p->opreq_conn.con_ssap,
				opreq_p->opreq_aal);
		if (opreq_p->opreq_vpvc.vpf_valid)
			printf(", vp.vc=%d.%d",
					SPANS_EXTRACT_VPI(opreq_p->opreq_vpvc.vpf_vpvc),
					SPANS_EXTRACT_VCI(opreq_p->opreq_vpvc.vpf_vpvc));
		break;
	case SPANS_OPEN_IND:
		opind_p = &objp->spans_msgbody_u.mb_open_ind;
		strncpy(daddr, spans_addr_print(&opind_p->opind_conn.con_dst),
				sizeof(daddr));
		strncpy(saddr, spans_addr_print(&opind_p->opind_conn.con_src),
				sizeof(saddr));
		printf("daddr=%s, saddr=%s, dsap=%d, ssap=%d, aal=%d",
				daddr, saddr,
				opind_p->opind_conn.con_dsap,
				opind_p->opind_conn.con_ssap,
				opind_p->opind_aal);
		if (opind_p->opind_vpvc.vpf_valid)
			printf(", vp.vc=%d.%d",
					SPANS_EXTRACT_VPI(opind_p->opind_vpvc.vpf_vpvc),
					SPANS_EXTRACT_VCI(opind_p->opind_vpvc.vpf_vpvc));
		break;
	case SPANS_OPEN_RSP:
		oprsp_p = &objp->spans_msgbody_u.mb_open_rsp;
		strncpy(daddr, spans_addr_print(&oprsp_p->oprsp_conn.con_dst),
				sizeof(daddr));
		strncpy(saddr, spans_addr_print(&oprsp_p->oprsp_conn.con_src),
				sizeof(saddr));
		spans_result_str(&oprsp_p->oprsp_result, result_str,
			sizeof(result_str));
		printf("result=%s, daddr=%s, saddr=%s, dsap=%d, ssap=%d, vp.vc=%d.%d",
				result_str, daddr, saddr,
				oprsp_p->oprsp_conn.con_dsap,
				oprsp_p->oprsp_conn.con_ssap,
				SPANS_EXTRACT_VPI(oprsp_p->oprsp_vpvc),
				SPANS_EXTRACT_VCI(oprsp_p->oprsp_vpvc));
		break;
	case SPANS_OPEN_CNF:
		opcnf_p = &objp->spans_msgbody_u.mb_open_cnf;
		strncpy(daddr, spans_addr_print(&opcnf_p->opcnf_conn.con_dst),
				sizeof(daddr));
		strncpy(saddr, spans_addr_print(&opcnf_p->opcnf_conn.con_src),
				sizeof(saddr));
		spans_result_str(&opcnf_p->opcnf_result, result_str,
			sizeof(result_str));
		printf("result=%s, daddr=%s, saddr=%s, dsap=%d, ssap=%d, vp.vc=%d.%d",
				result_str, daddr, saddr,
				opcnf_p->opcnf_conn.con_dsap,
				opcnf_p->opcnf_conn.con_ssap,
				SPANS_EXTRACT_VPI(opcnf_p->opcnf_vpvc),
				SPANS_EXTRACT_VCI(opcnf_p->opcnf_vpvc));
		break;
	case SPANS_CLOSE_REQ:
		clreq_p = &objp->spans_msgbody_u.mb_close_req;
		strncpy(daddr, spans_addr_print(&clreq_p->clreq_conn.con_dst),
				sizeof(daddr));
		strncpy(saddr, spans_addr_print(&clreq_p->clreq_conn.con_src),
				sizeof(saddr));
		printf("daddr=%s, saddr=%s, dsap=%d, ssap=%d",
				daddr, saddr,
				clreq_p->clreq_conn.con_dsap,
				clreq_p->clreq_conn.con_ssap);
		break;
	case SPANS_CLOSE_IND:
		clind_p = &objp->spans_msgbody_u.mb_close_ind;
		strncpy(daddr, spans_addr_print(&clind_p->clind_conn.con_dst),
				sizeof(daddr));
		strncpy(saddr, spans_addr_print(&clind_p->clind_conn.con_src),
				sizeof(saddr));
		printf("daddr=%s, saddr=%s, dsap=%d, ssap=%d",
				daddr, saddr,
				clind_p->clind_conn.con_dsap,
				clind_p->clind_conn.con_ssap);
		break;
	case SPANS_CLOSE_RSP:
		clrsp_p = &objp->spans_msgbody_u.mb_close_rsp;
		strncpy(daddr, spans_addr_print(&clrsp_p->clrsp_conn.con_dst),
				sizeof(daddr));
		strncpy(saddr, spans_addr_print(&clrsp_p->clrsp_conn.con_src),
				sizeof(saddr));
		spans_result_str(&clrsp_p->clrsp_result, result_str,
			sizeof(result_str));
		printf("result=%s, daddr=%s, saddr=%s, dsap=%d, ssap=%d",
				result_str, daddr, saddr,
				clrsp_p->clrsp_conn.con_dsap,
				clrsp_p->clrsp_conn.con_ssap);
		break;
	case SPANS_CLOSE_CNF:
		clcnf_p = &objp->spans_msgbody_u.mb_close_cnf;
		strncpy(daddr, spans_addr_print(&clcnf_p->clcnf_conn.con_dst),
				sizeof(daddr));
		strncpy(saddr, spans_addr_print(&clcnf_p->clcnf_conn.con_src),
				sizeof(saddr));
		spans_result_str(&clcnf_p->clcnf_result, result_str,
			sizeof(result_str));
		printf("result=%s, daddr=%s, saddr=%s, dsap=%d, ssap=%d",
				result_str, daddr, saddr,
				clcnf_p->clcnf_conn.con_dsap,
				clcnf_p->clcnf_conn.con_ssap);
		break;
	case SPANS_RCLOSE_REQ:
		rcreq_p = &objp->spans_msgbody_u.mb_rclose_req;
		strncpy(daddr, spans_addr_print(&rcreq_p->rcreq_conn.con_dst),
				sizeof(daddr));
		strncpy(saddr, spans_addr_print(&rcreq_p->rcreq_conn.con_src),
				sizeof(saddr));
		printf("daddr=%s, saddr=%s, dsap=%d, ssap=%d",
				daddr, saddr,
				rcreq_p->rcreq_conn.con_dsap,
				rcreq_p->rcreq_conn.con_ssap);
		break;
	case SPANS_RCLOSE_IND:
		rcind_p = &objp->spans_msgbody_u.mb_rclose_ind;
		strncpy(daddr, spans_addr_print(&rcind_p->rcind_conn.con_dst),
				sizeof(daddr));
		strncpy(saddr, spans_addr_print(&rcind_p->rcind_conn.con_src),
				sizeof(saddr));
		printf("daddr=%s, saddr=%s, dsap=%d, ssap=%d",
				daddr, saddr,
				rcind_p->rcind_conn.con_dsap,
				rcind_p->rcind_conn.con_ssap);
		break;
	case SPANS_RCLOSE_RSP:
		rcrsp_p = &objp->spans_msgbody_u.mb_rclose_rsp;
		strncpy(daddr, spans_addr_print(&rcrsp_p->rcrsp_conn.con_dst),
				sizeof(daddr));
		strncpy(saddr, spans_addr_print(&rcrsp_p->rcrsp_conn.con_src),
				sizeof(saddr));
		spans_result_str(&rcrsp_p->rcrsp_result, result_str,
			sizeof(result_str));
		printf("result=%s, daddr=%s, saddr=%s, dsap=%d, ssap=%d",
				result_str, daddr, saddr,
				rcrsp_p->rcrsp_conn.con_dsap,
				rcrsp_p->rcrsp_conn.con_ssap);
		break;
	case SPANS_RCLOSE_CNF:
		rccnf_p = &objp->spans_msgbody_u.mb_rclose_cnf;
		strncpy(daddr, spans_addr_print(&rccnf_p->rccnf_conn.con_dst),
				sizeof(daddr));
		strncpy(saddr, spans_addr_print(&rccnf_p->rccnf_conn.con_src),
				sizeof(saddr));
		spans_result_str(&rccnf_p->rccnf_result, result_str,
			sizeof(result_str));
		printf("result=%s, daddr=%s, saddr=%s, dsap=%d, ssap=%d",
				result_str, daddr, saddr,
				rccnf_p->rccnf_conn.con_dsap,
				rccnf_p->rccnf_conn.con_ssap);
		break;
	default:
		break;
	}
	printf("\n");
}

void
spans_print_msg(objp)
	spans_msg *objp;
{
#ifdef LONGPRINT
	spans_indent = INIT_INDENT;
#endif
	spans_print_msgbody(&objp->sm_body);
}

#endif	/* ifdef LONGPRINT */
