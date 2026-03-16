/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * This software was developed as part of the tcpstats kernel module.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Dual-compile filter string parser for tcpstats.
 *
 * Compiles as both kernel module code (_KERNEL) and userspace test code.
 * Parses filter strings like "local_port=443 exclude=listen,timewait"
 * into a tcpstats_filter struct.
 */

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/libkern.h>
#include <netinet/in.h>
#include <netinet/tcp_fsm.h>
#else
/* Userspace shims */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <errno.h>

#define	log(level, fmt, ...)	fprintf(stderr, fmt, ##__VA_ARGS__)
#ifndef bzero
#define	bzero(ptr, len)		memset(ptr, 0, len)
#endif
#ifndef bcopy
#define	bcopy(src, dst, len)	memcpy(dst, src, len)
#endif
#endif /* _KERNEL */

/* ASCII-only tolower -- input guaranteed printable ASCII (0x20-0x7E) */
#define	TSF_TOLOWER(c)	(((c) >= 'A' && (c) <= 'Z') ? ((c) | 0x20) : (c))

#include <netinet/tcp_statsdev_filter.h>

/* TCP state constants for userspace compilation */
#ifndef _KERNEL
#ifndef TCPS_CLOSED
#define	TCPS_CLOSED		0
#define	TCPS_LISTEN		1
#define	TCPS_SYN_SENT		2
#define	TCPS_SYN_RECEIVED	3
#define	TCPS_ESTABLISHED	4
#define	TCPS_CLOSE_WAIT		5
#define	TCPS_FIN_WAIT_1		6
#define	TCPS_CLOSING		7
#define	TCPS_LAST_ACK		8
#define	TCPS_FIN_WAIT_2		9
#define	TCPS_TIME_WAIT		10
#endif
#endif

/* --- State name table --- */

static const struct {
	const char	*name;
	int		state;
} tsf_state_names[] = {
	{ "closed",		TCPS_CLOSED },
	{ "listen",		TCPS_LISTEN },
	{ "syn_sent",		TCPS_SYN_SENT },
	{ "syn_received",	TCPS_SYN_RECEIVED },
	{ "established",	TCPS_ESTABLISHED },
	{ "close_wait",		TCPS_CLOSE_WAIT },
	{ "fin_wait_1",		TCPS_FIN_WAIT_1 },
	{ "closing",		TCPS_CLOSING },
	{ "last_ack",		TCPS_LAST_ACK },
	{ "fin_wait_2",		TCPS_FIN_WAIT_2 },
	{ "time_wait",		TCPS_TIME_WAIT },
	/* Aliases */
	{ "timewait",		TCPS_TIME_WAIT },
	{ "closewait",		TCPS_CLOSE_WAIT },
	{ "syn_rcvd",		TCPS_SYN_RECEIVED },
	{ NULL,			0 }
};

/* --- Field group table --- */

static const struct {
	const char	*name;
	uint32_t	mask;
} tsf_field_groups[] = {
	{ "identity",		TSR_FIELDS_IDENTITY },
	{ "state",		TSR_FIELDS_STATE },
	{ "congestion",		TSR_FIELDS_CONGESTION },
	{ "rtt",		TSR_FIELDS_RTT },
	{ "sequences",		TSR_FIELDS_SEQUENCES },
	{ "counters",		TSR_FIELDS_COUNTERS },
	{ "timers",		TSR_FIELDS_TIMERS },
	{ "buffers",		TSR_FIELDS_BUFFERS },
	{ "ecn",		TSR_FIELDS_ECN },
	{ "names",		TSR_FIELDS_NAMES },
	{ "all",		TSR_FIELDS_ALL },
	{ "default",		TSR_FIELDS_DEFAULT },
	{ NULL,			0 }
};

/* --- Forward declarations --- */

static int	tsf_parse_directive(char *token, struct tcpstats_filter *f,
		    char *errbuf, size_t errbuflen);
static int	tsf_parse_port_number(const char *str, uint16_t *port_out,
		    char *errbuf, size_t errbuflen);
static int	tsf_parse_port_list(char *value, uint16_t *ports, int maxports,
		    uint32_t *flags, uint32_t flag_bit,
		    char *errbuf, size_t errbuflen);
