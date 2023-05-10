/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 The FreeBSD Foundation
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
 */

#include <sys/types.h>
#include <netinet/in.h>

#include <stdlib.h>
#include <string.h>

#include <iscsi_proto.h>
#include "libiscsiutil.h"

/* Construct a new TextRequest PDU. */
static struct pdu *
text_new_request(struct connection *conn, uint32_t ttt)
{
	struct pdu *request;
	struct iscsi_bhs_text_request *bhstr;

	request = pdu_new(conn);
	bhstr = (struct iscsi_bhs_text_request *)request->pdu_bhs;
	bhstr->bhstr_opcode = ISCSI_BHS_OPCODE_TEXT_REQUEST |
	    ISCSI_BHS_OPCODE_IMMEDIATE;
	bhstr->bhstr_flags = BHSTR_FLAGS_FINAL;
	bhstr->bhstr_initiator_task_tag = 0;
	bhstr->bhstr_target_transfer_tag = ttt;

	bhstr->bhstr_cmdsn = conn->conn_cmdsn;
	bhstr->bhstr_expstatsn = htonl(conn->conn_statsn + 1);

	return (request);
}

/* Receive a TextRequest PDU from a connection. */
static struct pdu *
text_receive_request(struct connection *conn)
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

	/*
	 * XXX: Implement the C flag some day.
	 */
	if ((bhstr->bhstr_flags & (BHSTR_FLAGS_FINAL | BHSTR_FLAGS_CONTINUE)) !=
	    BHSTR_FLAGS_FINAL)
		log_errx(1, "received TextRequest PDU with invalid "
		    "flags: %u", bhstr->bhstr_flags);
	if (ISCSI_SNLT(ntohl(bhstr->bhstr_cmdsn), conn->conn_cmdsn)) {
		log_errx(1, "received TextRequest PDU with decreasing CmdSN: "
		    "was %u, is %u", conn->conn_cmdsn, ntohl(bhstr->bhstr_cmdsn));
	}
	conn->conn_cmdsn = ntohl(bhstr->bhstr_cmdsn);
	if ((bhstr->bhstr_opcode & ISCSI_BHS_OPCODE_IMMEDIATE) == 0)
		conn->conn_cmdsn++;

	return (request);
}

/* Construct a new TextResponse PDU in reply to a request. */
static struct pdu *
text_new_response(struct pdu *request, uint32_t ttt, bool final)
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
	if (final)
		bhstr2->bhstr_flags = BHSTR_FLAGS_FINAL;
	else
		bhstr2->bhstr_flags = BHSTR_FLAGS_CONTINUE;
	bhstr2->bhstr_lun = bhstr->bhstr_lun;
	bhstr2->bhstr_initiator_task_tag = bhstr->bhstr_initiator_task_tag;
	bhstr2->bhstr_target_transfer_tag = ttt;
	bhstr2->bhstr_statsn = htonl(conn->conn_statsn++);
	bhstr2->bhstr_expcmdsn = htonl(conn->conn_cmdsn);
	bhstr2->bhstr_maxcmdsn = htonl(conn->conn_cmdsn);

	return (response);
}

/* Receive a TextResponse PDU from a connection. */
static struct pdu *
text_receive_response(struct connection *conn)
{
	struct pdu *response;
	struct iscsi_bhs_text_response *bhstr;
	uint8_t flags;

	response = pdu_new(conn);
	pdu_receive(response);
	if (response->pdu_bhs->bhs_opcode != ISCSI_BHS_OPCODE_TEXT_RESPONSE)
		log_errx(1, "protocol error: received invalid opcode 0x%x",
		    response->pdu_bhs->bhs_opcode);
	bhstr = (struct iscsi_bhs_text_response *)response->pdu_bhs;
	flags = bhstr->bhstr_flags & (BHSTR_FLAGS_FINAL | BHSTR_FLAGS_CONTINUE);
	switch (flags) {
	case BHSTR_FLAGS_CONTINUE:
		if (bhstr->bhstr_target_transfer_tag == 0xffffffff)
			log_errx(1, "received continue TextResponse PDU with "
			    "invalid TTT 0x%x",
			    bhstr->bhstr_target_transfer_tag);
		break;
	case BHSTR_FLAGS_FINAL:
		if (bhstr->bhstr_target_transfer_tag != 0xffffffff)
			log_errx(1, "received final TextResponse PDU with "
			    "invalid TTT 0x%x",
			    bhstr->bhstr_target_transfer_tag);
		break;
	default:
		log_errx(1, "received TextResponse PDU with invalid "
		    "flags: %u", bhstr->bhstr_flags);
	}
	if (ntohl(bhstr->bhstr_statsn) != conn->conn_statsn + 1) {
		log_errx(1, "received TextResponse PDU with wrong StatSN: "
		    "is %u, should be %u", ntohl(bhstr->bhstr_statsn),
		    conn->conn_statsn + 1);
	}
	conn->conn_statsn = ntohl(bhstr->bhstr_statsn);

	return (response);
}

/*
 * Send a list of keys from the initiator to the target in a
 * TextRequest PDU.
 */
