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
 *
 */

#include <sys/cdefs.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>

#include "ctld.hh"
#include "iscsi.hh"
#include "iscsi_proto.h"

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
	if (ISCSI_SNLT(ntohl(bhslr->bhslr_cmdsn), conn->conn_cmdsn)) {
		log_errx(1, "received Logout PDU with decreasing CmdSN: "
		    "was %u, is %u", conn->conn_cmdsn,
		    ntohl(bhslr->bhslr_cmdsn));
	}
	if (ntohl(bhslr->bhslr_expstatsn) != conn->conn_statsn) {
		log_errx(1, "received Logout PDU with wrong ExpStatSN: "
		    "is %u, should be %u", ntohl(bhslr->bhslr_expstatsn),
		    conn->conn_statsn);
	}
	conn->conn_cmdsn = ntohl(bhslr->bhslr_cmdsn);
	if ((bhslr->bhslr_opcode & ISCSI_BHS_OPCODE_IMMEDIATE) == 0)
		conn->conn_cmdsn++;

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

static void
discovery_add_target(struct keys *response_keys, const struct target *targ)
{
	char *buf;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	const struct addrinfo *ai;
	int ret;

	keys_add(response_keys, "TargetName", targ->name());
	for (const port *port : targ->ports()) {
	    const struct portal_group *pg = port->portal_group();
	    if (pg == nullptr)
		continue;
	    for (const portal_up &portal : pg->portals()) {
		if (portal->protocol() != portal_protocol::ISCSI &&
		    portal->protocol() != portal_protocol::ISER)
			continue;
		ai = portal->ai();
		ret = getnameinfo(ai->ai_addr, ai->ai_addrlen,
		    hbuf, sizeof(hbuf), sbuf, sizeof(sbuf),
		    NI_NUMERICHOST | NI_NUMERICSERV);
		if (ret != 0) {
			log_warnx("getnameinfo: %s", gai_strerror(ret));
			continue;
		}
		switch (ai->ai_addr->sa_family) {
		case AF_INET:
			if (strcmp(hbuf, "0.0.0.0") == 0)
				continue;
			ret = asprintf(&buf, "%s:%s,%d", hbuf, sbuf,
			    pg->tag());
			break;
		case AF_INET6:
			if (strcmp(hbuf, "::") == 0)
				continue;
			ret = asprintf(&buf, "[%s]:%s,%d", hbuf, sbuf,
			    pg->tag());
			break;
		default:
			continue;
		}
		if (ret <= 0)
		    log_err(1, "asprintf");
		keys_add(response_keys, "TargetAddress", buf);
		free(buf);
	    }
	}
}

bool
iscsi_connection::discovery_target_filtered_out(const struct port *port) const
{
	const struct auth_group *ag;
	const struct portal_group *pg;
	const struct target *targ;
	const struct auth *auth;
	int error;

	targ = port->target();
	ag = port->auth_group();
	if (ag == nullptr)
		ag = targ->auth_group();
	pg = conn_portal->portal_group();

	assert(pg->discovery_filter() != discovery_filter::UNKNOWN);

	if (pg->discovery_filter() >= discovery_filter::PORTAL &&
	    !ag->initiator_permitted(conn_initiator_sa)) {
		log_debugx("initiator does not match initiator portals "
		    "allowed for target \"%s\"; skipping", targ->name());
		return (true);
	}

	if (pg->discovery_filter() >= discovery_filter::PORTAL_NAME &&
	    !ag->initiator_permitted(conn_initiator_name)) {
		log_debugx("initiator does not match initiator names "
		    "allowed for target \"%s\"; skipping", targ->name());
		return (true);
	}

	if (pg->discovery_filter() >= discovery_filter::PORTAL_NAME_AUTH &&
	    ag->type() != auth_type::NO_AUTHENTICATION) {
		if (conn_chap == nullptr) {
			assert(pg->discovery_auth_group()->type() ==
			    auth_type::NO_AUTHENTICATION);

			log_debugx("initiator didn't authenticate, but target "
			    "\"%s\" requires CHAP; skipping", targ->name());
			return (true);
		}

		assert(!conn_user.empty());
		auth = ag->find_auth(conn_user);
		if (auth == NULL) {
			log_debugx("CHAP user \"%s\" doesn't match target "
			    "\"%s\"; skipping", conn_user.c_str(),
			    targ->name());
			return (true);
		}

		error = chap_authenticate(conn_chap, auth->secret());
		if (error != 0) {
			log_debugx("password for CHAP user \"%s\" doesn't "
			    "match target \"%s\"; skipping",
			    conn_user.c_str(), targ->name());
			return (true);
		}
	}

	return (false);
}

void
iscsi_connection::discovery()
{
	struct pdu *request, *response;
	struct keys *request_keys, *response_keys;
	const struct port *port;
	const struct portal_group *pg;
	const char *send_targets;

	pg = conn_portal->portal_group();

	log_debugx("beginning discovery session; waiting for TextRequest PDU");
	request_keys = text_read_request(&conn, &request);

	send_targets = keys_find(request_keys, "SendTargets");
	if (send_targets == NULL)
		log_errx(1, "received TextRequest PDU without SendTargets");

	response_keys = keys_new();

	if (strcmp(send_targets, "All") == 0) {
		for (const auto &kv : pg->ports()) {
			port = kv.second;
			if (discovery_target_filtered_out(port)) {
				/* Ignore this target. */
				continue;
			}
			discovery_add_target(response_keys, port->target());
		}
	} else {
		port = pg->find_port(send_targets);
		if (port == NULL) {
			log_debugx("initiator requested information on unknown "
			    "target \"%s\"; returning nothing", send_targets);
		} else {
			if (discovery_target_filtered_out(port)) {
				/* Ignore this target. */
			} else {
				discovery_add_target(response_keys,
				    port->target());
			}
		}
	}

	text_send_response(request, response_keys);
	keys_delete(response_keys);
	pdu_delete(request);
	keys_delete(request_keys);

	log_debugx("done sending targets; waiting for Logout PDU");
	request = logout_receive(&conn);
	response = logout_new_response(request);

	pdu_send(response);
	pdu_delete(response);
	pdu_delete(request);

	log_debugx("discovery session done");
}