static int	tsf_parse_exclude_list(char *value, struct tcpstats_filter *f,
		    char *errbuf, size_t errbuflen);
static int	tsf_parse_include_state_list(char *value,
		    struct tcpstats_filter *f,
		    char *errbuf, size_t errbuflen);
static int	tsf_parse_addr(char *value, struct tcpstats_filter *f,
		    uint32_t flag_bit, char *errbuf, size_t errbuflen);
static int	tsf_parse_ipv4_addr(const char *str, struct in_addr *addr,
		    struct in_addr *mask, char *errbuf, size_t errbuflen);
static int	tsf_parse_ipv6_addr(const char *str, struct in6_addr *addr,
		    uint8_t *prefix_out, char *errbuf, size_t errbuflen);
static int	tsf_validate_v6_cidr(const struct in6_addr *addr,
		    uint8_t prefix, char *errbuf, size_t errbuflen);
static int	tsf_parse_format(char *value, struct tcpstats_filter *f,
		    char *errbuf, size_t errbuflen);
static int	tsf_parse_field_list(char *value, struct tcpstats_filter *f,
		    char *errbuf, size_t errbuflen);
static int	tsf_validate_filter(struct tcpstats_filter *f,
		    char *errbuf, size_t errbuflen);

/* ================================================================
 * Top-level parser
 * ================================================================ */

int
tsf_parse_filter_string(const char *input, size_t len,
    struct tcpstats_filter *out, char *errbuf, size_t errbuflen)
{
	struct tcpstats_filter tmp;
	char buf[TSF_PARSE_MAXLEN];
	char *tokens[TSF_PARSE_MAXDIRECTIVES];
	int ntokens, error;
	size_t i;

	/* --- Input validation --- */
	if (len == 0 || input[0] == '\0') {
		bzero(out, sizeof(*out));
		out->version = TSF_VERSION;
		out->state_mask = 0xFFFF;
		out->field_mask = TSR_FIELDS_DEFAULT;
		return (0);
	}

	if (len > TSF_PARSE_MAXLEN - 1) {
		snprintf(errbuf, errbuflen,
		    "filter string too long (%zu > %d)",
		    len, TSF_PARSE_MAXLEN - 1);
		return (ENAMETOOLONG);
	}

	/* Validate printable ASCII + copy in a single pass */
	for (i = 0; i < len; i++) {
		if (input[i] != '\0' &&
		    (input[i] < 0x20 || input[i] > 0x7E)) {
			snprintf(errbuf, errbuflen,
			    "non-printable character 0x%02x at offset %zu",
			    (unsigned char)input[i], i);
			return (EINVAL);
		}
		buf[i] = input[i];
	}
	buf[len] = '\0';

	/* --- Pass 1: Tokenize --- */
	ntokens = 0;
	{
		char *p = buf;
		char *tok;

		while ((tok = strsep(&p, " \t")) != NULL) {
			if (tok[0] == '\0')
				continue;
			if (ntokens >= TSF_PARSE_MAXDIRECTIVES) {
				snprintf(errbuf, errbuflen,
				    "too many directives (%d > %d)",
				    ntokens + 1, TSF_PARSE_MAXDIRECTIVES);
				return (EINVAL);
			}
			tokens[ntokens++] = tok;
		}
	}

	if (ntokens == 0) {
		bzero(out, sizeof(*out));
		out->version = TSF_VERSION;
		out->state_mask = 0xFFFF;
		out->field_mask = TSR_FIELDS_DEFAULT;
		return (0);
	}

	/* --- Pass 2: Parse into temporary struct --- */
	bzero(&tmp, sizeof(tmp));
	tmp.version = TSF_VERSION;
	tmp.state_mask = 0xFFFF;
	tmp.field_mask = TSR_FIELDS_DEFAULT;

	for (i = 0; i < (size_t)ntokens; i++) {
		error = tsf_parse_directive(tokens[i], &tmp,
		    errbuf, errbuflen);
		if (error != 0)
			return (error);
	}

	/* --- Cross-directive validation --- */
	error = tsf_validate_filter(&tmp, errbuf, errbuflen);
	if (error != 0)
		return (error);

