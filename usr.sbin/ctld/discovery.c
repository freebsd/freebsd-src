/*-
 * Copyright (c) 2012 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "ctld.h"
#include "iscsi_proto.h"

static struct pdu *
text_receive(struct connection *conn)
{
	struct pdu *request;
	struct iscsi_bhs_text_request *bhstr;

	request = pdu_new(conn);
	pdu_receive(request);
	if ((request->pdu_bhs->bhs_opcode & ~ISCSI_BHS_OPCODE_IMMEDIATE) !=
	    ISCSI_BHS_OPCODE_TEXT_REQUEST)
		log_errx(1, "protocol error: received invalid opcode 0x%x",
		    request->pdu_bhs->bhs_opcode);
	bhstr = (struct iscsi_bhs_text_request *)request->pdu_bhs;
#if 0
	if ((bhstr->bhstr_flags & ISCSI_BHSTR_FLAGS_FINAL) == 0)
		log_errx(1, "received Text PDU without the \"F\" flag");
#endif
	/*
	 * XXX: Implement the C flag some day.
	 */
	if ((bhstr->bhstr_flags & BHSTR_FLAGS_CONTINUE) != 0)
		log_errx(1, "received Text PDU with unsupported \"C\" flag");
	if (request->pdu_data_len == 0)
		log_errx(1, "received Text PDU with empty data segment");

	if (ntohl(bhstr->bhstr_cmdsn) < conn->conn_cmdsn) {
		log_errx(1, "received Text PDU with decreasing CmdSN: "
		    "was %d, is %d", conn->conn_cmdsn, ntohl(bhstr->bhstr_cmdsn));
	}
	if (ntohl(bhstr->bhstr_expstatsn) != conn->conn_statsn) {
		log_errx(1, "received Text PDU with wrong StatSN: "
		    "is %d, should be %d", ntohl(bhstr->bhstr_expstatsn),
		    conn->conn_statsn);
	}
	conn->conn_cmdsn = ntohl(bhstr->bhstr_cmdsn);

	return (request);
}

static struct pdu *
text_new_response(struct pdu *request)
{
	struct pdu *response;
	struct connection *conn;
	struct iscsi_bhs_text_request *bhstr;
	struct iscsi_bhs_text_response *bhstr2;

	bhstr = (struct iscsi_bhs_text_request *)request->pdu_bhs;
	conn = request->pdu_connection;

	response = pdu_new_response(request);
	bhstr2 = (struct iscsi_bhs_text_response *)response->pdu_bhs;
	bhstr2->bhstr_opcode = ISCSI_BHS_OPCODE_TEXT_RESPONSE;
	bhstr2->bhstr_flags = BHSTR_FLAGS_FINAL;
	bhstr2->bhstr_lun = bhstr->bhstr_lun;
	bhstr2->bhstr_initiator_task_tag = bhstr->bhstr_initiator_task_tag;
	bhstr2->bhstr_target_transfer_tag = bhstr->bhstr_target_transfer_tag;
	bhstr2->bhstr_statsn = htonl(conn->conn_statsn++);
	bhstr2->bhstr_expcmdsn = htonl(conn->conn_cmdsn);
	bhstr2->bhstr_maxcmdsn = htonl(conn->conn_cmdsn);

	return (response);
}

