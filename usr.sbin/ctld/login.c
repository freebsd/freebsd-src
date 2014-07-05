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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/md5.h>
#include <openssl/rand.h>

#include "ctld.h"
#include "iscsi_proto.h"

static void login_send_error(struct pdu *request,
    char class, char detail);

static void
login_set_nsg(struct pdu *response, int nsg)
{
	struct iscsi_bhs_login_response *bhslr;

	assert(nsg == BHSLR_STAGE_SECURITY_NEGOTIATION ||
	    nsg == BHSLR_STAGE_OPERATIONAL_NEGOTIATION ||
	    nsg == BHSLR_STAGE_FULL_FEATURE_PHASE);

	bhslr = (struct iscsi_bhs_login_response *)response->pdu_bhs;

	bhslr->bhslr_flags &= 0xFC;
	bhslr->bhslr_flags |= nsg;
}

static int
login_csg(const struct pdu *request)
{
	struct iscsi_bhs_login_request *bhslr;

	bhslr = (struct iscsi_bhs_login_request *)request->pdu_bhs;

	return ((bhslr->bhslr_flags & 0x0C) >> 2);
}

static void
login_set_csg(struct pdu *response, int csg)
{
	struct iscsi_bhs_login_response *bhslr;

	assert(csg == BHSLR_STAGE_SECURITY_NEGOTIATION ||
	    csg == BHSLR_STAGE_OPERATIONAL_NEGOTIATION ||
	    csg == BHSLR_STAGE_FULL_FEATURE_PHASE);

	bhslr = (struct iscsi_bhs_login_response *)response->pdu_bhs;

	bhslr->bhslr_flags &= 0xF3;
	bhslr->bhslr_flags |= csg << 2;
}

static struct pdu *
login_receive(struct connection *conn, bool initial)
{
	struct pdu *request;
	struct iscsi_bhs_login_request *bhslr;

	request = pdu_new(conn);
	pdu_receive(request);
	if ((request->pdu_bhs->bhs_opcode & ~ISCSI_BHS_OPCODE_IMMEDIATE) !=
	    ISCSI_BHS_OPCODE_LOGIN_REQUEST) {
		/*
		 * The first PDU in session is special - if we receive any PDU
		 * different than login request, we have to drop the connection
		 * without sending response ("A target receiving any PDU
		 * except a Login request before the Login Phase is started MUST
		 * immediately terminate the connection on which the PDU
		 * was received.")
		 */
		if (initial == false)
			login_send_error(request, 0x02, 0x0b);
		log_errx(1, "protocol error: received invalid opcode 0x%x",
		    request->pdu_bhs->bhs_opcode);
	}
	bhslr = (struct iscsi_bhs_login_request *)request->pdu_bhs;
	/*
	 * XXX: Implement the C flag some day.
	 */
	if ((bhslr->bhslr_flags & BHSLR_FLAGS_CONTINUE) != 0) {
		login_send_error(request, 0x03, 0x00);
		log_errx(1, "received Login PDU with unsupported \"C\" flag");
	}
	if (bhslr->bhslr_version_max != 0x00) {
		login_send_error(request, 0x02, 0x05);
		log_errx(1, "received Login PDU with unsupported "
		    "Version-max 0x%x", bhslr->bhslr_version_max);
	}
	if (bhslr->bhslr_version_min != 0x00) {
		login_send_error(request, 0x02, 0x05);
		log_errx(1, "received Login PDU with unsupported "
		    "Version-min 0x%x", bhslr->bhslr_version_min);
	}
	if (ntohl(bhslr->bhslr_cmdsn) < conn->conn_cmdsn) {
		login_send_error(request, 0x02, 0x05);
		log_errx(1, "received Login PDU with decreasing CmdSN: "
		    "was %d, is %d", conn->conn_cmdsn,
		    ntohl(bhslr->bhslr_cmdsn));
	}
	if (initial == false &&
	    ntohl(bhslr->bhslr_expstatsn) != conn->conn_statsn) {
		login_send_error(request, 0x02, 0x05);
		log_errx(1, "received Login PDU with wrong ExpStatSN: "
		    "is %d, should be %d", ntohl(bhslr->bhslr_expstatsn),
		    conn->conn_statsn);
	}
	conn->conn_cmdsn = ntohl(bhslr->bhslr_cmdsn);

	return (request);
}

