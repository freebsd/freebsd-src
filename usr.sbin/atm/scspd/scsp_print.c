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
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * Print routines
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h> 
#include <netatm/queue.h> 
#include <netatm/atm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>
  
#include <errno.h>
#include <libatm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "scsp_msg.h"
#include "scsp_if.h"
#include "scsp_var.h"

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Indent string
 */
#define	MIN_INDENT	2
#define	MAX_INDENT	64
static char	indent[MAX_INDENT + 1];


/*
 * Value-name translation table entry
 */
struct type_name {
	char	*name;
	u_char	type;
};
typedef	struct type_name	Type_name;


/*
 * SCSP name-type tables
 */
static Type_name if_msg_types[] = {
	{ "Config Request",		SCSP_CFG_REQ },
	{ "Config Response",		SCSP_CFG_RSP },
	{ "Cache Indication",		SCSP_CACHE_IND },
	{ "Cache Response",		SCSP_CACHE_RSP },
	{ "Solicit Indication",		SCSP_SOLICIT_IND },
	{ "Solicit Response",		SCSP_SOLICIT_RSP },
	{ "Cache Update Indication",	SCSP_UPDATE_IND },
	{ "Cache Update Request",	SCSP_UPDATE_REQ },
	{ "Cache Update Response",	SCSP_UPDATE_RSP },
	{ (char *)0,	0 }
};

static Type_name msg_types[] = {
	{ "Cache Alignment",	SCSP_CA_MSG },
	{ "CSU Request",	SCSP_CSU_REQ_MSG },
	{ "CSU Reply",		SCSP_CSU_REPLY_MSG },
	{ "CSU Solicit",	SCSP_CSUS_MSG },
	{ "Hello",		SCSP_HELLO_MSG },
	{ (char *)0,		0 }
};

static Type_name proto_types[] = {
	{ "ATMARP",	SCSP_PROTO_ATMARP },
	{ "NHRP",	SCSP_PROTO_NHRP },
	{ "MARS",	SCSP_PROTO_MARS },
	{ "DHCP",	SCSP_PROTO_DHCP },
	{ "LNNI",	SCSP_PROTO_LNNI },
	{ (char *)0,	0 }
};

static Type_name ext_types[] = {
	{ "End of Extensions",	SCSP_EXT_END },
	{ "Authentication",	SCSP_EXT_AUTH },
	{ "Vendor Private",	SCSP_EXT_VENDOR },
	{ (char *)0,		0 }
};

static Type_name hfsm_state_names[] = {
	{ "Down",		SCSP_HFSM_DOWN },
	{ "Waiting",		SCSP_HFSM_WAITING },
	{ "Unidirectional",	SCSP_HFSM_UNI_DIR },
	{ "Bidirectional",	SCSP_HFSM_BI_DIR },
	{ (char *)0,		0 }
};

static Type_name hfsm_event_names[] = {
	{ "VC open",		SCSP_HFSM_VC_ESTAB },
	{ "VC closed",		SCSP_HFSM_VC_CLOSED },
	{ "Hello timer",	SCSP_HFSM_HELLO_T },
	{ "Receive timer",	SCSP_HFSM_RCV_T },
	{ "Msg received",	SCSP_HFSM_RCVD },
	{ (char *)0,		0 }
};

static Type_name cafsm_state_names[] = {
	{ "Down",			SCSP_CAFSM_DOWN },
	{ "Master/Slave negotiation",	SCSP_CAFSM_NEG },
	{ "Master",			SCSP_CAFSM_MASTER },
	{ "Slave",			SCSP_CAFSM_SLAVE },
	{ "Update cache",		SCSP_CAFSM_UPDATE },
	{ "Aligned",			SCSP_CAFSM_ALIGNED },
	{ (char *)0,			0 }
};

static Type_name cafsm_event_names[] = {
	{ "Hello FSM up",		SCSP_CAFSM_HELLO_UP },
	{ "Hello FSM down",		SCSP_CAFSM_HELLO_DOWN },
	{ "CA received",		SCSP_CAFSM_CA_MSG },
	{ "CSU Solicit received",	SCSP_CAFSM_CSUS_MSG },
	{ "CSU Request received",	SCSP_CAFSM_CSU_REQ },
	{ "CSU Reply received",		SCSP_CAFSM_CSU_REPLY },
	{ "CA timer",			SCSP_CAFSM_CA_T },
	{ "CSUS timer",			SCSP_CAFSM_CSUS_T },
	{ "CSU timer",			SCSP_CAFSM_CSU_T },
	{ "Cache Update",		SCSP_CAFSM_CACHE_UPD },
	{ "Cache Response",		SCSP_CAFSM_CACHE_RSP },
	{ (char *)0,			0 }
};

