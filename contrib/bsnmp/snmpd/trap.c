/*
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 *	All rights reserved.
 *
 * Author: Harti Brandt <harti@freebsd.org>
 *
 * Redistribution of this software and documentation and use in source and
 * binary forms, with or without modification, are permitted provided that
 * the following conditions are met:
 *
 * 1. Redistributions of source code or documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE AND DOCUMENTATION IS PROVIDED BY FRAUNHOFER FOKUS
 * AND ITS CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * FRAUNHOFER FOKUS OR ITS CONTRIBUTORS  BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Begemot: bsnmp/snmpd/trap.c,v 1.5 2003/01/28 13:44:35 hbb Exp $
 *
 * TrapSinkTable
 */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "snmpmod.h"
#include "snmpd.h"
#include "tree.h"
#include "oid.h"

struct trapsink_list trapsink_list = TAILQ_HEAD_INITIALIZER(trapsink_list);

static const struct asn_oid oid_begemotTrapSinkTable =
    OIDX_begemotTrapSinkTable;
static const struct asn_oid oid_sysUpTime = OIDX_sysUpTime;
static const struct asn_oid oid_snmpTrapOID = OIDX_snmpTrapOID;

struct trapsink_dep {
	struct snmp_dependency dep;
	u_int	set;
	u_int	status;
	u_char	comm[SNMP_COMMUNITY_MAXLEN + 1];
	u_int	version;
	u_int	rb;
	u_int	rb_status;
	u_int	rb_version;
	u_char	rb_comm[SNMP_COMMUNITY_MAXLEN + 1];
};
enum {
	TDEP_STATUS	= 0x0001,
	TDEP_COMM	= 0x0002,
	TDEP_VERSION	= 0x0004,

	TDEP_CREATE	= 0x0001,
	TDEP_MODIFY	= 0x0002,
	TDEP_DESTROY	= 0x0004,
};