	/* --- Success: atomic copy to output --- */
	bcopy(&tmp, out, sizeof(*out));
	return (0);
}

/* ================================================================
 * Per-directive dispatcher
 * ================================================================ */

static int
tsf_parse_directive(char *token, struct tcpstats_filter *f,
    char *errbuf, size_t errbuflen)
{
	char *key, *value, *c;

	/* Split on '=' */
	key = token;
	value = strchr(token, '=');
	if (value != NULL) {
		*value = '\0';
		value++;
		if (*value == '\0') {
			snprintf(errbuf, errbuflen,
			    "directive '%s' has empty value", key);
			return (EINVAL);
		}
	}

	/* Normalize key to lowercase */
	for (c = key; *c != '\0'; c++)
		*c = TSF_TOLOWER(*c);

	/* --- Dispatch by first character to reduce strcmp calls --- */
	switch (key[0]) {
	case 'e':
		if (value == NULL)
			break;
		if (strcmp(key, "exclude") == 0)
			return (tsf_parse_exclude_list(value, f,
			    errbuf, errbuflen));
		break;
	case 'f':
		if (value == NULL)
			break;
		if (strcmp(key, "format") == 0)
			return (tsf_parse_format(value, f,
			    errbuf, errbuflen));
		if (strcmp(key, "fields") == 0)
			return (tsf_parse_field_list(value, f,
			    errbuf, errbuflen));
		break;
	case 'i':
		if (strcmp(key, "ipv4_only") == 0) {
			if (value != NULL) {
				snprintf(errbuf, errbuflen,
				    "'ipv4_only' is a flag and does not "
				    "accept a value");
				return (EINVAL);
			}
			f->flags |= TSF_IPV4_ONLY;
			return (0);
		}
		if (strcmp(key, "ipv6_only") == 0) {
			if (value != NULL) {
				snprintf(errbuf, errbuflen,
				    "'ipv6_only' is a flag and does not "
				    "accept a value");
				return (EINVAL);
			}
			f->flags |= TSF_IPV6_ONLY;
			return (0);
		}
		if (value == NULL)
			break;
		if (strcmp(key, "include_state") == 0)
			return (tsf_parse_include_state_list(value, f,
			    errbuf, errbuflen));
		break;
	case 'l':
		if (value == NULL)
			break;
		if (strcmp(key, "local_port") == 0)
			return (tsf_parse_port_list(value,
			    f->local_ports, TSF_MAX_PORTS,
			    &f->flags, TSF_LOCAL_PORT_MATCH,
			    errbuf, errbuflen));
		if (strcmp(key, "local_addr") == 0)
			return (tsf_parse_addr(value, f,
			    TSF_LOCAL_ADDR_MATCH,
			    errbuf, errbuflen));
		break;
	case 'r':
		if (value == NULL)
			break;
		if (strcmp(key, "remote_port") == 0)
			return (tsf_parse_port_list(value,
			    f->remote_ports, TSF_MAX_PORTS,
			    &f->flags, TSF_REMOTE_PORT_MATCH,
			    errbuf, errbuflen));
		if (strcmp(key, "remote_addr") == 0)
			return (tsf_parse_addr(value, f,
			    TSF_REMOTE_ADDR_MATCH,
			    errbuf, errbuflen));
		break;
	default:
		break;
	}

	/* Unrecognized key */
	if (value == NULL)
		snprintf(errbuf, errbuflen,
		    "unknown flag '%s' (did you mean '%s=...'?)", key, key);
	else
		snprintf(errbuf, errbuflen, "unknown directive '%s'", key);
	return (EINVAL);
}

/* ================================================================
 * Port parsing
 * ================================================================ */