static Type_name cifsm_state_names[] = {
	{ "Null",	SCSP_CIFSM_NULL },
	{ "Summarize",	SCSP_CIFSM_SUM },
	{ "Update",	SCSP_CIFSM_UPD },
	{ "Aligned",	SCSP_CIFSM_ALIGN },
	{ (char *)0,			0 }
};

static Type_name cifsm_event_names[] = {
	{ "CA FSM down",	SCSP_CIFSM_CA_DOWN },
	{ "CA FSM to Summarize",SCSP_CIFSM_CA_SUMM },
	{ "CA FSM to Update",	SCSP_CIFSM_CA_UPD },
	{ "CA FSM to Aligned",	SCSP_CIFSM_CA_ALIGN },
	{ "Solicit Rsp",	SCSP_CIFSM_SOL_RSP },
	{ "Update Req",		SCSP_CIFSM_UPD_REQ },
	{ "Update Rsp",		SCSP_CIFSM_UPD_RSP },
	{ "CSU Request",	SCSP_CIFSM_CSU_REQ },
	{ "CSU Reply",		SCSP_CIFSM_CSU_REPLY },
	{ "CSU Solicit",	SCSP_CIFSM_CSU_SOL },
	{ (char *)0,			0 }
};

static Type_name atmarp_state_names[] = {
	{ "New",	SCSP_ASTATE_NEW },
	{ "Updated",	SCSP_ASTATE_UPD },
	{ "Deleted",	SCSP_ASTATE_DEL },
	{ (char *)0,	0 }
};


/*
 * Initialize the indent string
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	none
 *
 */
static void
init_indent()
{
	indent[0] = '\0';
}


/*
 * Increment the indent string
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	none
 *
 */
static void
inc_indent()
{
	if (strlen(indent) >= MAX_INDENT)
		return;
	strcat(indent, "  ");
}


/*
 * Decrement the indent string
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	none
 *
 */
static void
dec_indent()
{
	if (strlen(indent) < MIN_INDENT)
		return;
	indent[strlen(indent) - 2] = '\0';
}



/*
 * Search for a type in a name-type table
 *
 * Arguments:
 *	type	the value being searched for
 *	tbl	pointer to the table to search
 *
 * Returns:
 *	pointer to a string identifying the type
 *
 */
static char *
scsp_type_name(type, tbl)
	u_char	type;
	Type_name	*tbl;
{
	int	i;

	/*
	 * Search the table
	 */
	for (i = 0; tbl[i].name != (char *)0 && tbl[i].type != type;
			i++)
		;

	/*
	 * Check the result and return the appropriate value
	 */
	if (tbl[i].name)
		return(tbl[i].name);
	else
		return("-");
}


/*
 * Format a Hello FSM state name
 *
 * Arguments:
 *	state	the state
 *
 * Returns:
 *	pointer to a string identifying the state
 *
 */
char *
format_hfsm_state(state)
	int	state;
{
	return(scsp_type_name((u_char)state, hfsm_state_names));
}


/*
 * Format a Hello FSM event name
 *
 * Arguments:
 *	event	the event
 *
 * Returns:
 *	pointer to a string identifying the event
 *
 */
char *
format_hfsm_event(event)
	int	event;
{
	char	*cp;

	cp = scsp_type_name((u_char)event, hfsm_event_names);
	return(cp);
}


/*
 * Format a CA FSM state name
 *
 * Arguments:
 *	state	the state
 *
 * Returns:
 *	pointer to a string identifying the state
 *
 */
char *
format_cafsm_state(state)
	int	state;
{
	return(scsp_type_name((u_char)state, cafsm_state_names));
}


/*
 * Format a CA FSM event name
 *
 * Arguments:
 *	event	the event
 *
 * Returns:
 *	pointer to a string identifying the event
 *
 */
char *
format_cafsm_event(event)
	int	event;
{
	return(scsp_type_name((u_char)event, cafsm_event_names));
}


/*
 * Format a client interface FSM state name
 *
 * Arguments:
 *	state	the state
 *
 * Returns:
 *	pointer to a string identifying the state
 *
 */
char *
format_cifsm_state(state)
	int	state;
{
	return(scsp_type_name((u_char)state, cifsm_state_names));
}


/*
 * Format a client interface FSM event name
 *
 * Arguments:
 *	event	the event
 *
 * Returns:
 *	pointer to a string identifying the event
 *
 */
char *
format_cifsm_event(event)
	int	event;
{
	return(scsp_type_name((u_char)event, cifsm_event_names));
}