static struct pdu *
login_new_response(struct pdu *request)
{
	struct pdu *response;
	struct connection *conn;
	struct iscsi_bhs_login_request *bhslr;
	struct iscsi_bhs_login_response *bhslr2;

	bhslr = (struct iscsi_bhs_login_request *)request->pdu_bhs;
	conn = request->pdu_connection;

	response = pdu_new_response(request);
	bhslr2 = (struct iscsi_bhs_login_response *)response->pdu_bhs;
	bhslr2->bhslr_opcode = ISCSI_BHS_OPCODE_LOGIN_RESPONSE;
	login_set_csg(response, BHSLR_STAGE_SECURITY_NEGOTIATION);
	memcpy(bhslr2->bhslr_isid,
	    bhslr->bhslr_isid, sizeof(bhslr2->bhslr_isid));
	bhslr2->bhslr_initiator_task_tag = bhslr->bhslr_initiator_task_tag;
	bhslr2->bhslr_statsn = htonl(conn->conn_statsn++);
	bhslr2->bhslr_expcmdsn = htonl(conn->conn_cmdsn);
	bhslr2->bhslr_maxcmdsn = htonl(conn->conn_cmdsn);

	return (response);
}

static void
login_send_error(struct pdu *request, char class, char detail)
{
	struct pdu *response;
	struct iscsi_bhs_login_response *bhslr2;

	log_debugx("sending Login Response PDU with failure class 0x%x/0x%x; "
	    "see next line for reason", class, detail);
	response = login_new_response(request);
	bhslr2 = (struct iscsi_bhs_login_response *)response->pdu_bhs;
	bhslr2->bhslr_status_class = class;
	bhslr2->bhslr_status_detail = detail;

	pdu_send(response);
	pdu_delete(response);
}

static int
login_list_contains(const char *list, const char *what)
{
	char *tofree, *str, *token;

	tofree = str = checked_strdup(list);

	while ((token = strsep(&str, ",")) != NULL) {
		if (strcmp(token, what) == 0) {
			free(tofree);
			return (1);
		}
	}
	free(tofree);
	return (0);
}

static int
login_list_prefers(const char *list,
    const char *choice1, const char *choice2)
{
	char *tofree, *str, *token;

	tofree = str = checked_strdup(list);

	while ((token = strsep(&str, ",")) != NULL) {
		if (strcmp(token, choice1) == 0) {
			free(tofree);
			return (1);
		}
		if (strcmp(token, choice2) == 0) {
			free(tofree);
			return (2);
		}
	}
	free(tofree);
	return (-1);
}

static int
login_hex2int(const char hex)
{
	switch (hex) {
	case '0':
		return (0x00);
	case '1':
		return (0x01);
	case '2':
		return (0x02);
	case '3':
		return (0x03);
	case '4':
		return (0x04);
	case '5':
		return (0x05);
	case '6':
		return (0x06);
	case '7':
		return (0x07);
	case '8':
		return (0x08);
	case '9':
		return (0x09);
	case 'a':
	case 'A':
		return (0x0a);
	case 'b':
	case 'B':
		return (0x0b);
	case 'c':
	case 'C':
		return (0x0c);
	case 'd':
	case 'D':
		return (0x0d);
	case 'e':
	case 'E':
		return (0x0e);
	case 'f':
	case 'F':
		return (0x0f);
	default:
		return (-1);
	}
}

/*
 * XXX: Review this _carefully_.
 */
static int
login_hex2bin(const char *hex, char **binp, size_t *bin_lenp)
{
	int i, hex_len, nibble;
	bool lo = true; /* As opposed to 'hi'. */
	char *bin;
	size_t bin_off, bin_len;

	if (strncasecmp(hex, "0x", strlen("0x")) != 0) {
		log_warnx("malformed variable, should start with \"0x\"");
		return (-1);
	}

	hex += strlen("0x");
	hex_len = strlen(hex);
	if (hex_len < 1) {
		log_warnx("malformed variable; doesn't contain anything "
		    "but \"0x\"");
		return (-1);
	}

	bin_len = hex_len / 2 + hex_len % 2;
	bin = calloc(bin_len, 1);
	if (bin == NULL)
		log_err(1, "calloc");

	bin_off = bin_len - 1;
	for (i = hex_len - 1; i >= 0; i--) {
		nibble = login_hex2int(hex[i]);
		if (nibble < 0) {
			log_warnx("malformed variable, invalid char \"%c\"",
			    hex[i]);
			return (-1);
		}

		assert(bin_off < bin_len);
		if (lo) {
			bin[bin_off] = nibble;
			lo = false;
		} else {
			bin[bin_off] |= nibble << 4;
			bin_off--;
			lo = true;
		}
	}

	*binp = bin;
	*bin_lenp = bin_len;
	return (0);
}