static int
tsf_parse_port_number(const char *str, uint16_t *port_out,
    char *errbuf, size_t errbuflen)
{
	unsigned long val;
	size_t len, i;

	if (str == NULL || str[0] == '\0') {
		snprintf(errbuf, errbuflen, "empty port number");
		return (EINVAL);
	}

	len = strnlen(str, 6);

	/* Reject leading zeros (octal confusion) */
	if (len > 1 && str[0] == '0') {
		snprintf(errbuf, errbuflen,
		    "port '%s' has leading zero (octal not supported)", str);
		return (EINVAL);
	}

	/* Reject if more than 5 digits */
	if (len > 5) {
		snprintf(errbuf, errbuflen,
		    "port '%s' too many digits (max 65535)", str);
		return (EINVAL);
	}

	/* Validate digits + convert in a single pass (avoids strtoul) */
	val = 0;
	for (i = 0; i < len && str[i] != '\0'; i++) {
		if (str[i] < '0' || str[i] > '9') {
			snprintf(errbuf, errbuflen,
			    "port '%s' contains non-digit character '%c'",
			    str, str[i]);
			return (EINVAL);
		}
		val = val * 10 + (unsigned long)(str[i] - '0');
	}

	if (val == 0) {
		snprintf(errbuf, errbuflen, "port 0 is not valid");
		return (EINVAL);
	}
	if (val > 65535) {
		snprintf(errbuf, errbuflen,
		    "port %lu exceeds maximum 65535", val);
		return (EINVAL);
	}

	*port_out = htons((uint16_t)val);
	return (0);
}

static int
tsf_parse_port_list(char *value, uint16_t *ports, int maxports,
    uint32_t *flags, uint32_t flag_bit,
    char *errbuf, size_t errbuflen)
{
	char *tok, *p;
	int count, i;
	uint16_t port;
	int error;

	if (*flags & flag_bit) {
		snprintf(errbuf, errbuflen, "duplicate port directive");
		return (EINVAL);
	}

	count = 0;
	p = value;
	while ((tok = strsep(&p, ",")) != NULL) {
		if (tok[0] == '\0')
			continue;

		if (count >= maxports) {
			snprintf(errbuf, errbuflen,
			    "too many ports (max %d per direction)",
			    maxports);
			return (EINVAL);
		}

		error = tsf_parse_port_number(tok, &port,
		    errbuf, errbuflen);
		if (error != 0)
			return (error);

		/* Duplicate detection */
		for (i = 0; i < count; i++) {
			if (ports[i] == port) {
				snprintf(errbuf, errbuflen,
				    "duplicate port %u", ntohs(port));
				return (EINVAL);
			}
		}

		ports[count++] = port;
	}

	if (count == 0) {
		snprintf(errbuf, errbuflen, "empty port list");
		return (EINVAL);
	}

	*flags |= flag_bit;
	return (0);
}

/* ================================================================
 * State parsing
 * ================================================================ */

static int
tsf_lookup_state(const char *name, int *state_out,
    char *errbuf, size_t errbuflen)
{
	char lname[32];
	size_t i;

	/* Normalize to lowercase in a temp buffer */
	for (i = 0; i < sizeof(lname) - 1 && name[i] != '\0'; i++)
		lname[i] = TSF_TOLOWER(name[i]);
	lname[i] = '\0';

	for (i = 0; tsf_state_names[i].name != NULL; i++) {
		if (strcmp(lname, tsf_state_names[i].name) == 0) {
			*state_out = tsf_state_names[i].state;
			return (0);
		}
	}

	snprintf(errbuf, errbuflen, "unknown state name '%s'", name);
	return (EINVAL);
}

static int
tsf_parse_exclude_list(char *value, struct tcpstats_filter *f,
    char *errbuf, size_t errbuflen)
{
	char *tok, *p;
	int state;
	int error;
	uint16_t seen;

	/* Check for conflict with include_state */
	if (f->flags & TSF_STATE_INCLUDE_MODE) {
		snprintf(errbuf, errbuflen,
		    "'exclude' and 'include_state' are mutually exclusive");
		return (EINVAL);
	}

	seen = 0;
	p = value;
	while ((tok = strsep(&p, ",")) != NULL) {
		if (tok[0] == '\0')
			continue;

		error = tsf_lookup_state(tok, &state, errbuf, errbuflen);
		if (error != 0)
			return (error);

		/* Duplicate detection */
		if (seen & (1 << state)) {
			snprintf(errbuf, errbuflen,
			    "duplicate state '%s' in exclude list", tok);
			return (EINVAL);
		}
		seen |= (1 << state);

		/* Clear the state bit in state_mask */
		f->state_mask &= (uint16_t)~(1 << state);
	}

	return (0);
}