/*
 * Print a Sender or Receiver ID structure
 *
 * Arguments:
 *	fp	file to print message to
 *	idp	pointer to ID to be printed
 *
 * Returns:
 *	none
 *
 */
void
print_scsp_id(fp, idp)
	FILE	*fp;
	Scsp_id	*idp;
{
	int	i;

	inc_indent();
	fprintf(fp, "%sNext:                 %p\n", indent, idp->next);
	fprintf(fp, "%sLength:               %d\n", indent,
			idp->id_len);
	fprintf(fp, "%sID:                   0x", indent);
	for (i = 0; i < idp->id_len; i++)
		fprintf(fp, "%02x ", idp->id[i]);
	fprintf(fp, "\n");
	dec_indent();
}


/*
 * Print a Cache Key structure
 *
 * Arguments:
 *	fp	file to print message to
 *	ckp	pointer to cache key structure
 *
 * Returns:
 *	none
 *
 */
void
print_scsp_cache_key(fp, ckp)
	FILE		*fp;
	Scsp_ckey	*ckp;
{
	int	i;

	inc_indent();
	fprintf(fp, "%sLength:               %d\n", indent,
			ckp->key_len);
	fprintf(fp, "%sKey:                  0x", indent);
	for (i = 0; i < ckp->key_len; i++)
		fprintf(fp, "%02x ", ckp->key[i]);
	fprintf(fp, "\n");
	dec_indent();
}


/*
 * Print the mandatory common part of a message
 *
 * Arguments:
 *	fp	file to print message to
 *	mcp	pointer to mandatory common part structure
 *
 * Returns:
 *	none
 *
 */
static void
print_scsp_mcp(fp, mcp)
	FILE		*fp;
	Scsp_mcp	*mcp;
{
	inc_indent();
	fprintf(fp, "%sProtocol ID:          %s (0x%02x)\n", indent,
			scsp_type_name(mcp->pid, proto_types),
			mcp->pid);
	fprintf(fp, "%sServer Group ID:      %d\n", indent, mcp->sgid);
	fprintf(fp, "%sFlags:                0x%04x\n", indent,
			mcp->flags);
	fprintf(fp, "%sRecord Count:         %d\n", indent,
			mcp->rec_cnt);
	fprintf(fp, "%sSender ID:\n", indent);
	print_scsp_id(fp, &mcp->sid);
	fprintf(fp, "%sReceiver ID:\n", indent);
	print_scsp_id(fp, &mcp->rid);
	dec_indent();
}


/*
 * Print an extension
 *
 * Arguments:
 *	fp	file to print message to
 *	exp	pointer to extension
 *
 * Returns:
 *	none
 *
 */
static void
print_scsp_ext(fp, exp)
	FILE		*fp;
	Scsp_ext	*exp;
{
	int	i;
	u_char	*cp;

	inc_indent();
	fprintf(fp, "%sNext:                 %p\n", indent, exp->next);
	fprintf(fp, "%sType:                 %s (0x%02x)\n", indent,
			scsp_type_name(exp->type, ext_types),
			exp->type);
	fprintf(fp, "%sLength:               %d\n", indent, exp->len);
	if (exp->len) {
		fprintf(fp, "%sValue:                0x", indent);
		cp = (u_char *)((caddr_t)exp + sizeof(Scsp_ext));
		for (i = 0; i < exp->len; i++)
			fprintf(fp, "%02x ", *cp++);
		fprintf(fp, "\n");
	}
	dec_indent();
}


/*
 * Print an ATMARP Cache State Advertisement record
 *
 * Arguments:
 *	fp	file to print message to
 *	acsp	pointer to extension
 *
 * Returns:
 *	none
 *
 */
static void
print_scsp_atmarp_csa(fp, acsp)
	FILE		*fp;
	Scsp_atmarp_csa	*acsp;
{
	inc_indent();
	fprintf(fp, "%sState:                 %s (%d)\n", indent,
			scsp_type_name(acsp->sa_state,
				atmarp_state_names),
			acsp->sa_state);
	fprintf(fp, "%sSource ATM addr:       %s\n", indent,
			format_atm_addr(&acsp->sa_sha));
	fprintf(fp, "%sSource ATM subaddr:    %s\n", indent,
			format_atm_addr(&acsp->sa_ssa));
	fprintf(fp, "%sSource IP addr:        %s\n", indent,
			format_ip_addr(&acsp->sa_spa));
	fprintf(fp, "%sTarget ATM addr:       %s\n", indent,
			format_atm_addr(&acsp->sa_tha));
	fprintf(fp, "%sTarget ATM subaddr:    %s\n", indent,
			format_atm_addr(&acsp->sa_tsa));
	fprintf(fp, "%sTarget IP addr:        %s\n", indent,
			format_ip_addr(&acsp->sa_tpa));
	dec_indent();
}