static struct pdu *
logout_receive(struct connection *conn)
{
	struct pdu *request;
	struct iscsi_bhs_logout_request *bhslr;

	request = pdu_new(conn);
	pdu_receive(request);
	if ((request->pdu_bhs->bhs_opcode & ~ISCSI_BHS_OPCODE_IMMEDIATE) !=
	    ISCSI_BHS_OPCODE_LOGOUT_REQUEST)
		log_errx(1, "protocol error: received invalid opcode 0x%x",
		    request->pdu_bhs->bhs_opcode);
	bhslr = (struct iscsi_bhs_logout_request *)request->pdu_bhs;
	if ((bhslr->bhslr_reason & 0x7f) != BHSLR_REASON_CLOSE_SESSION)
		log_debugx("received Logout PDU with invalid reason 0x%x; "
		    "continuing anyway", bhslr->bhslr_reason & 0x7f);
	if (ntohl(bhslr->bhslr_cmdsn) < conn->conn_cmdsn) {
		log_errx(1, "received Logout PDU with decreasing CmdSN: "
		    "was %d, is %d", conn->conn_cmdsn,
		    ntohl(bhslr->bhslr_cmdsn));
	}
	if (ntohl(bhslr->bhslr_expstatsn) != conn->conn_statsn) {
		log_errx(1, "received Logout PDU with wrong StatSN: "
		    "is %d, should be %d", ntohl(bhslr->bhslr_expstatsn),
		    conn->conn_statsn);
	}
	conn->conn_cmdsn = ntohl(bhslr->bhslr_cmdsn);

	return (request);
}

static struct pdu *
logout_new_response(struct pdu *request)
{
	struct pdu *response;
	struct connection *conn;
	struct iscsi_bhs_logout_request *bhslr;
	struct iscsi_bhs_logout_response *bhslr2;

	bhslr = (struct iscsi_bhs_logout_request *)request->pdu_bhs;
	conn = request->pdu_connection;

	response = pdu_new_response(request);
	bhslr2 = (struct iscsi_bhs_logout_response *)response->pdu_bhs;
	bhslr2->bhslr_opcode = ISCSI_BHS_OPCODE_LOGOUT_RESPONSE;
	bhslr2->bhslr_flags = 0x80;
	bhslr2->bhslr_response = BHSLR_RESPONSE_CLOSED_SUCCESSFULLY;
	bhslr2->bhslr_initiator_task_tag = bhslr->bhslr_initiator_task_tag;
	bhslr2->bhslr_statsn = htonl(conn->conn_statsn++);
	bhslr2->bhslr_expcmdsn = htonl(conn->conn_cmdsn);
	bhslr2->bhslr_maxcmdsn = htonl(conn->conn_cmdsn);

	return (response);
}

void
discovery(struct connection *conn)
{
	struct pdu *request, *response;
	struct keys *request_keys, *response_keys;
	struct target *targ;
	const char *send_targets;

	log_debugx("beginning discovery session; waiting for Text PDU");
	request = text_receive(conn);
	request_keys = keys_new();
	keys_load(request_keys, request);

	send_targets = keys_find(request_keys, "SendTargets");
	if (send_targets == NULL)
		log_errx(1, "received Text PDU without SendTargets");

	response = text_new_response(request);
	response_keys = keys_new();

	if (strcmp(send_targets, "All") == 0) {
		TAILQ_FOREACH(targ,
		    &conn->conn_portal->p_portal_group->pg_conf->conf_targets,
		    t_next) {
			if (targ->t_portal_group !=
			    conn->conn_portal->p_portal_group) {
				log_debugx("not returning target \"%s\"; "
				    "belongs to a different portal group",
				    targ->t_iqn);
				continue;
			}
			keys_add(response_keys, "TargetName", targ->t_iqn);
		}
	} else {
		targ = target_find(conn->conn_portal->p_portal_group->pg_conf,
		    send_targets);
		if (targ == NULL) {
			log_debugx("initiator requested information on unknown "
			    "target \"%s\"; returning nothing", send_targets);
		} else {
			keys_add(response_keys, "TargetName", targ->t_iqn);
		}
	}
	keys_save(response_keys, response);

	pdu_send(response);
	pdu_delete(response);
	keys_delete(response_keys);
	pdu_delete(request);
	keys_delete(request_keys);

	log_debugx("done sending targets; waiting for Logout PDU");
	request = logout_receive(conn);
	response = logout_new_response(request);

	pdu_send(response);
	pdu_delete(response);
	pdu_delete(request);

	log_debugx("discovery session done");
}