static int
tsf_parse_include_state_list(char *value, struct tcpstats_filter *f,
    char *errbuf, size_t errbuflen)
{
	char *tok, *p;
	int state;
	int error;
	uint16_t mask, seen;

	/* Check for conflict with exclude */
	if (f->state_mask != 0xFFFF) {
		/* state_mask has been modified by exclude= */
		snprintf(errbuf, errbuflen,
		    "'exclude' and 'include_state' are mutually exclusive");
		return (EINVAL);
	}

	if (f->flags & TSF_STATE_INCLUDE_MODE) {
		snprintf(errbuf, errbuflen,
		    "duplicate 'include_state' directive");
		return (EINVAL);
	}

	mask = 0;
	seen = 0;
	p = value;
	while ((tok = strsep(&p, ",")) != NULL) {
		if (tok[0] == '\0')
			continue;

		error = tsf_lookup_state(tok, &state, errbuf, errbuflen);
		if (error != 0)
			return (error);

		/* Duplicate detection */
		if (seen & (1 << state)) {
			snprintf(errbuf, errbuflen,
			    "duplicate state '%s' in include_state list",
			    tok);
			return (EINVAL);
		}
		seen |= (1 << state);
		mask |= (1 << state);
	}

	if (mask == 0) {
		snprintf(errbuf, errbuflen, "empty include_state list");
		return (EINVAL);
	}

	f->state_mask = mask;
	f->flags |= TSF_STATE_INCLUDE_MODE;
	return (0);
}

/* ================================================================
 * Address parsing
 * ================================================================ */

static int
tsf_parse_prefix_length(const char *str, int maxprefix, int *prefix_out,
    char *errbuf, size_t errbuflen)
{
	unsigned long val;
	size_t len, i;

	if (str == NULL || str[0] == '\0') {
		snprintf(errbuf, errbuflen, "empty prefix length");
		return (EINVAL);
	}

	len = strnlen(str, 4);

	/* Reject leading zeros */
	if (len > 1 && str[0] == '0') {
		snprintf(errbuf, errbuflen,
		    "prefix length '%s' has leading zero", str);
		return (EINVAL);
	}

	/* Validate digits + convert in a single pass (avoids strtoul) */
	val = 0;
	for (i = 0; i < len && str[i] != '\0'; i++) {
		if (str[i] < '0' || str[i] > '9') {
			snprintf(errbuf, errbuflen,
			    "prefix length '%s' contains non-digit", str);
			return (EINVAL);
		}
		val = val * 10 + (unsigned long)(str[i] - '0');
	}
	if ((int)val > maxprefix) {
		snprintf(errbuf, errbuflen,
		    "prefix length %lu exceeds maximum %d",
		    val, maxprefix);
		return (EINVAL);
	}

	*prefix_out = (int)val;
	return (0);
}

static int
tsf_parse_ipv4_addr(const char *str, struct in_addr *addr,
    struct in_addr *mask, char *errbuf, size_t errbuflen)
{
	const char *slash;
	char addrbuf[16];
	int prefix, error;

	prefix = 32;

	/* Separate address from prefix */
	slash = strchr(str, '/');
	if (slash != NULL) {
		size_t addrlen = (size_t)(slash - str);

		if (addrlen >= sizeof(addrbuf)) {
			snprintf(errbuf, errbuflen, "IPv4 address too long");
			return (EINVAL);
		}
		strlcpy(addrbuf, str, addrlen + 1);

		error = tsf_parse_prefix_length(slash + 1, 32, &prefix,
		    errbuf, errbuflen);
		if (error != 0)
			return (error);
	} else {
		if (strnlen(str, sizeof(addrbuf)) >= sizeof(addrbuf)) {
			snprintf(errbuf, errbuflen, "IPv4 address too long");
			return (EINVAL);
		}
		strlcpy(addrbuf, str, sizeof(addrbuf));
	}