/*
 * Print a Cache State Advertisement record or
 * Cache State Advertisement Summary record
 *
 * Arguments:
 *	fp	file to print message to
 *	csap	pointer to CSA or CSAS
 *
 * Returns:
 *	none
 *
 */
static void
print_scsp_csa(fp, csap)
	FILE		*fp;
	Scsp_csa	*csap;
{
	inc_indent();
	fprintf(fp, "%sNext:                 %p\n", indent, csap->next);
	fprintf(fp, "%sHops:                 %d\n", indent, csap->hops);
	fprintf(fp, "%sNull Flag:            %s\n", indent,
			csap->null ? "True" : "False");
	fprintf(fp, "%sSequence no.:         %ld (0x%lx)\n",
			indent, csap->seq, csap->seq);
	fprintf(fp, "%sCache Key:\n", indent);
	print_scsp_cache_key(fp, &csap->key);
	fprintf(fp, "%sOriginator ID:\n", indent);
	print_scsp_id(fp, &csap->oid);
	if (csap->atmarp_data) {
		fprintf(fp, "%sATMARP data:\n", indent);
		print_scsp_atmarp_csa(fp, csap->atmarp_data);
	}
	dec_indent();
}


/*
 * Print a Cache Alignment message
 *
 * Arguments:
 *	fp	file to print message to
 *	cap	pointer to extension
 *
 * Returns:
 *	none
 *
 */
static void
print_scsp_ca(fp, cap)
	FILE	*fp;
	Scsp_ca	*cap;
{
	int		n;
	Scsp_csa	*csap;

	inc_indent();
	fprintf(fp, "%sCA Seq. No.:          %ld\n", indent,
			cap->ca_seq);
	fprintf(fp, "%sM bit:                %s\n", indent,
			cap->ca_m ? "True" : "False");
	fprintf(fp, "%sI bit:                %s\n", indent,
			cap->ca_i ? "True" : "False");
	fprintf(fp, "%sO bit:                %s\n", indent,
			cap->ca_o ? "True" : "False");
	fprintf(fp, "%sMandatory Common Part:\n", indent);
	print_scsp_mcp(fp, &cap->ca_mcp);
	for (csap = cap->ca_csa_rec, n = 1; csap;
			csap = csap->next, n++) {
		fprintf(fp, "%sCSA Record %d (%p):\n", indent, n, csap);
		print_scsp_csa(fp, csap);
	}
	dec_indent();
}


/*
 * Print a Cache State Update Request, Cache State Update Reply, or
 * Cache State Update Solicit message
 *
 * Arguments:
 *	fp	file to print message to
 *	csup	pointer to CSU message
 *
 * Returns:
 *	none
 *
 */
static void
print_scsp_csu(fp, csup)
	FILE		*fp;
	Scsp_csu_msg	*csup;
{
	int		i;
	Scsp_csa	*csap;

	inc_indent();
	fprintf(fp, "%sMandatory Common Part:\n", indent);
	print_scsp_mcp(fp, &csup->csu_mcp);
	for (csap = csup->csu_csa_rec, i = 1; csap;
			csap = csap->next, i++) {
		fprintf(fp, "%sCSA Record %d:\n", indent, i);
		print_scsp_csa(fp, csap);
	}
	dec_indent();
}


/*
 * Print a Hello message
 *
 * Arguments:
 *	fp	file to print message to
 *	hp	pointer to hello message
 *
 * Returns:
 *	none
 *
 */
static void
print_scsp_hello(fp, hp)
	FILE		*fp;
	Scsp_hello	*hp;
{
	Scsp_id	*ridp;

	inc_indent();
	fprintf(fp, "%sHello Interval:       %d\n", indent,
			hp->hello_int);
	fprintf(fp, "%sDead Factor:          %d\n", indent,
			hp->dead_factor);
	fprintf(fp, "%sFamily ID:            %d\n", indent,
			hp->family_id);
	fprintf(fp, "%sMandatory Common Part:\n", indent);
	print_scsp_mcp(fp, &hp->hello_mcp);
	ridp = hp->hello_mcp.rid.next;
	if (ridp) {
		fprintf(fp, "%sAdditional Receiver IDs:\n", indent);
		for (; ridp; ridp = ridp->next)
			print_scsp_id(fp, ridp);
	}
	dec_indent();
}