static char *
login_bin2hex(const char *bin, size_t bin_len)
{
	unsigned char *hex, *tmp, ch;
	size_t hex_len;
	size_t i;

	hex_len = bin_len * 2 + 3; /* +2 for "0x", +1 for '\0'. */
	hex = malloc(hex_len);
	if (hex == NULL)
		log_err(1, "malloc");

	tmp = hex;
	tmp += sprintf(tmp, "0x");
	for (i = 0; i < bin_len; i++) {
		ch = bin[i];
		tmp += sprintf(tmp, "%02x", ch);
	}

	return (hex);
}

static void
login_compute_md5(const char id, const char *secret,
    const void *challenge, size_t challenge_len, void *response,
    size_t response_len)
{
	MD5_CTX ctx;
	int rv;

	assert(response_len == MD5_DIGEST_LENGTH);

	MD5_Init(&ctx);
	MD5_Update(&ctx, &id, sizeof(id));
	MD5_Update(&ctx, secret, strlen(secret));
	MD5_Update(&ctx, challenge, challenge_len);
	rv = MD5_Final(response, &ctx);
	if (rv != 1)
		log_errx(1, "MD5_Final");
}

#define	LOGIN_CHALLENGE_LEN	1024

static struct pdu *
login_receive_chap_a(struct connection *conn)
{
	struct pdu *request;
	struct keys *request_keys;
	const char *chap_a;

	request = login_receive(conn, false);
	request_keys = keys_new();
	keys_load(request_keys, request);

	chap_a = keys_find(request_keys, "CHAP_A");
	if (chap_a == NULL) {
		login_send_error(request, 0x02, 0x07);
		log_errx(1, "received CHAP Login PDU without CHAP_A");
	}
	if (login_list_contains(chap_a, "5") == 0) {
		login_send_error(request, 0x02, 0x01);
		log_errx(1, "received CHAP Login PDU with unsupported CHAP_A "
		    "\"%s\"", chap_a);
	}
	keys_delete(request_keys);

	return (request);
}

static void
login_send_chap_c(struct pdu *request, const unsigned char id,
    const void *challenge, const size_t challenge_len)
{
	struct pdu *response;
	struct keys *response_keys;
	char *chap_c, chap_i[4];

	chap_c = login_bin2hex(challenge, challenge_len);
	snprintf(chap_i, sizeof(chap_i), "%d", id);

	response = login_new_response(request);
	response_keys = keys_new();
	keys_add(response_keys, "CHAP_A", "5");
	keys_add(response_keys, "CHAP_I", chap_i);
	keys_add(response_keys, "CHAP_C", chap_c);
	free(chap_c);
	keys_save(response_keys, response);
	pdu_send(response);
	pdu_delete(response);
	keys_delete(response_keys);
}

static struct pdu *
login_receive_chap_r(struct connection *conn,
    struct auth_group *ag, const unsigned char id, const void *challenge,
    const size_t challenge_len, const struct auth **cap)
{
	struct pdu *request;
	struct keys *request_keys;
	const char *chap_n, *chap_r;
	char *response_bin, expected_response_bin[MD5_DIGEST_LENGTH];
	size_t response_bin_len;
	const struct auth *auth;
	int error;

	request = login_receive(conn, false);
	request_keys = keys_new();
	keys_load(request_keys, request);

	chap_n = keys_find(request_keys, "CHAP_N");
	if (chap_n == NULL) {
		login_send_error(request, 0x02, 0x07);
		log_errx(1, "received CHAP Login PDU without CHAP_N");
	}
	chap_r = keys_find(request_keys, "CHAP_R");
	if (chap_r == NULL) {
		login_send_error(request, 0x02, 0x07);
		log_errx(1, "received CHAP Login PDU without CHAP_R");
	}
	error = login_hex2bin(chap_r, &response_bin, &response_bin_len);
	if (error != 0) {
		login_send_error(request, 0x02, 0x07);
		log_errx(1, "received CHAP Login PDU with malformed CHAP_R");
	}

	/*
	 * Verify the response.
	 */
	assert(ag->ag_type == AG_TYPE_CHAP ||
	    ag->ag_type == AG_TYPE_CHAP_MUTUAL);
	auth = auth_find(ag, chap_n);
	if (auth == NULL) {
		login_send_error(request, 0x02, 0x01);
		log_errx(1, "received CHAP Login with invalid user \"%s\"",
		    chap_n);
	}

	assert(auth->a_secret != NULL);
	assert(strlen(auth->a_secret) > 0);
	login_compute_md5(id, auth->a_secret, challenge,
	    challenge_len, expected_response_bin,
	    sizeof(expected_response_bin));

	if (memcmp(response_bin, expected_response_bin,
	    sizeof(expected_response_bin)) != 0) {
		login_send_error(request, 0x02, 0x01);
		log_errx(1, "CHAP authentication failed for user \"%s\"",
		    auth->a_user);
	}

	keys_delete(request_keys);
	free(response_bin);

	*cap = auth;
	return (request);
}