	/* Parse 4 octets manually */
	{
		const char *p = addrbuf;
		uint32_t octets[4];
		int noctets = 0;

		while (noctets < 4) {
			unsigned long val = 0;
			int ndigits = 0;
			const char *start = p;

			while (*p >= '0' && *p <= '9' && ndigits < 4) {
				val = val * 10 +
				    (unsigned long)(*p - '0');
				ndigits++;
				p++;
			}

			if (ndigits == 0) {
				snprintf(errbuf, errbuflen,
				    "IPv4 address has %d octets "
				    "(expected 4)", noctets);
				return (EINVAL);
			}

			/* Reject leading zeros in octets */
			if (ndigits > 1 && start[0] == '0') {
				snprintf(errbuf, errbuflen,
				    "IPv4 octet has leading zero");
				return (EINVAL);
			}

			if (val > 255) {
				snprintf(errbuf, errbuflen,
				    "IPv4 octet %lu exceeds 255", val);
				return (EINVAL);
			}

			octets[noctets++] = (uint32_t)val;

			if (noctets < 4) {
				if (*p != '.') {
					snprintf(errbuf, errbuflen,
					    "IPv4 address has %d octets "
					    "(expected 4)", noctets);
					return (EINVAL);
				}
				p++;
			}
		}

		if (*p != '\0') {
			snprintf(errbuf, errbuflen,
			    "trailing characters after IPv4 address");
			return (EINVAL);
		}

		addr->s_addr = htonl(
		    (octets[0] << 24) | (octets[1] << 16) |
		    (octets[2] << 8) | octets[3]);
	}

	/* Compute netmask from prefix */
	if (prefix == 0)
		mask->s_addr = 0;
	else
		mask->s_addr = htonl(~((1U << (32 - prefix)) - 1));

	/* Validate host bits are zero */
	if ((addr->s_addr & ~mask->s_addr) != 0) {
		snprintf(errbuf, errbuflen,
		    "host bits set in IPv4 CIDR (prefix /%d)", prefix);
		return (EINVAL);
	}

	return (0);
}

static int
tsf_parse_ipv6_addr(const char *str, struct in6_addr *addr,
    uint8_t *prefix_out, char *errbuf, size_t errbuflen)
{
	uint16_t groups[8];
	int ngroups_before, double_colon, gi, i;
	const char *p, *slash;
	char addrbuf[48];

	ngroups_before = 0;
	double_colon = -1;
	p = str;

	bzero(groups, sizeof(groups));
	bzero(addr, sizeof(*addr));

	/* Separate address from prefix length */
	slash = strchr(str, '/');
	if (slash != NULL) {
		size_t addrlen = (size_t)(slash - str);

		if (addrlen >= sizeof(addrbuf)) {
			snprintf(errbuf, errbuflen,
			    "IPv6 address too long before '/'");
			return (EINVAL);
		}
		strlcpy(addrbuf, str, addrlen + 1);
		p = addrbuf;
	}

	/* Parse groups with :: detection */
	gi = 0;

	while (*p != '\0' && gi < 8) {
		/* Check for :: */
		if (p[0] == ':' && p[1] == ':') {
			if (double_colon >= 0) {
				snprintf(errbuf, errbuflen,
				    "multiple '::' in IPv6 address");
				return (EINVAL);
			}
			double_colon = gi;
			ngroups_before = gi;
			p += 2;
			if (*p == '\0')
				break;
			continue;
		}

		/* Skip single colon separator */
		if (*p == ':') {
			if (gi == 0 && double_colon < 0) {
				snprintf(errbuf, errbuflen,
				    "IPv6 address starts with "
				    "single ':'");
				return (EINVAL);
			}
			p++;
		}

		/* Parse hex group (1-4 hex digits) */
		{
			int seen_digits = 0;
			uint32_t val = 0;
			char c;

			while (*p != '\0' && *p != ':' &&
			    *p != '/' && seen_digits < 4) {
				c = TSF_TOLOWER(*p);
				if (c >= '0' && c <= '9')
					val = (val << 4) |
					    (uint32_t)(c - '0');
				else if (c >= 'a' && c <= 'f')
					val = (val << 4) |
					    (uint32_t)(c - 'a' + 10);
				else {
					snprintf(errbuf, errbuflen,
					    "invalid character '%c' "
					    "in IPv6 group", *p);
					return (EINVAL);
				}
				seen_digits++;
				p++;
			}

			if (seen_digits == 0) {
				snprintf(errbuf, errbuflen,
				    "empty group in IPv6 address");
				return (EINVAL);
			}
			if (val > 0xFFFF) {
				snprintf(errbuf, errbuflen,
				    "IPv6 group value 0x%x "
				    "exceeds 0xFFFF", val);
				return (EINVAL);
			}

			groups[gi++] = (uint16_t)val;
		}
	}

	/* Determine total groups and expand :: */
	if (double_colon >= 0) {
		int ngroups_after = gi - ngroups_before;
		int total_groups = ngroups_before + ngroups_after;
		int zero_fill;

		if (total_groups > 7) {
			snprintf(errbuf, errbuflen,
			    "too many groups (%d) with '::' "
			    "in IPv6 address", total_groups);
			return (EINVAL);
		}

		zero_fill = 8 - total_groups;
		for (i = ngroups_after - 1; i >= 0; i--)
			groups[ngroups_before + zero_fill + i] =
			    groups[ngroups_before + i];
		for (i = 0; i < zero_fill; i++)
			groups[ngroups_before + i] = 0;
	} else {
		if (gi != 8) {
			snprintf(errbuf, errbuflen,
			    "IPv6 address has %d groups "
			    "(expected 8, or use '::')", gi);
			return (EINVAL);
		}
	}

	/* Convert groups to in6_addr (network byte order) */
	for (i = 0; i < 8; i++) {
		addr->s6_addr[i * 2] =
		    (uint8_t)((groups[i] >> 8) & 0xFF);
		addr->s6_addr[i * 2 + 1] =
		    (uint8_t)(groups[i] & 0xFF);
	}

	/* Parse prefix length if present */
	if (prefix_out != NULL && slash != NULL) {
		int prefix, error;

		error = tsf_parse_prefix_length(slash + 1, 128, &prefix,
		    errbuf, errbuflen);
		if (error != 0)
			return (error);
		*prefix_out = (uint8_t)prefix;

		/* Validate host bits */
		error = tsf_validate_v6_cidr(addr, *prefix_out,
		    errbuf, errbuflen);
		if (error != 0)
			return (error);
	} else if (prefix_out != NULL) {
		*prefix_out = 128;
	}

	return (0);
}