#ifdef NOTDEF
/*
 * NHRP-specific Cache State Advertisement record
 */
struct scsp_nhrp_csa {
	u_char	req_id;			/* Request ID */
	u_char	state;			/* State */
	u_char	pref_len;		/* Prefix length */
	u_short	flags;			/* See below */
	u_short	mtu;			/* Maximim transmission unit */
	u_short	hold_time;		/* Entry holding time */
	u_char	caddr_tlen;		/* Client addr type/length */
	u_char	csaddr_tlen;		/* Client subaddr type/length */
	u_char	cproto_len;		/* Client proto addr length */
	u_char	pref;			/* Preference */
	Atm_addr	caddr;		/* Client address */
	Atm_addr	csaddr;		/* Client subaddress */
	struct in_addr	cproto_addr;	/* Client protocol address */
};
typedef	struct scsp_nhrp	Scsp_nhrp;

#define	SCSP_NHRP_UNIQ	0x8000
#define	SCSP_NHRP_ARP	0x4000

#endif


/*
 * Print an SCSP message
 *
 * Arguments:
 *	fp	file to print message to
 *	msg	pointer to message to be printed
 *
 * Returns:
 *	none
 *
 */
void
print_scsp_msg(fp, msg)
	FILE		*fp;
	Scsp_msg	*msg;
{
	int		n;
	Scsp_ext	*exp;

	/*
	 * Initialize
	 */
	init_indent();

	/*
	 * Print the message type
	 */
	inc_indent();
	fprintf(fp, "%sMessage type:         %s (0x%02x)\n", indent,
			scsp_type_name(msg->sc_msg_type, msg_types),
			msg->sc_msg_type);

	/*
	 * Print the body of the message
	 */
	switch(msg->sc_msg_type) {
	case SCSP_CA_MSG:
		print_scsp_ca(fp, msg->sc_ca);
		break;
	case SCSP_CSU_REQ_MSG:
	case SCSP_CSU_REPLY_MSG:
	case SCSP_CSUS_MSG:
		print_scsp_csu(fp, msg->sc_csu_msg);
		break;
	case SCSP_HELLO_MSG:
		print_scsp_hello(fp, msg->sc_hello);
		break;
	}

	/*
	 * Print any extensions
	 */
	for (exp = msg->sc_ext, n = 1; exp; exp = exp->next, n++) {
		fprintf(fp, "%sExtension %d:\n", indent, n);
		print_scsp_ext(fp, exp);
	}
	dec_indent();

	(void)fflush(fp);
}


/*
 * Print an SCSP ATMARP message
 *
 * Arguments:
 *	fp	file to print message to
 *	acp	pointer to ATMARP message
 *
 * Returns:
 *	none
 *
 */
static void
print_scsp_if_atmarp(fp, amp)
	FILE		*fp;
	Scsp_atmarp_msg	*amp;
{
	inc_indent();
	fprintf(fp, "%sState:                %s (%d)\n", indent,
			scsp_type_name(amp->sa_state,
				atmarp_state_names),
			amp->sa_state);
	fprintf(fp, "%sCached protocol addr: %s\n", indent,
			format_ip_addr(&amp->sa_cpa));
	fprintf(fp, "%sCached ATM addr:      %s\n", indent,
			format_atm_addr(&amp->sa_cha));
	fprintf(fp, "%sCached ATM subaddr:      %s\n", indent,
			format_atm_addr(&amp->sa_csa));
	fprintf(fp, "%sCache key:\n", indent);
	print_scsp_cache_key(fp, &amp->sa_key);
	fprintf(fp, "%sOriginator ID:\n", indent);
	print_scsp_id(fp, &amp->sa_oid);
	fprintf(fp, "%sSequence number:         %ld (0x%08lx)\n", indent,
			amp->sa_seq, (u_long)amp->sa_seq);
	dec_indent();
}


/*
 * Print an SCSP client interface message
 *
 * Arguments:
 *	fp	file to print message to
 *	imsg	pointer to message to be printed
 *
 * Returns:
 *	none
 *
 */