static void
login_send_chap_success(struct pdu *request,
    const struct auth *auth)
{
	struct pdu *response;
	struct keys *request_keys, *response_keys;
	struct iscsi_bhs_login_response *bhslr2;
	const char *chap_i, *chap_c;
	char *chap_r, *challenge, response_bin[MD5_DIGEST_LENGTH];
	size_t challenge_len;
	unsigned char id;
	int error;

	response = login_new_response(request);
	bhslr2 = (struct iscsi_bhs_login_response *)response->pdu_bhs;
	bhslr2->bhslr_flags |= BHSLR_FLAGS_TRANSIT;
	login_set_nsg(response, BHSLR_STAGE_OPERATIONAL_NEGOTIATION);

	/*
	 * Actually, one more thing: mutual authentication.
	 */
	request_keys = keys_new();
	keys_load(request_keys, request);
	chap_i = keys_find(request_keys, "CHAP_I");
	chap_c = keys_find(request_keys, "CHAP_C");
	if (chap_i != NULL || chap_c != NULL) {
		if (chap_i == NULL) {
			login_send_error(request, 0x02, 0x07);
			log_errx(1, "initiator requested target "
			    "authentication, but didn't send CHAP_I");
		}
		if (chap_c == NULL) {
			login_send_error(request, 0x02, 0x07);
			log_errx(1, "initiator requested target "
			    "authentication, but didn't send CHAP_C");
		}
		if (auth->a_auth_group->ag_type != AG_TYPE_CHAP_MUTUAL) {
			login_send_error(request, 0x02, 0x01);
			log_errx(1, "initiator requests target authentication "
			    "for user \"%s\", but mutual user/secret "
			    "is not set", auth->a_user);
		}

		id = strtoul(chap_i, NULL, 10);
		error = login_hex2bin(chap_c, &challenge, &challenge_len);
		if (error != 0) {
			login_send_error(request, 0x02, 0x07);
			log_errx(1, "received CHAP Login PDU with malformed "
			    "CHAP_C");
		}

		log_debugx("performing mutual authentication as user \"%s\"",
		    auth->a_mutual_user);
		login_compute_md5(id, auth->a_mutual_secret, challenge,
		    challenge_len, response_bin, sizeof(response_bin));

		chap_r = login_bin2hex(response_bin,
		    sizeof(response_bin));
		response_keys = keys_new();
		keys_add(response_keys, "CHAP_N", auth->a_mutual_user);
		keys_add(response_keys, "CHAP_R", chap_r);
		free(chap_r);
		keys_save(response_keys, response);
		keys_delete(response_keys);
	} else {
		log_debugx("initiator did not request target authentication");
	}

	keys_delete(request_keys);
	pdu_send(response);
	pdu_delete(response);
}

