/*
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 *	All rights reserved.
 *
 * Author: Harti Brandt <harti@freebsd.org>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Begemot: bsnmp/lib/snmp.h,v 1.30 2004/08/06 08:46:54 brandt Exp $
 *
 * Header file for SNMP functions.
 */
#ifndef snmp_h_
#define snmp_h_

#include <sys/types.h>

#define SNMP_COMMUNITY_MAXLEN	128
#define SNMP_MAX_BINDINGS	100

enum snmp_syntax {
	SNMP_SYNTAX_NULL	= 0,
	SNMP_SYNTAX_INTEGER,		/* == INTEGER32 */
	SNMP_SYNTAX_OCTETSTRING,
	SNMP_SYNTAX_OID,
	SNMP_SYNTAX_IPADDRESS,
	SNMP_SYNTAX_COUNTER,
	SNMP_SYNTAX_GAUGE,		/* == UNSIGNED32 */
	SNMP_SYNTAX_TIMETICKS,

	/* v2 additions */
	SNMP_SYNTAX_COUNTER64,
	SNMP_SYNTAX_NOSUCHOBJECT,	/* exception */
	SNMP_SYNTAX_NOSUCHINSTANCE,	/* exception */
	SNMP_SYNTAX_ENDOFMIBVIEW,	/* exception */
};

struct snmp_value {
	struct asn_oid		var;
	enum snmp_syntax	syntax;
	union snmp_values {
	  int32_t		integer;	/* also integer32 */
	  struct {
	    u_int		len;
	    u_char		*octets;
	  }			octetstring;
	  struct asn_oid	oid;
	  u_char		ipaddress[4];
	  uint32_t		uint32;		/* also gauge32, counter32,
						   unsigned32, timeticks */
	  uint64_t		counter64;
	}			v;
};

enum snmp_version {
	SNMP_Verr = 0,
	SNMP_V1 = 1,
	SNMP_V2c,
};

struct snmp_pdu {
	char		community[SNMP_COMMUNITY_MAXLEN + 1];
	enum snmp_version version;
	u_int		type;

	/* trap only */
	struct asn_oid	enterprise;
	u_char		agent_addr[4];
	int32_t		generic_trap;
	int32_t		specific_trap;
	uint32_t	time_stamp;

	/* others */
	int32_t		request_id;
	int32_t		error_status;
	int32_t		error_index;

	/* fixes for encoding */
	u_char		*outer_ptr;
	u_char		*pdu_ptr;
	u_char		*vars_ptr;

	struct snmp_value bindings[SNMP_MAX_BINDINGS];
	u_int		nbindings;
};
#define snmp_v1_pdu snmp_pdu

#define SNMP_PDU_GET		0
#define SNMP_PDU_GETNEXT	1
#define SNMP_PDU_RESPONSE	2
#define SNMP_PDU_SET		3
#define SNMP_PDU_TRAP		4	/* v1 */
#define SNMP_PDU_GETBULK	5	/* v2 */
#define SNMP_PDU_INFORM		6	/* v2 */
#define SNMP_PDU_TRAP2		7	/* v2 */
#define SNMP_PDU_REPORT		8	/* v2 */

#define SNMP_ERR_NOERROR	0
#define SNMP_ERR_TOOBIG		1
#define SNMP_ERR_NOSUCHNAME	2	/* v1 */
#define SNMP_ERR_BADVALUE	3	/* v1 */
#define SNMP_ERR_READONLY	4	/* v1 */
#define SNMP_ERR_GENERR		5
#define SNMP_ERR_NO_ACCESS	6	/* v2 */
#define SNMP_ERR_WRONG_TYPE	7	/* v2 */
#define SNMP_ERR_WRONG_LENGTH	8	/* v2 */
#define SNMP_ERR_WRONG_ENCODING	9	/* v2 */
#define SNMP_ERR_WRONG_VALUE	10	/* v2 */
#define SNMP_ERR_NO_CREATION	11	/* v2 */
#define SNMP_ERR_INCONS_VALUE	12	/* v2 */
#define SNMP_ERR_RES_UNAVAIL	13	/* v2 */
#define SNMP_ERR_COMMIT_FAILED	14	/* v2 */
#define SNMP_ERR_UNDO_FAILED	15	/* v2 */
#define SNMP_ERR_AUTH_ERR	16	/* v2 */
#define SNMP_ERR_NOT_WRITEABLE	17	/* v2 */
#define SNMP_ERR_INCONS_NAME	18	/* v2 */

#define SNMP_TRAP_COLDSTART	0
#define SNMP_TRAP_WARMSTART	1
#define SNMP_TRAP_LINKDOWN	2
#define SNMP_TRAP_LINKUP	3
#define SNMP_TRAP_AUTHENTICATION_FAILURE	4
#define SNMP_TRAP_EGP_NEIGHBOR_LOSS	5
#define SNMP_TRAP_ENTERPRISE	6

enum snmp_code {
	SNMP_CODE_OK = 0,
	SNMP_CODE_FAILED,
	SNMP_CODE_BADVERS,
	SNMP_CODE_BADLEN,
	SNMP_CODE_BADENC,
	SNMP_CODE_OORANGE,
};

void snmp_value_free(struct snmp_value *);
int snmp_value_parse(const char *, enum snmp_syntax, union snmp_values *);
int snmp_value_copy(struct snmp_value *, const struct snmp_value *);

void snmp_pdu_free(struct snmp_pdu *);
enum snmp_code snmp_pdu_decode(struct asn_buf *b, struct snmp_pdu *pdu, int32_t *);
enum snmp_code snmp_pdu_encode(struct snmp_pdu *pdu, struct asn_buf *resp_b);

int snmp_pdu_snoop(const struct asn_buf *);

void snmp_pdu_dump(const struct snmp_pdu *pdu);

extern void (*snmp_error)(const char *, ...);
extern void (*snmp_printf)(const char *, ...);

#define TRUTH_MK(F) ((F) ? 1 : 2)
#define TRUTH_GET(T) (((T) == 1) ? 1 : 0)
#define TRUTH_OK(T)  ((T) == 1 || (T) == 2)

#endif