void
print_scsp_if_msg(fp, imsg)
	FILE		*fp;
	Scsp_if_msg	*imsg;
{
	int		len;
	Scsp_atmarp_msg	*ap;

	/*
	 * Initialize
	 */
	init_indent();
	fprintf(fp, "SCSP Client Interface Message at %p\n", imsg);

	/*
	 * Print the message header
	 */
	inc_indent();
	fprintf(fp, "%sMessage type:         %s (0x%02x)\n", indent,
			scsp_type_name(imsg->si_type, if_msg_types),
			imsg->si_type);
	fprintf(fp, "%sResponse code:        %d\n", indent,
			imsg->si_rc);
	fprintf(fp, "%sProtocol type:        %s (%d)\n", indent,
			scsp_type_name(imsg->si_proto, proto_types),
			imsg->si_proto);
	fprintf(fp, "%sLength:               %d\n", indent,
			imsg->si_len);
	fprintf(fp, "%sToken:                0x%lx\n", indent,
			imsg->si_tok);

	/*
	 * Print the body of the message
	 */
	switch(imsg->si_type) {
	case SCSP_CFG_REQ:
		fprintf(fp, "%sInterface:            %s\n", indent,
				imsg->si_cfg.atmarp_netif);
		break;
	case SCSP_CACHE_RSP:
	case SCSP_UPDATE_IND:
	case SCSP_UPDATE_REQ:
		len = imsg->si_len - sizeof(Scsp_if_msg_hdr);
		ap = &imsg->si_atmarp;
		while (len) {
			switch(imsg->si_proto) {
			case SCSP_PROTO_ATMARP:
				fprintf(fp, "%sATMARP CSA:\n", indent);
				print_scsp_if_atmarp(fp, ap);
				len -= sizeof(Scsp_atmarp_msg);
				ap++;
				break;
			case SCSP_PROTO_NHRP:
			case SCSP_PROTO_MARS:
			case SCSP_PROTO_DHCP:
			case SCSP_PROTO_LNNI:
			default:
				fprintf(fp, "Protocol type not implemented\n");
				break;
			}
		}
		break;
	}
	dec_indent();

	(void)fflush(fp);
}


/*
 * Print an SCSP pending connection block
 *
 * Arguments:
 *	fp	file to print message to
 *	pp	pointer to pending control block
 *
 * Returns:
 *	none
 *
 */
void
print_scsp_pending(fp, pp)
	FILE		*fp;
	Scsp_pending	*pp;
{
	/*
	 * Initialize
	 */
	init_indent();

	/*
	 * Print a header
	 */
	fprintf(fp, "Pending control block at %p\n", pp);

	/*
	 * Print the fields of the control block
	 */
	inc_indent();
	fprintf(fp, "%sNext:                 %p\n", indent, pp->sp_next);
	fprintf(fp, "%sSocket:               %d\n", indent,
			pp->sp_sock);

	dec_indent();
}


/*
 * Print an SCSP server control block
 *
 * Arguments:
 *	fp	file to print message to
 *	ssp	pointer to server control block
 *
 * Returns:
 *	none
 *
 */
void
print_scsp_server(fp, ssp)
	FILE		*fp;
	Scsp_server	*ssp;
{
	/*
	 * Initialize
	 */
	init_indent();

	/*
	 * Print a header
	 */
	fprintf(fp, "Server control block at %p\n", ssp);

	/*
	 * Print the fields of the client control block
	 */
	inc_indent();
	fprintf(fp, "%sNext:                 %p\n", indent,
			ssp->ss_next);
	fprintf(fp, "%sName:                 %s\n", indent,
			ssp->ss_name);
	fprintf(fp, "%sNetwork Interface:    %s\n", indent,
			ssp->ss_intf);
	fprintf(fp, "%sState:                %d\n", indent,
			ssp->ss_state);
	fprintf(fp, "%sProtocol ID:          0x%lx\n", indent,
			ssp->ss_pid);
	fprintf(fp, "%sID length:            %d\n", indent,
			ssp->ss_id_len);
	fprintf(fp, "%sCache key length:     %d\n", indent,
			ssp->ss_ckey_len);
	fprintf(fp, "%sServer Group ID:      0x%lx\n", indent,
			ssp->ss_sgid);
	fprintf(fp, "%sFamily ID:            0x%lx\n", indent,
			ssp->ss_fid);
	fprintf(fp, "%sSocket:               %d\n", indent,
			ssp->ss_sock);
	fprintf(fp, "%sDCS Listen Socket:    %d\n", indent,
			ssp->ss_dcs_lsock);
	fprintf(fp, "%sLocal Server ID:\n", indent);
	print_scsp_id(fp, &ssp->ss_lsid);
	fprintf(fp, "%sATM address:          %s\n", indent,
			format_atm_addr(&ssp->ss_addr));
	fprintf(fp, "%sATM subaddress:       %s\n", indent,
			format_atm_addr(&ssp->ss_subaddr));
	fprintf(fp, "%sInterface MTU:        %d\n", indent,
			ssp->ss_mtu);
	fprintf(fp, "%sMark:                 %d\n", indent,
			ssp->ss_mark);
	dec_indent();
}