static void
login_chap(struct connection *conn, struct auth_group *ag)
{
	const struct auth *auth;
	struct pdu *request;
	char challenge_bin[LOGIN_CHALLENGE_LEN];
	unsigned char id;
	int rv;

	/*
	 * Receive CHAP_A PDU.
	 */
	log_debugx("beginning CHAP authentication; waiting for CHAP_A");
	request = login_receive_chap_a(conn);

	/*
	 * Generate the challenge.
	 */
	rv = RAND_bytes(challenge_bin, sizeof(challenge_bin));
	if (rv != 1) {
		login_send_error(request, 0x03, 0x02);
		log_errx(1, "RAND_bytes failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
	}
	rv = RAND_bytes(&id, sizeof(id));
	if (rv != 1) {
		login_send_error(request, 0x03, 0x02);
		log_errx(1, "RAND_bytes failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
	}

	/*
	 * Send the challenge.
	 */
	log_debugx("sending CHAP_C, binary challenge size is %zd bytes",
	    sizeof(challenge_bin));
	login_send_chap_c(request, id, challenge_bin,
	    sizeof(challenge_bin));
	pdu_delete(request);

	/*
	 * Receive CHAP_N/CHAP_R PDU and authenticate.
	 */
	log_debugx("waiting for CHAP_N/CHAP_R");
	request = login_receive_chap_r(conn, ag, id, challenge_bin,
	    sizeof(challenge_bin), &auth);

	/*
	 * Yay, authentication succeeded!
	 */
	log_debugx("authentication succeeded for user \"%s\"; "
	    "transitioning to Negotiation Phase", auth->a_user);
	login_send_chap_success(request, auth);
	pdu_delete(request);
}

static void
login_negotiate_key(struct pdu *request, const char *name,
    const char *value, bool skipped_security, struct keys *response_keys)
{
	int which, tmp;
	struct connection *conn;

	conn = request->pdu_connection;

	if (strcmp(name, "InitiatorName") == 0) {
		if (!skipped_security)
			log_errx(1, "initiator resent InitiatorName");
	} else if (strcmp(name, "SessionType") == 0) {
		if (!skipped_security)
			log_errx(1, "initiator resent SessionType");
	} else if (strcmp(name, "TargetName") == 0) {
		if (!skipped_security)
			log_errx(1, "initiator resent TargetName");
	} else if (strcmp(name, "InitiatorAlias") == 0) {
		if (conn->conn_initiator_alias != NULL)
			free(conn->conn_initiator_alias);
		conn->conn_initiator_alias = checked_strdup(value);
	} else if (strcmp(value, "Irrelevant") == 0) {
		/* Ignore. */
	} else if (strcmp(name, "HeaderDigest") == 0) {
		/*
		 * We don't handle digests for discovery sessions.
		 */
		if (conn->conn_session_type == CONN_SESSION_TYPE_DISCOVERY) {
			log_debugx("discovery session; digests disabled");
			keys_add(response_keys, name, "None");
			return;
		}

		which = login_list_prefers(value, "CRC32C", "None");
		switch (which) {
		case 1:
			log_debugx("initiator prefers CRC32C "
			    "for header digest; we'll use it");
			conn->conn_header_digest = CONN_DIGEST_CRC32C;
			keys_add(response_keys, name, "CRC32C");
			break;
		case 2:
			log_debugx("initiator prefers not to do "
			    "header digest; we'll comply");
			keys_add(response_keys, name, "None");
			break;
		default:
			log_warnx("initiator sent unrecognized "
			    "HeaderDigest value \"%s\"; will use None", value);
			keys_add(response_keys, name, "None");
			break;
		}
	} else if (strcmp(name, "DataDigest") == 0) {
		if (conn->conn_session_type == CONN_SESSION_TYPE_DISCOVERY) {
			log_debugx("discovery session; digests disabled");
			keys_add(response_keys, name, "None");
			return;
		}

		which = login_list_prefers(value, "CRC32C", "None");
		switch (which) {
		case 1:
			log_debugx("initiator prefers CRC32C "
			    "for data digest; we'll use it");
			conn->conn_data_digest = CONN_DIGEST_CRC32C;
			keys_add(response_keys, name, "CRC32C");
			break;
		case 2:
			log_debugx("initiator prefers not to do "
			    "data digest; we'll comply");
			keys_add(response_keys, name, "None");
			break;
		default:
			log_warnx("initiator sent unrecognized "
			    "DataDigest value \"%s\"; will use None", value);
			keys_add(response_keys, name, "None");
			break;
		}
	} else if (strcmp(name, "MaxConnections") == 0) {
		keys_add(response_keys, name, "1");
	} else if (strcmp(name, "InitialR2T") == 0) {
		keys_add(response_keys, name, "Yes");
	} else if (strcmp(name, "ImmediateData") == 0) {
		if (conn->conn_session_type == CONN_SESSION_TYPE_DISCOVERY) {
			log_debugx("discovery session; ImmediateData irrelevant");
			keys_add(response_keys, name, "Irrelevant");
		} else {
			if (strcmp(value, "Yes") == 0) {
				conn->conn_immediate_data = true;
				keys_add(response_keys, name, "Yes");
			} else {
				conn->conn_immediate_data = false;
				keys_add(response_keys, name, "No");
			}
		}
	} else if (strcmp(name, "MaxRecvDataSegmentLength") == 0) {
		tmp = strtoul(value, NULL, 10);
		if (tmp <= 0) {
			login_send_error(request, 0x02, 0x00);
			log_errx(1, "received invalid "
			    "MaxRecvDataSegmentLength");
		}
		if (tmp > MAX_DATA_SEGMENT_LENGTH) {
			log_debugx("capping MaxDataSegmentLength from %d to %d",
			    tmp, MAX_DATA_SEGMENT_LENGTH);
			tmp = MAX_DATA_SEGMENT_LENGTH;
		}
		conn->conn_max_data_segment_length = tmp;
		keys_add_int(response_keys, name, tmp);
	} else if (strcmp(name, "MaxBurstLength") == 0) {
		tmp = strtoul(value, NULL, 10);
		if (tmp <= 0) {
			login_send_error(request, 0x02, 0x00);
			log_errx(1, "received invalid MaxBurstLength");
		}
		if (tmp > MAX_BURST_LENGTH) {
			log_debugx("capping MaxBurstLength from %d to %d",
			    tmp, MAX_BURST_LENGTH);
			tmp = MAX_BURST_LENGTH;
		}
		conn->conn_max_burst_length = tmp;
		keys_add(response_keys, name, value);
	} else if (strcmp(name, "FirstBurstLength") == 0) {
		tmp = strtoul(value, NULL, 10);
		if (tmp <= 0) {
			login_send_error(request, 0x02, 0x00);
			log_errx(1, "received invalid "
			    "FirstBurstLength");
		}
		if (tmp > MAX_DATA_SEGMENT_LENGTH) {
			log_debugx("capping FirstBurstLength from %d to %d",
			    tmp, MAX_DATA_SEGMENT_LENGTH);
			tmp = MAX_DATA_SEGMENT_LENGTH;
		}
		/*
		 * We don't pass the value to the kernel; it only enforces
		 * hardcoded limit anyway.
		 */
		keys_add_int(response_keys, name, tmp);
	} else if (strcmp(name, "DefaultTime2Wait") == 0) {
		keys_add(response_keys, name, value);
	} else if (strcmp(name, "DefaultTime2Retain") == 0) {
		keys_add(response_keys, name, "0");
	} else if (strcmp(name, "MaxOutstandingR2T") == 0) {
		keys_add(response_keys, name, "1");
	} else if (strcmp(name, "DataPDUInOrder") == 0) {
		keys_add(response_keys, name, "Yes");
	} else if (strcmp(name, "DataSequenceInOrder") == 0) {
		keys_add(response_keys, name, "Yes");
	} else if (strcmp(name, "ErrorRecoveryLevel") == 0) {
		keys_add(response_keys, name, "0");
	} else if (strcmp(name, "OFMarker") == 0) {
		keys_add(response_keys, name, "No");
	} else if (strcmp(name, "IFMarker") == 0) {
		keys_add(response_keys, name, "No");
	} else {
		log_debugx("unknown key \"%s\"; responding "
		    "with NotUnderstood", name);
		keys_add(response_keys, name, "NotUnderstood");
	}
}

static void
login_negotiate(struct connection *conn, struct pdu *request)
{
	struct pdu *response;
	struct iscsi_bhs_login_response *bhslr2;
	struct keys *request_keys, *response_keys;
	int i;
	bool skipped_security;

	if (request == NULL) {
		log_debugx("beginning operational parameter negotiation; "
		    "waiting for Login PDU");
		request = login_receive(conn, false);
		skipped_security = false;
	} else
		skipped_security = true;

	request_keys = keys_new();
	keys_load(request_keys, request);

	response = login_new_response(request);
	bhslr2 = (struct iscsi_bhs_login_response *)response->pdu_bhs;
	bhslr2->bhslr_flags |= BHSLR_FLAGS_TRANSIT;
	bhslr2->bhslr_tsih = htons(0xbadd);
	login_set_csg(response, BHSLR_STAGE_OPERATIONAL_NEGOTIATION);
	login_set_nsg(response, BHSLR_STAGE_FULL_FEATURE_PHASE);
	response_keys = keys_new();
	for (i = 0; i < KEYS_MAX; i++) {
		if (request_keys->keys_names[i] == NULL)
			break;

		login_negotiate_key(request, request_keys->keys_names[i],
		    request_keys->keys_values[i], skipped_security,
		    response_keys);
	}

	log_debugx("operational parameter negotiation done; "
	    "transitioning to Full Feature Phase");

	keys_save(response_keys, response);
	pdu_send(response);
	pdu_delete(response);
	keys_delete(response_keys);
	pdu_delete(request);
	keys_delete(request_keys);
}

void
login(struct connection *conn)
{
	struct pdu *request, *response;
	struct iscsi_bhs_login_request *bhslr;
	struct iscsi_bhs_login_response *bhslr2;
	struct keys *request_keys, *response_keys;
	struct auth_group *ag;
	const char *initiator_name, *initiator_alias, *session_type,
	    *target_name, *auth_method;
	char *portal_group_tag;
	int rv;

	/*
	 * Handle the initial Login Request - figure out required authentication
	 * method and either transition to the next phase, if no authentication
	 * is required, or call appropriate authentication code.
	 */
	log_debugx("beginning Login Phase; waiting for Login PDU");
	request = login_receive(conn, true);
	bhslr = (struct iscsi_bhs_login_request *)request->pdu_bhs;
	if (bhslr->bhslr_tsih != 0) {
		login_send_error(request, 0x02, 0x0a);
		log_errx(1, "received Login PDU with non-zero TSIH");
	}

	memcpy(conn->conn_initiator_isid, bhslr->bhslr_isid,
	    sizeof(conn->conn_initiator_isid));

	/*
	 * XXX: Implement the C flag some day.
	 */
	request_keys = keys_new();
	keys_load(request_keys, request);

	assert(conn->conn_initiator_name == NULL);
	initiator_name = keys_find(request_keys, "InitiatorName");
	if (initiator_name == NULL) {
		login_send_error(request, 0x02, 0x07);
		log_errx(1, "received Login PDU without InitiatorName");
	}
	if (valid_iscsi_name(initiator_name) == false) {
		login_send_error(request, 0x02, 0x00);
		log_errx(1, "received Login PDU with invalid InitiatorName");
	}
	conn->conn_initiator_name = checked_strdup(initiator_name);
	log_set_peer_name(conn->conn_initiator_name);
	/*
	 * XXX: This doesn't work (does nothing) because of Capsicum.
	 */
	setproctitle("%s (%s)", conn->conn_initiator_addr, conn->conn_initiator_name);

	initiator_alias = keys_find(request_keys, "InitiatorAlias");
	if (initiator_alias != NULL)
		conn->conn_initiator_alias = checked_strdup(initiator_alias);

	assert(conn->conn_session_type == CONN_SESSION_TYPE_NONE);
	session_type = keys_find(request_keys, "SessionType");
	if (session_type != NULL) {
		if (strcmp(session_type, "Normal") == 0) {
			conn->conn_session_type = CONN_SESSION_TYPE_NORMAL;
		} else if (strcmp(session_type, "Discovery") == 0) {
			conn->conn_session_type = CONN_SESSION_TYPE_DISCOVERY;
		} else {
			login_send_error(request, 0x02, 0x00);
			log_errx(1, "received Login PDU with invalid "
			    "SessionType \"%s\"", session_type);
		}
	} else
		conn->conn_session_type = CONN_SESSION_TYPE_NORMAL;

	assert(conn->conn_target == NULL);
	if (conn->conn_session_type == CONN_SESSION_TYPE_NORMAL) {
		target_name = keys_find(request_keys, "TargetName");
		if (target_name == NULL) {
			login_send_error(request, 0x02, 0x07);
			log_errx(1, "received Login PDU without TargetName");
		}

		conn->conn_target =
		    target_find(conn->conn_portal->p_portal_group->pg_conf,
		    target_name);
		if (conn->conn_target == NULL) {
			login_send_error(request, 0x02, 0x03);
			log_errx(1, "requested target \"%s\" not found",
			    target_name);
		}
	}

	/*
	 * At this point we know what kind of authentication we need.
	 */
	if (conn->conn_session_type == CONN_SESSION_TYPE_NORMAL) {
		ag = conn->conn_target->t_auth_group;
		if (ag->ag_name != NULL) {
			log_debugx("initiator requests to connect "
			    "to target \"%s\"; auth-group \"%s\"",
			    conn->conn_target->t_name,
			    conn->conn_target->t_auth_group->ag_name);
		} else {
			log_debugx("initiator requests to connect "
			    "to target \"%s\"", conn->conn_target->t_name);
		}
	} else {
		assert(conn->conn_session_type == CONN_SESSION_TYPE_DISCOVERY);
		ag = conn->conn_portal->p_portal_group->pg_discovery_auth_group;
		if (ag->ag_name != NULL) {
			log_debugx("initiator requests "
			    "discovery session; auth-group \"%s\"", ag->ag_name);
		} else {
			log_debugx("initiator requests discovery session");
		}
	}

	/*
	 * Enforce initiator-name and initiator-portal.
	 */
	if (auth_name_defined(ag)) {
		if (auth_name_find(ag, initiator_name) == NULL) {
			login_send_error(request, 0x02, 0x02);
			log_errx(1, "initiator does not match allowed "
			    "initiator names");
		}
		log_debugx("initiator matches allowed initiator names");
	} else {
		log_debugx("auth-group does not define initiator name "
		    "restrictions");
	}

	if (auth_portal_defined(ag)) {
		if (auth_portal_find(ag, conn->conn_initiator_addr) == NULL) {
			login_send_error(request, 0x02, 0x02);
			log_errx(1, "initiator does not match allowed "
			    "initiator portals");
		}
		log_debugx("initiator matches allowed initiator portals");
	} else {
		log_debugx("auth-group does not define initiator portal "
		    "restrictions");
	}

	/*
	 * Let's see if the initiator intends to do any kind of authentication
	 * at all.
	 */
	if (login_csg(request) == BHSLR_STAGE_OPERATIONAL_NEGOTIATION) {
		if (ag->ag_type != AG_TYPE_NO_AUTHENTICATION) {
			login_send_error(request, 0x02, 0x01);
			log_errx(1, "initiator skipped the authentication, "
			    "but authentication is required");
		}

		keys_delete(request_keys);

		log_debugx("initiator skipped the authentication, "
		    "and we don't need it; proceeding with negotiation");
		login_negotiate(conn, request);
		return;
	}

	if (ag->ag_type == AG_TYPE_NO_AUTHENTICATION) {
		/*
		 * Initiator might want to to authenticate,
		 * but we don't need it.
		 */
		log_debugx("authentication not required; "
		    "transitioning to operational parameter negotiation");

		if ((bhslr->bhslr_flags & BHSLR_FLAGS_TRANSIT) == 0)
			log_warnx("initiator did not set the \"T\" flag; "
			    "transitioning anyway");

		response = login_new_response(request);
		bhslr2 = (struct iscsi_bhs_login_response *)response->pdu_bhs;
		bhslr2->bhslr_flags |= BHSLR_FLAGS_TRANSIT;
		login_set_nsg(response,
		    BHSLR_STAGE_OPERATIONAL_NEGOTIATION);
		response_keys = keys_new();
		/*
		 * Required by Linux initiator.
		 */
		auth_method = keys_find(request_keys, "AuthMethod");
		if (auth_method != NULL &&
		    login_list_contains(auth_method, "None"))
			keys_add(response_keys, "AuthMethod", "None");

		if (conn->conn_session_type == CONN_SESSION_TYPE_NORMAL) {
			if (conn->conn_target->t_alias != NULL)
				keys_add(response_keys,
				    "TargetAlias", conn->conn_target->t_alias);
			rv = asprintf(&portal_group_tag, "%d",
			    conn->conn_portal->p_portal_group->pg_tag);
			if (rv <= 0)
				log_err(1, "asprintf");
			keys_add(response_keys,
			    "TargetPortalGroupTag", portal_group_tag);
			free(portal_group_tag);
		}
		keys_save(response_keys, response);
		pdu_send(response);
		pdu_delete(response);
		keys_delete(response_keys);
		pdu_delete(request);
		keys_delete(request_keys);

		login_negotiate(conn, NULL);
		return;
	}

	if (ag->ag_type == AG_TYPE_DENY) {
		login_send_error(request, 0x02, 0x01);
		log_errx(1, "auth-type is \"deny\"");
	}

	if (ag->ag_type == AG_TYPE_UNKNOWN) {
		/*
		 * This can happen with empty auth-group.
		 */
		login_send_error(request, 0x02, 0x01);
		log_errx(1, "auth-type not set, denying access");
	}

	log_debugx("CHAP authentication required");

	auth_method = keys_find(request_keys, "AuthMethod");
	if (auth_method == NULL) {
		login_send_error(request, 0x02, 0x07);
		log_errx(1, "received Login PDU without AuthMethod");
	}
	/*
	 * XXX: This should be Reject, not just a login failure (5.3.2).
	 */
	if (login_list_contains(auth_method, "CHAP") == 0) {
		login_send_error(request, 0x02, 0x01);
		log_errx(1, "initiator requests unsupported AuthMethod \"%s\" "
		    "instead of \"CHAP\"", auth_method);
	}

	response = login_new_response(request);

	response_keys = keys_new();
	keys_add(response_keys, "AuthMethod", "CHAP");
	if (conn->conn_session_type == CONN_SESSION_TYPE_NORMAL) {
		rv = asprintf(&portal_group_tag, "%d",
		    conn->conn_portal->p_portal_group->pg_tag);
		if (rv <= 0)
			log_err(1, "asprintf");
		keys_add(response_keys,
		    "TargetPortalGroupTag", portal_group_tag);
		free(portal_group_tag);
		if (conn->conn_target->t_alias != NULL)
			keys_add(response_keys,
			    "TargetAlias", conn->conn_target->t_alias);
	}
	keys_save(response_keys, response);

	pdu_send(response);
	pdu_delete(response);
	keys_delete(response_keys);
	pdu_delete(request);
	keys_delete(request_keys);

	login_chap(conn, ag);

	login_negotiate(conn, NULL);
}