static int
tsf_validate_v6_cidr(const struct in6_addr *addr, uint8_t prefix,
    char *errbuf, size_t errbuflen)
{
	int full_bytes, remainder_bits, i;

	if (prefix == 128 || prefix == 0)
		return (0);

	full_bytes = prefix / 8;
	remainder_bits = prefix % 8;

	/* Check partial byte */
	if (remainder_bits > 0) {
		uint8_t mask = (uint8_t)(0xFF << (8 - remainder_bits));

		if ((addr->s6_addr[full_bytes] & ~mask) != 0) {
			snprintf(errbuf, errbuflen,
			    "host bits set in IPv6 CIDR (prefix /%u)",
			    prefix);
			return (EINVAL);
		}
		full_bytes++;
	}

	/* Check remaining bytes */
	for (i = full_bytes; i < 16; i++) {
		if (addr->s6_addr[i] != 0) {
			snprintf(errbuf, errbuflen,
			    "host bits set in IPv6 CIDR (prefix /%u)",
			    prefix);
			return (EINVAL);
		}
	}

	return (0);
}

static int
tsf_parse_addr(char *value, struct tcpstats_filter *f, uint32_t flag_bit,
    char *errbuf, size_t errbuflen)
{
	int error;

	if (f->flags & flag_bit) {
		snprintf(errbuf, errbuflen, "duplicate address directive");
		return (EINVAL);
	}

	if (strchr(value, ':') != NULL) {
		/* IPv6 */
		if (flag_bit == TSF_LOCAL_ADDR_MATCH) {
			error = tsf_parse_ipv6_addr(value,
			    &f->local_addr_v6, &f->local_prefix_v6,
			    errbuf, errbuflen);
		} else {
			error = tsf_parse_ipv6_addr(value,
			    &f->remote_addr_v6, &f->remote_prefix_v6,
			    errbuf, errbuflen);
		}
	} else {
		/* IPv4 */
		if (flag_bit == TSF_LOCAL_ADDR_MATCH) {
			error = tsf_parse_ipv4_addr(value,
			    &f->local_addr_v4, &f->local_mask_v4,
			    errbuf, errbuflen);
		} else {
			error = tsf_parse_ipv4_addr(value,
			    &f->remote_addr_v4, &f->remote_mask_v4,
			    errbuf, errbuflen);
		}
	}

	if (error != 0)
		return (error);

	f->flags |= flag_bit;
	return (0);
}