/*
 * Print an SCSP client cache summary entry control block
 *
 * Arguments:
 *	fp	file to print message to
 *	csep	pointer to summary entry
 *
 * Returns:
 *	none
 *
 */
void
print_scsp_cse(fp, csep)
	FILE		*fp;
	Scsp_cse	*csep;
{
	/*
	 * Print the fields of the cache summary entry
	 */
	inc_indent();
	fprintf(fp, "%sNext CSE:             %p\n", indent, csep->sc_next);
	fprintf(fp, "%sCSA sequence no.:     %ld (0x%lx)\n", indent,
			csep->sc_seq, csep->sc_seq);
	fprintf(fp, "%sCache key:\n", indent);
	print_scsp_cache_key(fp, &csep->sc_key);
	fprintf(fp, "%sOrigin ID:\n", indent);
	print_scsp_id(fp, &csep->sc_oid);
	dec_indent();
}


/*
 * Print an SCSP CSU Request retransmission control block
 *
 * Arguments:
 *	fp	file to print message to
 *	csurp	pointer to retransmission entry
 *
 * Returns:
 *	none
 *
 */
void
print_scsp_csu_rexmt(fp, rxp)
	FILE		*fp;
	Scsp_csu_rexmt	*rxp;
{
	int		i;
	Scsp_csa	*csap;

	inc_indent();
	fprintf(fp, "%sNext CSU Req rexmt:   %p\n", indent, rxp->sr_next);
	fprintf(fp, "%sDCS address:          %p\n", indent, rxp->sr_dcs);
	for (csap = rxp->sr_csa, i = 1; csap;
			csap = csap->next, i++) {
		fprintf(fp, "%sCSA %d:\n", indent, i);
		print_scsp_csa(fp, csap);
	}
	dec_indent();
}


/*
 * Print an SCSP DCS control block
 *
 * Arguments:
 *	fp	file to print message to
 *	dcsp	pointer to DCS control block
 *
 * Returns:
 *	none
 *
 */
