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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/md5.h>
#include <openssl/rand.h>

#include "iscsid.h"
#include "iscsi_proto.h"

static int
login_nsg(const struct pdu *response)
{
	struct iscsi_bhs_login_response *bhslr;

	bhslr = (struct iscsi_bhs_login_response *)response->pdu_bhs;

	return (bhslr->bhslr_flags & 0x03);
}

static void
login_set_nsg(struct pdu *request, int nsg)
{
	struct iscsi_bhs_login_request *bhslr;

	assert(nsg == BHSLR_STAGE_SECURITY_NEGOTIATION ||
	    nsg == BHSLR_STAGE_OPERATIONAL_NEGOTIATION ||
	    nsg == BHSLR_STAGE_FULL_FEATURE_PHASE);

	bhslr = (struct iscsi_bhs_login_request *)request->pdu_bhs;

	bhslr->bhslr_flags &= 0xFC;
	bhslr->bhslr_flags |= nsg;
}

static void
login_set_csg(struct pdu *request, int csg)
{
	struct iscsi_bhs_login_request *bhslr;

	assert(csg == BHSLR_STAGE_SECURITY_NEGOTIATION ||
	    csg == BHSLR_STAGE_OPERATIONAL_NEGOTIATION ||
	    csg == BHSLR_STAGE_FULL_FEATURE_PHASE);

	bhslr = (struct iscsi_bhs_login_request *)request->pdu_bhs;

	bhslr->bhslr_flags &= 0xF3;
	bhslr->bhslr_flags |= csg << 2;
}

static const char *
login_target_error_str(int class, int detail)
{
	static char msg[128];

	/*
	 * RFC 3270, 10.13.5.  Status-Class and Status-Detail
	 */
	switch (class) {
	case 0x01:
		switch (detail) {
		case 0x01:
			return ("Target moved temporarily");
		case 0x02:
			return ("Target moved permanently");
		default:
			snprintf(msg, sizeof(msg), "unknown redirection; "
			    "Status-Class 0x%x, Status-Detail 0x%x",
			    class, detail);
			return (msg);
		}
	case 0x02:
		switch (detail) {
		case 0x00:
			return ("Initiator error");
		case 0x01:
			return ("Authentication failure");
		case 0x02:
			return ("Authorization failure");
		case 0x03:
			return ("Not found");
		case 0x04:
			return ("Target removed");
		case 0x05:
			return ("Unsupported version");
		case 0x06:
			return ("Too many connections");
		case 0x07:
			return ("Missing parameter");
		case 0x08:
			return ("Can't include in session");
		case 0x09:
			return ("Session type not supported");
		case 0x0a:
			return ("Session does not exist");
		case 0x0b:
			return ("Invalid during login");
		default:
			snprintf(msg, sizeof(msg), "unknown initiator error; "
			    "Status-Class 0x%x, Status-Detail 0x%x",
			    class, detail);
			return (msg);
		}
	case 0x03:
		switch (detail) {
		case 0x00:
			return ("Target error");
		case 0x01:
			return ("Service unavailable");
		case 0x02:
			return ("Out of resources");
		default:
			snprintf(msg, sizeof(msg), "unknown target error; "
			    "Status-Class 0x%x, Status-Detail 0x%x",
			    class, detail);
			return (msg);
		}
	default:
		snprintf(msg, sizeof(msg), "unknown error; "
		    "Status-Class 0x%x, Status-Detail 0x%x",
		    class, detail);
		return (msg);
	}
}

static void
kernel_modify(const struct connection *conn, const char *target_address)
{
	struct iscsi_session_modify ism;
	int error;

	memset(&ism, 0, sizeof(ism));
	ism.ism_session_id = conn->conn_session_id;
	memcpy(&ism.ism_conf, &conn->conn_conf, sizeof(ism.ism_conf));
	strlcpy(ism.ism_conf.isc_target_addr, target_address,
	    sizeof(ism.ism_conf.isc_target));
	error = ioctl(conn->conn_iscsi_fd, ISCSISMODIFY, &ism);
	if (error != 0) {
		log_err(1, "failed to redirect to %s: ISCSISMODIFY",
		    target_address);
	}
}

