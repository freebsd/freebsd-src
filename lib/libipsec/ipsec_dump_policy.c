/*
 * Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
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
#include <sys/param.h>
#include <sys/socket.h>

#include <netkey/key_var.h>
#include <netinet/in.h>
#include <netinet6/ipsec.h>

#include <arpa/inet.h>

#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#include "ipsec_strerror.h"

static const char *ipsp_dir_strs[] = {
	"any", "in", "out",
};

static const char *ipsp_policy_strs[] = {
	"discard", "none", "ipsec", "entrust", "bypass",
};

static int set_addresses __P((char *buf, caddr_t ptr));

/*
 * policy is sadb_x_policy buffer.
 * Must call free() later.
 * When delimiter == NULL, alternatively ' '(space) is applied.
 */
char *
ipsec_dump_policy(policy, delimiter)
	caddr_t policy;
	char *delimiter;
{
	struct sadb_x_policy *xpl = (struct sadb_x_policy *)policy;
	struct sadb_x_ipsecrequest *xisr;
	int xtlen, buflen;
	char *buf;
	int error;

	/* sanity check */
	if (policy == NULL)
		return NULL;
	if (xpl->sadb_x_policy_exttype != SADB_X_EXT_POLICY) {
		ipsec_errcode = EIPSEC_INVAL_EXTTYPE;
		return NULL;
	}

	/* set delimiter */
	if (delimiter == NULL)
		delimiter = " ";

	switch (xpl->sadb_x_policy_dir) {
	case IPSEC_DIR_ANY:
	case IPSEC_DIR_INBOUND:
	case IPSEC_DIR_OUTBOUND:
		break;
	default:
		ipsec_errcode = EIPSEC_INVAL_DIR;
		return NULL;
	}

	switch (xpl->sadb_x_policy_type) {
	case IPSEC_POLICY_DISCARD:
	case IPSEC_POLICY_NONE:
	case IPSEC_POLICY_IPSEC:
	case IPSEC_POLICY_BYPASS:
	case IPSEC_POLICY_ENTRUST:
		break;
	default:
		ipsec_errcode = EIPSEC_INVAL_POLICY;
		return NULL;
	}

	buflen = strlen(ipsp_dir_strs[xpl->sadb_x_policy_dir])
		+ 1	/* space */
		+ strlen(ipsp_policy_strs[xpl->sadb_x_policy_type])
		+ 1;	/* NUL */

	if ((buf = malloc(buflen)) == NULL) {
		ipsec_errcode = EIPSEC_NO_BUFS;
		return NULL;
	}
	strcpy(buf, ipsp_dir_strs[xpl->sadb_x_policy_dir]);
	strcat(buf, " ");
	strcat(buf, ipsp_policy_strs[xpl->sadb_x_policy_type]);

	if (xpl->sadb_x_policy_type != IPSEC_POLICY_IPSEC) {
		ipsec_errcode = EIPSEC_NO_ERROR;
		return buf;
	}

	xtlen = PFKEY_EXTLEN(xpl) - sizeof(*xpl);
	xisr = (struct sadb_x_ipsecrequest *)(xpl + 1);

	/* count length of buffer for use */
	/* XXX non-seriously */
	while (xtlen > 0) {
		buflen += 20;
		if (xisr->sadb_x_ipsecrequest_mode ==IPSEC_MODE_TUNNEL)
			buflen += 50;
		xtlen -= xisr->sadb_x_ipsecrequest_len;
		xisr = (struct sadb_x_ipsecrequest *)((caddr_t)xisr
				+ xisr->sadb_x_ipsecrequest_len);
	}

	/* validity check */
	if (xtlen < 0) {
		ipsec_errcode = EIPSEC_INVAL_SADBMSG;
		free(buf);
		return NULL;
	}

	if ((buf = realloc(buf, buflen)) == NULL) {
		ipsec_errcode = EIPSEC_NO_BUFS;
		return NULL;
	}

	xtlen = PFKEY_EXTLEN(xpl) - sizeof(*xpl);
	xisr = (struct sadb_x_ipsecrequest *)(xpl + 1);

	while (xtlen > 0) {
		strcat(buf, delimiter);

		switch (xisr->sadb_x_ipsecrequest_proto) {
		case IPPROTO_ESP:
			strcat(buf, "esp");
			break;
		case IPPROTO_AH:
			strcat(buf, "ah");
			break;
		case IPPROTO_IPCOMP:
			strcat(buf, "ipcomp");
			break;
		default:
			ipsec_errcode = EIPSEC_INVAL_PROTO;
			free(buf);
			return NULL;
		}

		strcat(buf, "/");

		switch (xisr->sadb_x_ipsecrequest_mode) {
		case IPSEC_MODE_ANY:
			strcat(buf, "any");
			break;
		case IPSEC_MODE_TRANSPORT:
			strcat(buf, "transport");
			break;
		case IPSEC_MODE_TUNNEL:
			strcat(buf, "tunnel");
			break;
		default:
			ipsec_errcode = EIPSEC_INVAL_MODE;
			free(buf);
			return NULL;
		}

		strcat(buf, "/");

		if (xisr->sadb_x_ipsecrequest_len > sizeof(*xisr)) {
			error = set_addresses(buf, (caddr_t)(xisr + 1));
			if (error) {
				ipsec_errcode = EIPSEC_INVAL_MODE;
				free(buf);
				return NULL;
			}
		}

		switch (xisr->sadb_x_ipsecrequest_level) {
		case IPSEC_LEVEL_DEFAULT:
			strcat(buf, "/default");
			break;
		case IPSEC_LEVEL_USE:
			strcat(buf, "/use");
			break;
		case IPSEC_LEVEL_REQUIRE:
			strcat(buf, "/require");
			break;
		case IPSEC_LEVEL_UNIQUE:
			strcat(buf, "/unique");
			break;
		default:
			ipsec_errcode = EIPSEC_INVAL_LEVEL;
			free(buf);
			return NULL;
		}

		xtlen -= xisr->sadb_x_ipsecrequest_len;
		xisr = (struct sadb_x_ipsecrequest *)((caddr_t)xisr
				+ xisr->sadb_x_ipsecrequest_len);
	}

	ipsec_errcode = EIPSEC_NO_ERROR;
	return buf;
}

static int
set_addresses(buf, ptr)
	char *buf;
	caddr_t ptr;
{
	char tmp[100]; /* XXX */
	struct sockaddr *saddr = (struct sockaddr *)ptr;

	getnameinfo(saddr, saddr->sa_len, tmp, sizeof(tmp),
		NULL, 0, NI_NUMERICHOST);

	strcat(buf, tmp);

	strcat(buf, "-");

	saddr = (struct sockaddr *)((caddr_t)saddr + saddr->sa_len);
	getnameinfo(saddr, saddr->sa_len, tmp, sizeof(tmp),
		NULL, 0, NI_NUMERICHOST);

	strcat(buf, tmp);

	return 0;
}