void
print_scsp_dcs(fp, dcsp)
	FILE		*fp;
	Scsp_dcs	*dcsp;
{
	Scsp_csa	*csap;
	Scsp_cse	*csep;
	Scsp_csu_rexmt	*rxp;

	/*
	 * Initialize
	 */
	init_indent();

	/*
	 * Print a header
	 */
	fprintf(fp, "DCS control block at %p\n", dcsp);

	/*
	 * Print the fields of the DCS control block
	 */
	inc_indent();
	fprintf(fp, "%sNext DCS block:       %p\n", indent, dcsp->sd_next);
	fprintf(fp, "%sServer control block: %p\n", indent, dcsp->sd_server);
	fprintf(fp, "%sDCS ID:\n", indent);
	print_scsp_id(fp, &dcsp->sd_dcsid);
	fprintf(fp, "%sDCS address:          %s\n", indent,
			format_atm_addr(&dcsp->sd_addr));
	fprintf(fp, "%sDCS subaddress        %s\n", indent,
			format_atm_addr(&dcsp->sd_subaddr));
	fprintf(fp, "%sSocket:               %d\n", indent,
			dcsp->sd_sock);
	fprintf(fp, "%sOpen VCC Retry Timer:\n", indent);
	fprintf(fp, "%sHello FSM State:      %s\n", indent,
			format_hfsm_state(dcsp->sd_hello_state));
	fprintf(fp, "%sHello Interval:       %d\n", indent,
			dcsp->sd_hello_int);
	fprintf(fp, "%sHello Dead Factor:    %d\n", indent,
			dcsp->sd_hello_df);
	fprintf(fp, "%sHello Rcvd:           %d\n", indent,
			dcsp->sd_hello_rcvd);
	fprintf(fp, "%sCA FSM State:         %s\n", indent,
			format_cafsm_state(dcsp->sd_ca_state));
	fprintf(fp, "%sCA Seq. No.:          0x%lx\n", indent,
			dcsp->sd_ca_seq);
	fprintf(fp, "%sCA Rexmit Int:        %d\n", indent,
			dcsp->sd_ca_rexmt_int);
	fprintf(fp, "%sCA Retransmit Msg:    %p\n", indent,
			dcsp->sd_ca_rexmt_msg);
	fprintf(fp, "%sCSASs to send:        ", indent);
	if (dcsp->sd_ca_csas == (Scsp_cse *)0) {
		fprintf(fp, "Empty\n");
	} else {
		fprintf(fp, "%p\n", dcsp->sd_ca_csas);
	}
	fprintf(fp, "%sCSUS Rexmit Int:      %d\n", indent,
			dcsp->sd_csus_rexmt_int);
	fprintf(fp, "%sCache Request List:   ", indent);
	if (dcsp->sd_crl == (Scsp_csa *)0) {
		fprintf(fp, "Empty\n");
	} else {
		fprintf(fp, "%p\n", dcsp->sd_crl);
	}
	fprintf(fp, "%sCSUS Rexmit Msg:      %p\n", indent,
			dcsp->sd_csus_rexmt_msg);
	fprintf(fp, "%sCSA Hop count:        %d\n", indent,
			dcsp->sd_hops);
	fprintf(fp, "%sCSAs Pending ACK:     %p\n", indent,
			dcsp->sd_csu_ack_pend);
	fprintf(fp, "%sCSAs ACKed:           %p\n", indent,
			dcsp->sd_csu_ack);
	fprintf(fp, "%sCSU Req Rexmit Int:   %d\n", indent,
			dcsp->sd_csu_rexmt_int);
	fprintf(fp, "%sCSU Req Rexmit Max:   %d\n", indent,
			dcsp->sd_csu_rexmt_max);
	fprintf(fp, "%sCSU Req Rexmit Queue  ", indent);
	if (!dcsp->sd_csu_rexmt) {
		fprintf(fp, "Empty\n");
	} else {
		fprintf(fp, "%p\n", dcsp->sd_csu_rexmt);
	}
	fprintf(fp, "%sClient I/F state:     %d\n", indent,
			dcsp->sd_client_state);

	/*
	 * Print the list of CSASs waiting to be sent
	 */
	if (dcsp->sd_ca_csas) {
		fprintf(fp, "\n%sCSASs to send:", indent);
		inc_indent();
		for (csep = dcsp->sd_ca_csas; csep;
				csep = csep->sc_next) {
			fprintf(fp, "%sCache summary entry at %p\n",
					indent, csep);
			print_scsp_cse(fp, csep);
		}
		dec_indent();
	}

	/*
	 * Print the Cache Request List
	 */
	if (dcsp->sd_crl) {
		fprintf(fp, "\n%sCache Request List:\n", indent);
		inc_indent();
		for (csap = dcsp->sd_crl; csap; csap = csap->next) {
			fprintf(fp, "%sCSA at %p\n", indent, csap);
			print_scsp_csa(fp, csap);
		}
		dec_indent();
	}

	/*
	 * Print the CSU retransmit queue
	 */
	if (dcsp->sd_csu_rexmt) {
		fprintf(fp, "\n%sCSU Req Rexmit Queue:\n", indent);
		inc_indent();
		for (rxp = dcsp->sd_csu_rexmt; rxp;
				rxp = rxp->sr_next) {
			fprintf(fp, "%sCSU Rexmit Block at %p\n",
					indent, rxp);
			print_scsp_csu_rexmt(fp, rxp);
		}
		dec_indent();
	}

	dec_indent();
}


/*
 * Print SCSP's control blocks
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	None
 *
 */
void
print_scsp_dump()
{
	int		i;
	Scsp_server	*ssp;
	Scsp_dcs	*dcsp;
	Scsp_cse	*scp;
	Scsp_pending	*pp;
	FILE		*df;
	char		fname[64];
	static int	dump_no = 0;

	/*
	 * Build a file name
	 */
	UM_ZERO(fname, sizeof(fname));
	sprintf(fname, "/tmp/scspd.%d.%03d.out", getpid(), dump_no++);

	/*
	 * Open the output file
	 */
	df = fopen(fname, "w");
	if (df == (FILE *)0)
		return;

	/*
	 * Dump the server control blocks
	 */
	for (ssp = scsp_server_head; ssp; ssp = ssp->ss_next) {
		print_scsp_server(df, ssp);
		fprintf(df, "\n");

		/*
		 * Print the client's cache summary
		 */
		for (i = 0; i < SCSP_HASHSZ; i++) {
			for (scp = ssp->ss_cache[i]; scp;
					scp = scp->sc_next) {
				print_scsp_cse(df, scp);
				fprintf(df, "\n");
			}
		}

		/*
		 * Print the client's DCS control blocks
		 */
		for (dcsp = ssp->ss_dcs; dcsp; dcsp = dcsp->sd_next) {
			print_scsp_dcs(df, dcsp);
			fprintf(df, "\n\n");
		}
		fprintf(df, "\n\n");
	}

	/*
	 * Print the pending connection blocks
	 */
	for (pp = scsp_pending_head; pp; pp = pp->sp_next) {
		print_scsp_pending(df, pp);
		fprintf(df, "\n");
	}

	/*
	 * Close the output file
	 */
	(void)fclose(df);
}