/*
 * XXX:	The way it works is suboptimal; what should happen is described
 *	in draft-gilligan-iscsi-fault-tolerance-00.  That, however, would
 *	be much more complicated: we would need to keep "dependencies"
 *	for sessions, so that, in case described in draft and using draft
 *	terminology, we would have three sessions: one for discovery,
 *	one for initial target portal, and one for redirect portal.  
 *	This would allow us to "backtrack" on connection failure,
 *	as described in draft.
 */
static void
login_handle_redirection(struct connection *conn, struct pdu *response)
{
	struct iscsi_bhs_login_response *bhslr;
	struct keys *response_keys;
	const char *target_address;

	bhslr = (struct iscsi_bhs_login_response *)response->pdu_bhs;
	assert (bhslr->bhslr_status_class == 1);

	response_keys = keys_new();
	keys_load(response_keys, response);

	target_address = keys_find(response_keys, "TargetAddress");
	if (target_address == NULL)
		log_errx(1, "received redirection without TargetAddress");
	if (target_address[0] == '\0')
		log_errx(1, "received redirection with empty TargetAddress");
	if (strlen(target_address) >=
	    sizeof(conn->conn_conf.isc_target_addr) - 1)
		log_errx(1, "received TargetAddress is too long");

	log_debugx("received redirection to \"%s\"", target_address);
	kernel_modify(conn, target_address);
}

static struct pdu *
login_receive(struct connection *conn)
{
	struct pdu *response;
	struct iscsi_bhs_login_response *bhslr;
	const char *errorstr;
	static bool initial = true;

	response = pdu_new(conn);
	pdu_receive(response);
	if (response->pdu_bhs->bhs_opcode != ISCSI_BHS_OPCODE_LOGIN_RESPONSE) {
		log_errx(1, "protocol error: received invalid opcode 0x%x",
		    response->pdu_bhs->bhs_opcode);
	}
	bhslr = (struct iscsi_bhs_login_response *)response->pdu_bhs;
	/*
	 * XXX: Implement the C flag some day.
	 */
	if ((bhslr->bhslr_flags & BHSLR_FLAGS_CONTINUE) != 0)
		log_errx(1, "received Login PDU with unsupported \"C\" flag");
	if (bhslr->bhslr_version_max != 0x00)
		log_errx(1, "received Login PDU with unsupported "
		    "Version-max 0x%x", bhslr->bhslr_version_max);
	if (bhslr->bhslr_version_active != 0x00)
		log_errx(1, "received Login PDU with unsupported "
		    "Version-active 0x%x", bhslr->bhslr_version_active);
	if (bhslr->bhslr_status_class == 1) {
		login_handle_redirection(conn, response);
		log_debugx("redirection handled; exiting");
		exit(0);
	}
	if (bhslr->bhslr_status_class != 0) {
		errorstr = login_target_error_str(bhslr->bhslr_status_class,
		    bhslr->bhslr_status_detail);
		fail(conn, errorstr);
		log_errx(1, "target returned error: %s", errorstr);
	}
	if (initial == false &&
	    ntohl(bhslr->bhslr_statsn) != conn->conn_statsn + 1) {
		/*
		 * It's a warning, not an error, to work around what seems
		 * to be bug in NetBSD iSCSI target.
		 */
		log_warnx("received Login PDU with wrong StatSN: "
		    "is %d, should be %d", ntohl(bhslr->bhslr_statsn),
		    conn->conn_statsn + 1);
	}
	conn->conn_tsih = ntohs(bhslr->bhslr_tsih);
	conn->conn_statsn = ntohl(bhslr->bhslr_statsn);

	initial = false;

	return (response);
}