void
text_send_request(struct connection *conn, struct keys *request_keys)
{
	struct pdu *request;

	request = text_new_request(conn, 0xffffffff);
	keys_save_pdu(request_keys, request);
	if (request->pdu_data_len == 0)
		log_errx(1, "No keys to send in a TextRequest");
	if (request->pdu_data_len >
	    (size_t)conn->conn_max_send_data_segment_length)
		log_errx(1, "Keys to send in TextRequest are too long");

	pdu_send(request);
	pdu_delete(request);
}

/*
 * Read a list of keys from the target in a series of TextResponse
 * PDUs.
 */
struct keys *
text_read_response(struct connection *conn)
{
	struct keys *response_keys;
	char *keys_data;
	size_t keys_len;
	uint32_t ttt;

	keys_data = NULL;
	keys_len = 0;
	ttt = 0xffffffff;
	for (;;) {
		struct pdu *request, *response;
		struct iscsi_bhs_text_response *bhstr;

		response = text_receive_response(conn);
		bhstr = (struct iscsi_bhs_text_response *)response->pdu_bhs;
		if (keys_data == NULL) {
			ttt = bhstr->bhstr_target_transfer_tag;
			keys_data = response->pdu_data;
			keys_len = response->pdu_data_len;
			response->pdu_data = NULL;
		} else {
			keys_data = realloc(keys_data,
			    keys_len + response->pdu_data_len);
			if (keys_data == NULL)
				log_err(1, "failed to grow keys block");
			memcpy(keys_data + keys_len, response->pdu_data,
			    response->pdu_data_len);
			keys_len += response->pdu_data_len;
		}
		if ((bhstr->bhstr_flags & BHSTR_FLAGS_FINAL) != 0) {
			pdu_delete(response);
			break;
		}
		if (bhstr->bhstr_target_transfer_tag != ttt)
			log_errx(1, "received non-final TextRequest PDU with "
			    "invalid TTT 0x%x",
			    bhstr->bhstr_target_transfer_tag);
		pdu_delete(response);

		/* Send an empty request. */
		request = text_new_request(conn, ttt);
		pdu_send(request);
		pdu_delete(request);
	}

	response_keys = keys_new();
	keys_load(response_keys, keys_data, keys_len);
	free(keys_data);
	return (response_keys);
}

/*
 * Read a list of keys from the initiator in a TextRequest PDU.
 */
struct keys *
text_read_request(struct connection *conn, struct pdu **requestp)
{
	struct iscsi_bhs_text_request *bhstr;
	struct pdu *request;
	struct keys *request_keys;

	request = text_receive_request(conn);
	bhstr = (struct iscsi_bhs_text_request *)request->pdu_bhs;
	if (bhstr->bhstr_target_transfer_tag != 0xffffffff)
		log_errx(1, "received TextRequest PDU with invalid TTT 0x%x",
		    bhstr->bhstr_target_transfer_tag);
	if (ntohl(bhstr->bhstr_expstatsn) != conn->conn_statsn) {
		log_errx(1, "received TextRequest PDU with wrong ExpStatSN: "
		    "is %u, should be %u", ntohl(bhstr->bhstr_expstatsn),
		    conn->conn_statsn);
	}

	request_keys = keys_new();
	keys_load_pdu(request_keys, request);
	*requestp = request;
	return (request_keys);
}

/*
 * Send a response back to the initiator as a series of TextResponse
 * PDUs.
 */
void
text_send_response(struct pdu *request, struct keys *response_keys)
{
	struct connection *conn = request->pdu_connection;
	char *keys_data;
	size_t keys_len;
	size_t keys_offset;
	uint32_t ttt;

	keys_save(response_keys, &keys_data, &keys_len);
	keys_offset = 0;
	ttt = keys_len;
	for (;;) {
		struct pdu *request2, *response;
		struct iscsi_bhs_text_request *bhstr;
		size_t todo;
		bool final;

		todo = keys_len - keys_offset;
		if (todo > (size_t)conn->conn_max_send_data_segment_length) {
			final = false;
			todo = conn->conn_max_send_data_segment_length;
		} else {
			final = true;
			ttt = 0xffffffff;
		}

		response = text_new_response(request, ttt, final);
		response->pdu_data = keys_data + keys_offset;
		response->pdu_data_len = todo;
		keys_offset += todo;

		pdu_send(response);
		response->pdu_data = NULL;
		pdu_delete(response);

		if (final)
			break;

		/*
		 * Wait for an empty request.
		 *
		 * XXX: Linux's Open-iSCSI initiator doesn't update
		 * ExpStatSN when receiving a TextResponse PDU.
		 */
		request2 = text_receive_request(conn);
		bhstr = (struct iscsi_bhs_text_request *)request2->pdu_bhs;
		if ((bhstr->bhstr_flags & BHSTR_FLAGS_FINAL) == 0)
			log_errx(1, "received continuation TextRequest PDU "
			    "without F set");
		if (pdu_data_segment_length(request2) != 0)
			log_errx(1, "received non-empty continuation "
			    "TextRequest PDU");
		if (bhstr->bhstr_target_transfer_tag != ttt)
			log_errx(1, "received TextRequest PDU with invalid "
			    "TTT 0x%x", bhstr->bhstr_target_transfer_tag);
		pdu_delete(request2);
	}
	free(keys_data);
}