/* ================================================================
 * Format and fields parsing
 * ================================================================ */

static int
tsf_parse_format(char *value, struct tcpstats_filter *f,
    char *errbuf, size_t errbuflen)
{
	char *c;

	/* Normalize to lowercase */
	for (c = value; *c != '\0'; c++)
		*c = TSF_TOLOWER(*c);

	if (strcmp(value, "compact") == 0) {
		f->format = TSF_FORMAT_COMPACT;
	} else if (strcmp(value, "full") == 0) {
		f->format = TSF_FORMAT_FULL;
	} else {
		snprintf(errbuf, errbuflen,
		    "unknown format '%s' (expected 'compact' or 'full')",
		    value);
		return (EINVAL);
	}
	return (0);
}

static int
tsf_parse_field_list(char *value, struct tcpstats_filter *f,
    char *errbuf, size_t errbuflen)
{
	char *tok, *p, *c;
	uint32_t mask;
	int i, found;

	mask = 0;
	p = value;
	while ((tok = strsep(&p, ",")) != NULL) {
		if (tok[0] == '\0')
			continue;

		/* Normalize to lowercase */
		for (c = tok; *c != '\0'; c++)
			*c = TSF_TOLOWER(*c);

		found = 0;
		for (i = 0; tsf_field_groups[i].name != NULL; i++) {
			if (strcmp(tok, tsf_field_groups[i].name) == 0) {
				mask |= tsf_field_groups[i].mask;
				found = 1;
				break;
			}
		}

		if (!found) {
			snprintf(errbuf, errbuflen,
			    "unknown field group '%s'", tok);
			return (EINVAL);
		}
	}

	if (mask == 0) {
		snprintf(errbuf, errbuflen, "empty field list");
		return (EINVAL);
	}

	f->field_mask = mask;
	return (0);
}

/* ================================================================
 * Cross-directive validation
 * ================================================================ */

static int
tsf_validate_filter(struct tcpstats_filter *f,
    char *errbuf, size_t errbuflen)
{

	/* ipv4_only + ipv6_only conflict */
	if ((f->flags & TSF_IPV4_ONLY) && (f->flags & TSF_IPV6_ONLY)) {
		snprintf(errbuf, errbuflen,
		    "'ipv4_only' and 'ipv6_only' are mutually exclusive");
		return (EINVAL);
	}

	/* IPv4 address + ipv6_only conflict */
	if ((f->flags & TSF_LOCAL_ADDR_MATCH) &&
	    f->local_addr_v4.s_addr != INADDR_ANY &&
	    (f->flags & TSF_IPV6_ONLY)) {
		snprintf(errbuf, errbuflen,
		    "IPv4 address conflicts with ipv6_only flag");
		return (EINVAL);
	}
	if ((f->flags & TSF_REMOTE_ADDR_MATCH) &&
	    f->remote_addr_v4.s_addr != INADDR_ANY &&
	    (f->flags & TSF_IPV6_ONLY)) {
		snprintf(errbuf, errbuflen,
		    "IPv4 address conflicts with ipv6_only flag");
		return (EINVAL);
	}

	/* IPv6 address + ipv4_only conflict */
	if ((f->flags & TSF_LOCAL_ADDR_MATCH) &&
	    !IN6_IS_ADDR_UNSPECIFIED(&f->local_addr_v6) &&
	    (f->flags & TSF_IPV4_ONLY)) {
		snprintf(errbuf, errbuflen,
		    "IPv6 address conflicts with ipv4_only flag");
		return (EINVAL);
	}
	if ((f->flags & TSF_REMOTE_ADDR_MATCH) &&
	    !IN6_IS_ADDR_UNSPECIFIED(&f->remote_addr_v6) &&
	    (f->flags & TSF_IPV4_ONLY)) {
		snprintf(errbuf, errbuflen,
		    "IPv6 address conflicts with ipv4_only flag");
		return (EINVAL);
	}

	return (0);
}