static struct pdu *
login_new_request(struct connection *conn, int csg)
{
	struct pdu *request;
	struct iscsi_bhs_login_request *bhslr;
	int nsg;

	request = pdu_new(conn);
	bhslr = (struct iscsi_bhs_login_request *)request->pdu_bhs;
	bhslr->bhslr_opcode = ISCSI_BHS_OPCODE_LOGIN_REQUEST |
	    ISCSI_BHS_OPCODE_IMMEDIATE;

	bhslr->bhslr_flags = BHSLR_FLAGS_TRANSIT;
	switch (csg) {
	case BHSLR_STAGE_SECURITY_NEGOTIATION:
		nsg = BHSLR_STAGE_OPERATIONAL_NEGOTIATION;
		break;
	case BHSLR_STAGE_OPERATIONAL_NEGOTIATION:
		nsg = BHSLR_STAGE_FULL_FEATURE_PHASE;
		break;
	default:
		assert(!"invalid csg");
		log_errx(1, "invalid csg %d", csg);
	}
	login_set_csg(request, csg);
	login_set_nsg(request, nsg);

	memcpy(bhslr->bhslr_isid, &conn->conn_isid, sizeof(bhslr->bhslr_isid));
	bhslr->bhslr_tsih = htons(conn->conn_tsih);
	bhslr->bhslr_initiator_task_tag = 0;
	bhslr->bhslr_cmdsn = 0;
	bhslr->bhslr_expstatsn = htonl(conn->conn_statsn + 1);

	return (request);
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

static void
login_negotiate_key(struct connection *conn, const char *name,
    const char *value)
{
	int which, tmp;

	if (strcmp(name, "TargetAlias") == 0) {
		strlcpy(conn->conn_target_alias, value,
		    sizeof(conn->conn_target_alias));
	} else if (strcmp(value, "Irrelevant") == 0) {
		/* Ignore. */
	} else if (strcmp(name, "HeaderDigest") == 0) {
		which = login_list_prefers(value, "CRC32C", "None");
		switch (which) {
		case 1:
			log_debugx("target prefers CRC32C "
			    "for header digest; we'll use it");
			conn->conn_header_digest = CONN_DIGEST_CRC32C;
			break;
		case 2:
			log_debugx("target prefers not to do "
			    "header digest; we'll comply");
			break;
		default:
			log_warnx("target sent unrecognized "
			    "HeaderDigest value \"%s\"; will use None", value);
			break;
		}
	} else if (strcmp(name, "DataDigest") == 0) {
		which = login_list_prefers(value, "CRC32C", "None");
		switch (which) {
		case 1:
			log_debugx("target prefers CRC32C "
			    "for data digest; we'll use it");
			conn->conn_data_digest = CONN_DIGEST_CRC32C;
			break;
		case 2:
			log_debugx("target prefers not to do "
			    "data digest; we'll comply");
			break;
		default:
			log_warnx("target sent unrecognized "
			    "DataDigest value \"%s\"; will use None", value);
			break;
		}
	} else if (strcmp(name, "MaxConnections") == 0) {
		/* Ignore. */
	} else if (strcmp(name, "InitialR2T") == 0) {
		if (strcmp(value, "Yes") == 0)
			conn->conn_initial_r2t = true;
		else
			conn->conn_initial_r2t = false;
	} else if (strcmp(name, "ImmediateData") == 0) {
		if (strcmp(value, "Yes") == 0)
			conn->conn_immediate_data = true;
		else
			conn->conn_immediate_data = false;
	} else if (strcmp(name, "MaxRecvDataSegmentLength") == 0) {
		tmp = strtoul(value, NULL, 10);
		if (tmp <= 0)
			log_errx(1, "received invalid "
			    "MaxRecvDataSegmentLength");
		conn->conn_max_data_segment_length = tmp;
	} else if (strcmp(name, "MaxBurstLength") == 0) {
		if (conn->conn_immediate_data) {
			tmp = strtoul(value, NULL, 10);
			if (tmp <= 0)
				log_errx(1, "received invalid MaxBurstLength");
			conn->conn_max_burst_length = tmp;
		}
	} else if (strcmp(name, "FirstBurstLength") == 0) {
		tmp = strtoul(value, NULL, 10);
		if (tmp <= 0)
			log_errx(1, "received invalid FirstBurstLength");
		conn->conn_first_burst_length = tmp;
	} else if (strcmp(name, "DefaultTime2Wait") == 0) {
		/* Ignore */
	} else if (strcmp(name, "DefaultTime2Retain") == 0) {
		/* Ignore */
	} else if (strcmp(name, "MaxOutstandingR2T") == 0) {
		/* Ignore */
	} else if (strcmp(name, "DataPDUInOrder") == 0) {
		/* Ignore */
	} else if (strcmp(name, "DataSequenceInOrder") == 0) {
		/* Ignore */
	} else if (strcmp(name, "ErrorRecoveryLevel") == 0) {
		/* Ignore */
	} else if (strcmp(name, "OFMarker") == 0) {
		/* Ignore */
	} else if (strcmp(name, "IFMarker") == 0) {
		/* Ignore */
	} else if (strcmp(name, "TargetPortalGroupTag") == 0) {
		/* Ignore */
	} else {
		log_debugx("unknown key \"%s\"; ignoring",  name);
	}
}

static void
login_negotiate(struct connection *conn)
{
	struct pdu *request, *response;
	struct keys *request_keys, *response_keys;
	struct iscsi_bhs_login_response *bhslr;
	int i;

	log_debugx("beginning operational parameter negotiation");
	request = login_new_request(conn, BHSLR_STAGE_OPERATIONAL_NEGOTIATION);
	request_keys = keys_new();

	/*
	 * The following keys are irrelevant for discovery sessions.
	 */
	if (conn->conn_conf.isc_discovery == 0) {
		if (conn->conn_conf.isc_header_digest != 0)
			keys_add(request_keys, "HeaderDigest", "CRC32C");
		else
			keys_add(request_keys, "HeaderDigest", "None");
		if (conn->conn_conf.isc_data_digest != 0)
			keys_add(request_keys, "DataDigest", "CRC32C");
		else
			keys_add(request_keys, "DataDigest", "None");

		keys_add(request_keys, "ImmediateData", "Yes");
		keys_add_int(request_keys, "MaxBurstLength",
		    ISCSI_MAX_DATA_SEGMENT_LENGTH);
		keys_add_int(request_keys, "FirstBurstLength",
		    ISCSI_MAX_DATA_SEGMENT_LENGTH);
		keys_add(request_keys, "InitialR2T", "Yes");
	} else {
		keys_add(request_keys, "HeaderDigest", "None");
		keys_add(request_keys, "DataDigest", "None");
	}

	keys_add_int(request_keys, "MaxRecvDataSegmentLength",
	    ISCSI_MAX_DATA_SEGMENT_LENGTH);
	keys_add(request_keys, "DefaultTime2Wait", "0");
	keys_add(request_keys, "DefaultTime2Retain", "0");
	keys_add(request_keys, "ErrorRecoveryLevel", "0");
	keys_add(request_keys, "MaxOutstandingR2T", "1");
	keys_save(request_keys, request);
	keys_delete(request_keys);
	request_keys = NULL;
	pdu_send(request);
	pdu_delete(request);
	request = NULL;

	response = login_receive(conn);
	response_keys = keys_new();
	keys_load(response_keys, response);
	for (i = 0; i < KEYS_MAX; i++) {
		if (response_keys->keys_names[i] == NULL)
			break;

		login_negotiate_key(conn,
		    response_keys->keys_names[i], response_keys->keys_values[i]);
	}

	bhslr = (struct iscsi_bhs_login_response *)response->pdu_bhs;
	if ((bhslr->bhslr_flags & BHSLR_FLAGS_TRANSIT) == 0)
		log_warnx("received final login response "
		    "without the \"T\" flag");
	else if (login_nsg(response) != BHSLR_STAGE_FULL_FEATURE_PHASE)
		log_warnx("received final login response with wrong NSG 0x%x",
		    login_nsg(response));

	log_debugx("operational parameter negotiation done; "
	    "transitioning to Full Feature phase");

	keys_delete(response_keys);
	pdu_delete(response);
}

static void
login_send_chap_a(struct connection *conn)
{
	struct pdu *request;
	struct keys *request_keys;

	request = login_new_request(conn, BHSLR_STAGE_SECURITY_NEGOTIATION);
	request_keys = keys_new();
	keys_add(request_keys, "CHAP_A", "5");
	keys_save(request_keys, request);
	keys_delete(request_keys);
	pdu_send(request);
	pdu_delete(request);
}

static void
login_send_chap_r(struct pdu *response)
{
	struct connection *conn;
	struct pdu *request;
	struct keys *request_keys, *response_keys;
	const char *chap_a, *chap_c, *chap_i;
	char *chap_r, *challenge, response_bin[MD5_DIGEST_LENGTH];
	size_t challenge_len;
	int error, rv;
	unsigned char id;
        char *mutual_chap_c, mutual_chap_i[4];

	/*
	 * As in the rest of the initiator, 'request' means
	 * 'initiator -> target', and 'response' means 'target -> initiator',
	 *
	 * So, here the 'response' from the target is the packet that contains
	 * CHAP challenge; our CHAP response goes into 'request'.
	 */

	conn = response->pdu_connection;

	response_keys = keys_new();
	keys_load(response_keys, response);

	/*
	 * First, compute the response.
	 */
	chap_a = keys_find(response_keys, "CHAP_A");
	if (chap_a == NULL)
		log_errx(1, "received CHAP packet without CHAP_A");
	chap_c = keys_find(response_keys, "CHAP_C");
	if (chap_c == NULL)
		log_errx(1, "received CHAP packet without CHAP_C");
	chap_i = keys_find(response_keys, "CHAP_I");
	if (chap_i == NULL)
		log_errx(1, "received CHAP packet without CHAP_I");

	if (strcmp(chap_a, "5") != 0)
		log_errx(1, "received CHAP packet "
		    "with unsupported CHAP_A \"%s\"", chap_a);
	id = strtoul(chap_i, NULL, 10);
	error = login_hex2bin(chap_c, &challenge, &challenge_len);
	if (error != 0)
		log_errx(1, "received CHAP packet with malformed CHAP_C");
	login_compute_md5(id, conn->conn_conf.isc_secret,
	    challenge, challenge_len, response_bin, sizeof(response_bin));
	free(challenge);
	chap_r = login_bin2hex(response_bin, sizeof(response_bin));

	keys_delete(response_keys);

	request = login_new_request(conn, BHSLR_STAGE_SECURITY_NEGOTIATION);
	request_keys = keys_new();
	keys_add(request_keys, "CHAP_N", conn->conn_conf.isc_user);
	keys_add(request_keys, "CHAP_R", chap_r);
	free(chap_r);

	/*
	 * If we want mutual authentication, we're expected to send
	 * our CHAP_I/CHAP_C now.
	 */
	if (conn->conn_conf.isc_mutual_user[0] != '\0') {
		log_debugx("requesting mutual authentication; "
		    "binary challenge size is %zd bytes",
		    sizeof(conn->conn_mutual_challenge));

		rv = RAND_bytes(conn->conn_mutual_challenge,
		    sizeof(conn->conn_mutual_challenge));
		if (rv != 1) {
			log_errx(1, "RAND_bytes failed: %s",
			    ERR_error_string(ERR_get_error(), NULL));
		}
		rv = RAND_bytes(&conn->conn_mutual_id,
		    sizeof(conn->conn_mutual_id));
		if (rv != 1) {
			log_errx(1, "RAND_bytes failed: %s",
			    ERR_error_string(ERR_get_error(), NULL));
		}
		mutual_chap_c = login_bin2hex(conn->conn_mutual_challenge,
		    sizeof(conn->conn_mutual_challenge));
		snprintf(mutual_chap_i, sizeof(mutual_chap_i),
		    "%d", conn->conn_mutual_id);
		keys_add(request_keys, "CHAP_I", mutual_chap_i);
		keys_add(request_keys, "CHAP_C", mutual_chap_c);
		free(mutual_chap_c);
	}

	keys_save(request_keys, request);
	keys_delete(request_keys);
	pdu_send(request);
	pdu_delete(request);
}

static void
login_verify_mutual(const struct pdu *response)
{
	struct connection *conn;
	struct keys *response_keys;
	const char *chap_n, *chap_r;
	char *response_bin, expected_response_bin[MD5_DIGEST_LENGTH];
	size_t response_bin_len;
	int error;

	conn = response->pdu_connection;

	response_keys = keys_new();
	keys_load(response_keys, response);

        chap_n = keys_find(response_keys, "CHAP_N");
        if (chap_n == NULL)
                log_errx(1, "received CHAP Response PDU without CHAP_N");
        chap_r = keys_find(response_keys, "CHAP_R");
        if (chap_r == NULL)
                log_errx(1, "received CHAP Response PDU without CHAP_R");
        error = login_hex2bin(chap_r, &response_bin, &response_bin_len);
        if (error != 0)
                log_errx(1, "received CHAP Response PDU with malformed CHAP_R");

	if (strcmp(chap_n, conn->conn_conf.isc_mutual_user) != 0) {
		fail(conn, "Mutual CHAP failed");
		log_errx(1, "mutual CHAP authentication failed: wrong user");
	}

	login_compute_md5(conn->conn_mutual_id,
	    conn->conn_conf.isc_mutual_secret, conn->conn_mutual_challenge,
	    sizeof(conn->conn_mutual_challenge), expected_response_bin,
	    sizeof(expected_response_bin));

        if (memcmp(response_bin, expected_response_bin,
            sizeof(expected_response_bin)) != 0) {
		fail(conn, "Mutual CHAP failed");
                log_errx(1, "mutual CHAP authentication failed: wrong secret");
	}

        keys_delete(response_keys);
        free(response_bin);

	log_debugx("mutual CHAP authentication succeeded");
}

static void
login_chap(struct connection *conn)
{
	struct pdu *response;

	log_debugx("beginning CHAP authentication; sending CHAP_A");
	login_send_chap_a(conn);

	log_debugx("waiting for CHAP_A/CHAP_C/CHAP_I");
	response = login_receive(conn);

	log_debugx("sending CHAP_N/CHAP_R");
	login_send_chap_r(response);
	pdu_delete(response);

	/*
	 * XXX: Make sure this is not susceptible to MITM.
	 */

	log_debugx("waiting for CHAP result");
	response = login_receive(conn);
	if (conn->conn_conf.isc_mutual_user[0] != '\0')
		login_verify_mutual(response);
	pdu_delete(response);

	log_debugx("CHAP authentication done");
}

void
login(struct connection *conn)
{
	struct pdu *request, *response;
	struct keys *request_keys, *response_keys;
	struct iscsi_bhs_login_response *bhslr2;
	const char *auth_method;
	int i;

	log_debugx("beginning Login phase; sending Login PDU");
	request = login_new_request(conn, BHSLR_STAGE_SECURITY_NEGOTIATION);
	request_keys = keys_new();
	if (conn->conn_conf.isc_mutual_user[0] != '\0') {
		keys_add(request_keys, "AuthMethod", "CHAP");
	} else if (conn->conn_conf.isc_user[0] != '\0') {
		/*
		 * Give target a chance to skip authentication if it
		 * doesn't feel like it.
		 *
		 * None is first, CHAP second; this is to work around
		 * what seems to be LIO (Linux target) bug: otherwise,
		 * if target is configured with no authentication,
		 * and we are configured to authenticate, the target
		 * will erroneously respond with AuthMethod=CHAP
		 * instead of AuthMethod=None, and will subsequently
		 * fail the connection.  This usually happens with
		 * Discovery sessions, which default to no authentication.
		 */
		keys_add(request_keys, "AuthMethod", "None,CHAP");
	} else {
		keys_add(request_keys, "AuthMethod", "None");
	}
	keys_add(request_keys, "InitiatorName",
	    conn->conn_conf.isc_initiator);
	if (conn->conn_conf.isc_initiator_alias[0] != '\0') {
		keys_add(request_keys, "InitiatorAlias",
		    conn->conn_conf.isc_initiator_alias);
	}
	if (conn->conn_conf.isc_discovery == 0) {
		keys_add(request_keys, "SessionType", "Normal");
		keys_add(request_keys,
		    "TargetName", conn->conn_conf.isc_target);
	} else {
		keys_add(request_keys, "SessionType", "Discovery");
	}
	keys_save(request_keys, request);
	keys_delete(request_keys);
	pdu_send(request);
	pdu_delete(request);

	response = login_receive(conn);

	response_keys = keys_new();
	keys_load(response_keys, response);

	for (i = 0; i < KEYS_MAX; i++) {
		if (response_keys->keys_names[i] == NULL)
			break;

		/*
		 * Not interested in AuthMethod at this point; we only need
		 * to parse things such as TargetAlias.
		 *
		 * XXX: This is somewhat ugly.  We should have a way to apply
		 * 	all the keys to the session and use that by default
		 * 	instead of discarding them.
		 */
		if (strcmp(response_keys->keys_names[i], "AuthMethod") == 0)
			continue;

		login_negotiate_key(conn,
		    response_keys->keys_names[i], response_keys->keys_values[i]);
	}

	bhslr2 = (struct iscsi_bhs_login_response *)response->pdu_bhs;
	if ((bhslr2->bhslr_flags & BHSLR_FLAGS_TRANSIT) != 0 &&
	    login_nsg(response) == BHSLR_STAGE_OPERATIONAL_NEGOTIATION) {
		if (conn->conn_conf.isc_mutual_user[0] != '\0') {
			log_errx(1, "target requested transition "
			    "to operational parameter negotiation, "
			    "but we require mutual CHAP");
		}

		log_debugx("target requested transition "
		    "to operational parameter negotiation");
		keys_delete(response_keys);
		pdu_delete(response);
		login_negotiate(conn);
		return;
	}

	auth_method = keys_find(response_keys, "AuthMethod");
	if (auth_method == NULL)
		log_errx(1, "received response without AuthMethod");
	if (strcmp(auth_method, "None") == 0) {
		if (conn->conn_conf.isc_mutual_user[0] != '\0') {
			log_errx(1, "target does not require authantication, "
			    "but we require mutual CHAP");
		}

		log_debugx("target does not require authentication");
		keys_delete(response_keys);
		pdu_delete(response);
		login_negotiate(conn);
		return;
	}

	if (strcmp(auth_method, "CHAP") != 0) {
		fail(conn, "Unsupported AuthMethod");
		log_errx(1, "received response "
		    "with unsupported AuthMethod \"%s\"", auth_method);
	}

	if (conn->conn_conf.isc_user[0] == '\0' ||
	    conn->conn_conf.isc_secret[0] == '\0') {
		fail(conn, "Authentication required");
		log_errx(1, "target requests CHAP authentication, but we don't "
		    "have user and secret");
	}

	keys_delete(response_keys);
	response_keys = NULL;
	pdu_delete(response);
	response = NULL;

	login_chap(conn);
	login_negotiate(conn);
}