static int
trapsink_create(struct trapsink_dep *tdep)
{
	struct trapsink *t;
	struct sockaddr_in sa;

	if ((t = malloc(sizeof(*t))) == NULL)
		return (SNMP_ERR_RES_UNAVAIL);

	t->index = tdep->dep.idx;
	t->status = TRAPSINK_NOT_READY;
	t->comm[0] = '\0';
	t->version = TRAPSINK_V2;

	if ((t->socket = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		syslog(LOG_ERR, "socket(UDP): %m");
		free(t);
		return (SNMP_ERR_RES_UNAVAIL);
	}
	(void)shutdown(t->socket, SHUT_RD);

	sa.sin_len = sizeof(sa);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl((t->index.subs[0] << 24) |
	    (t->index.subs[1] << 16) | (t->index.subs[2] << 8) |
	    (t->index.subs[3] << 0));
	sa.sin_port = htons(t->index.subs[4]);

	if (connect(t->socket, (struct sockaddr *)&sa, sa.sin_len) == -1) {
		syslog(LOG_ERR, "connect(%s,%u): %m",
		    inet_ntoa(sa.sin_addr), ntohl(sa.sin_port));
		(void)close(t->socket);
		free(t);
		return (SNMP_ERR_GENERR);
	}

	if (tdep->set & TDEP_VERSION)
		t->version = tdep->version;
	if (tdep->set & TDEP_COMM)
		strcpy(t->comm, tdep->comm);

	if (t->comm[0] != '\0')
		t->status = TRAPSINK_NOT_IN_SERVICE;

	/* look whether we should activate */
	if (tdep->status == 4) {
		if (t->status == TRAPSINK_NOT_READY) {
			if (t->socket != -1)
				(void)close(t->socket);
			free(t);
			return (SNMP_ERR_INCONS_VALUE);
		}
		t->status = TRAPSINK_ACTIVE;
	}

	INSERT_OBJECT_OID(t, &trapsink_list);

	tdep->rb |= TDEP_CREATE;

	return (SNMP_ERR_NOERROR);
}

static void
trapsink_free(struct trapsink *t)
{
	TAILQ_REMOVE(&trapsink_list, t, link);
	if (t->socket != -1)
		(void)close(t->socket);
	free(t);
}

static int
trapsink_modify(struct trapsink *t, struct trapsink_dep *tdep)
{
	tdep->rb_status = t->status;
	tdep->rb_version = t->version;
	strcpy(tdep->rb_comm, t->comm);

	if (tdep->set & TDEP_STATUS) {
		/* if we are active and should move to not_in_service do
		 * this first */
		if (tdep->status == 2 && tdep->rb_status == TRAPSINK_ACTIVE) {
			t->status = TRAPSINK_NOT_IN_SERVICE;
			tdep->rb |= TDEP_MODIFY;
		}
	}

	if (tdep->set & TDEP_VERSION)
		t->version = tdep->version;
	if (tdep->set & TDEP_COMM)
		strcpy(t->comm, tdep->comm);

	if (tdep->set & TDEP_STATUS) {
		/* if we were inactive and should go active - do this now */
		if (tdep->status == 1 && tdep->rb_status != TRAPSINK_ACTIVE) {
			if (t->comm[0] == '\0') {
				t->status = tdep->rb_status;
				t->version = tdep->rb_version;
				strcpy(t->comm, tdep->rb_comm);
				return (SNMP_ERR_INCONS_VALUE);
			}
			t->status = TRAPSINK_ACTIVE;
			tdep->rb |= TDEP_MODIFY;
		}
	}
	return (SNMP_ERR_NOERROR);
}

static int
trapsink_unmodify(struct trapsink *t, struct trapsink_dep *tdep)
{
	if (tdep->set & TDEP_STATUS)
		t->status = tdep->rb_status;
	if (tdep->set & TDEP_VERSION)
		t->version = tdep->rb_version;
	if (tdep->set & TDEP_COMM)
		strcpy(t->comm, tdep->rb_comm);
	
	return (SNMP_ERR_NOERROR);
}

static void
trapsink_finish(struct snmp_context *ctx __unused, int fail, void *arg)
{
	struct trapsink *t = arg;

	if (!fail)
		trapsink_free(t);
}

static int
trapsink_destroy(struct snmp_context *ctx, struct trapsink *t,
    struct trapsink_dep *tdep)
{
	if (snmp_set_atfinish(ctx, trapsink_finish, t))
		return (SNMP_ERR_RES_UNAVAIL);
	t->status = TRAPSINK_DESTROY;
	tdep->rb_status = t->status;
	tdep->rb |= TDEP_DESTROY;
	return (SNMP_ERR_NOERROR);
}

static int
trapsink_undestroy(struct trapsink *t, struct trapsink_dep *tdep)
{
	t->status = tdep->rb_status;
	return (SNMP_ERR_NOERROR);
}

static int
trapsink_dep(struct snmp_context *ctx, struct snmp_dependency *dep,
    enum snmp_depop op)
{
	struct trapsink_dep *tdep = (struct trapsink_dep *)dep;
	struct trapsink *t;

	t = FIND_OBJECT_OID(&trapsink_list, &dep->idx, 0);

	switch (op) {

	  case SNMP_DEPOP_COMMIT:
		if (tdep->set & TDEP_STATUS) {
			switch (tdep->status) {

			  case 1:
			  case 2:
				if (t == NULL)
					return (SNMP_ERR_INCONS_VALUE);
				return (trapsink_modify(t, tdep));

			  case 4:
			  case 5:
				if (t != NULL)
					return (SNMP_ERR_INCONS_VALUE);
				return (trapsink_create(tdep));

			  case 6:
				if (t == NULL)
					return (SNMP_ERR_NOERROR);
				return (trapsink_destroy(ctx, t, tdep));
			}
		} else if (tdep->set != 0)
			return (trapsink_modify(t, tdep));

		return (SNMP_ERR_NOERROR);

	  case SNMP_DEPOP_ROLLBACK:
		if (tdep->rb & TDEP_CREATE) {
			trapsink_free(t);
			return (SNMP_ERR_NOERROR);
		}
		if (tdep->rb & TDEP_MODIFY)
			return (trapsink_unmodify(t, tdep));
		if(tdep->rb & TDEP_DESTROY)
			return (trapsink_undestroy(t, tdep));
		return (SNMP_ERR_NOERROR);
	}
	abort();
}

int
op_trapsink(struct snmp_context *ctx, struct snmp_value *value,
    u_int sub, u_int iidx, enum snmp_op op)
{
	struct trapsink *t;
	u_char ipa[4];
	int32_t port;
	struct asn_oid idx;
	struct trapsink_dep *tdep;
	u_char *p;

	t = NULL;		/* gcc */

	switch (op) {

	  case SNMP_OP_GETNEXT:
		if ((t = NEXT_OBJECT_OID(&trapsink_list, &value->var, sub)) == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		index_append(&value->var, sub, &t->index);
		break;

	  case SNMP_OP_GET:
		if ((t = FIND_OBJECT_OID(&trapsink_list, &value->var, sub)) == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		break;

	  case SNMP_OP_SET:
		if (index_decode(&value->var, sub, iidx, ipa, &port) ||
		    port == 0 || port > 65535)
			return (SNMP_ERR_NO_CREATION);
		t = FIND_OBJECT_OID(&trapsink_list, &value->var, sub);

		asn_slice_oid(&idx, &value->var, sub, value->var.len);

		tdep = (struct trapsink_dep *)snmp_dep_lookup(ctx,
		    &oid_begemotTrapSinkTable, &idx,
		    sizeof(*tdep), trapsink_dep);
		if (tdep == NULL)
			return (SNMP_ERR_RES_UNAVAIL);

		switch (value->var.subs[sub - 1]) {

		  case LEAF_begemotTrapSinkStatus:
			if (tdep->set & TDEP_STATUS)
				return (SNMP_ERR_INCONS_VALUE);
			switch (value->v.integer) {

			  case 1:
			  case 2:
				if (t == NULL)
					return (SNMP_ERR_INCONS_VALUE);
				break;

			  case 4:
			  case 5:
				if (t != NULL)
					return (SNMP_ERR_INCONS_VALUE);
				break;

			  case 6:
				break;

			  default:
				return (SNMP_ERR_WRONG_VALUE);
			}
			tdep->status = value->v.integer;
			tdep->set |= TDEP_STATUS;
			return (SNMP_ERR_NOERROR);

		  case LEAF_begemotTrapSinkComm:
			if (tdep->set & TDEP_COMM)
				return (SNMP_ERR_INCONS_VALUE);
			if (value->v.octetstring.len == 0 ||
			    value->v.octetstring.len > SNMP_COMMUNITY_MAXLEN)
				return (SNMP_ERR_WRONG_VALUE);
			for (p = value->v.octetstring.octets;
			     p < value->v.octetstring.octets + value->v.octetstring.len;
			     p++) {
				if (!isascii(*p) || !isprint(*p))
					return (SNMP_ERR_WRONG_VALUE);
			}
			tdep->set |= TDEP_COMM;
			strncpy(tdep->comm, value->v.octetstring.octets,
			    value->v.octetstring.len);
			tdep->comm[value->v.octetstring.len] = '\0';
			return (SNMP_ERR_NOERROR);

		  case LEAF_begemotTrapSinkVersion:
			if (tdep->set & TDEP_VERSION)
				return (SNMP_ERR_INCONS_VALUE);
			if (value->v.integer != TRAPSINK_V1 &&
			    value->v.integer != TRAPSINK_V2)
				return (SNMP_ERR_WRONG_VALUE);
			tdep->version = value->v.integer;
			tdep->set |= TDEP_VERSION;
			return (SNMP_ERR_NOERROR);
		}
		if (t == NULL)
			return (SNMP_ERR_INCONS_NAME);
		else
			return (SNMP_ERR_NOT_WRITEABLE);


	  case SNMP_OP_ROLLBACK:
	  case SNMP_OP_COMMIT:
		return (SNMP_ERR_NOERROR);
	}

	switch (value->var.subs[sub - 1]) {

	  case LEAF_begemotTrapSinkStatus:
		value->v.integer = t->status;
		break;

	  case LEAF_begemotTrapSinkComm:
		return (string_get(value, t->comm, -1));

	  case LEAF_begemotTrapSinkVersion:
		value->v.integer = t->version;
		break;

	}
	return (SNMP_ERR_NOERROR);
}

void
snmp_send_trap(const struct asn_oid *trap_oid, ...)
{
	struct snmp_pdu pdu;
	struct trapsink *t;
	const struct snmp_value *v;
	va_list ap;
	u_char *sndbuf;
	size_t sndlen;
	ssize_t len;

	TAILQ_FOREACH(t, &trapsink_list, link) {
		if (t->status != TRAPSINK_ACTIVE)
			continue;
		memset(&pdu, 0, sizeof(pdu));
		strcpy(pdu.community, t->comm);
		if (t->version == TRAPSINK_V1) {
			pdu.version = SNMP_V1;
			pdu.type = SNMP_PDU_TRAP;
			pdu.enterprise = systemg.object_id;
			memcpy(pdu.agent_addr, snmpd.trap1addr, 4);
			pdu.generic_trap = trap_oid->subs[trap_oid->len - 1] - 1;
			pdu.specific_trap = 0;
			pdu.time_stamp = get_ticks() - start_tick;

			pdu.nbindings = 0;
		} else {
			pdu.version = SNMP_V2c;
			pdu.type = SNMP_PDU_TRAP2;
			pdu.request_id = reqid_next(trap_reqid);
			pdu.error_index = 0;
			pdu.error_status = SNMP_ERR_NOERROR;

			pdu.bindings[0].var = oid_sysUpTime;
			pdu.bindings[0].var.subs[pdu.bindings[0].var.len++] = 0;
			pdu.bindings[0].syntax = SNMP_SYNTAX_TIMETICKS;
			pdu.bindings[0].v.uint32 = get_ticks() - start_tick;

			pdu.bindings[1].var = oid_snmpTrapOID;
			pdu.bindings[1].var.subs[pdu.bindings[1].var.len++] = 0;
			pdu.bindings[1].syntax = SNMP_SYNTAX_OID;
			pdu.bindings[1].v.oid = *trap_oid;

			pdu.nbindings = 2;
		}

		va_start(ap, trap_oid);
		while ((v = va_arg(ap, const struct snmp_value *)) != NULL)
			pdu.bindings[pdu.nbindings++] = *v;
		va_end(ap);

		if ((sndbuf = buf_alloc(1)) == NULL) {
			syslog(LOG_ERR, "trap send buffer: %m");
			return;
		}

		snmp_output(&pdu, sndbuf, &sndlen, "TRAP");

		if ((len = send(t->socket, sndbuf, sndlen, 0)) == -1)
			syslog(LOG_ERR, "send: %m");
		else if ((size_t)len != sndlen)
			syslog(LOG_ERR, "send: short write %zu/%zu",
			    sndlen, (size_t)len);

		free(sndbuf);
	}
}
