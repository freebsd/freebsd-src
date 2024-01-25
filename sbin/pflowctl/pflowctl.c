/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Rubicon Communications, LLC (Netgate)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <sys/cdefs.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <net/pflow.h>

#include <netlink/netlink.h>
#include <netlink/netlink_generic.h>
#include <netlink/netlink_snl.h>
#include <netlink/netlink_snl_generic.h>
#include <netlink/netlink_snl_route.h>

static int get(int id);

static bool verbose = false;

extern char *__progname;

static void
usage(void)
{
	fprintf(stderr,
"usage: %s [-lc] [-d id] [-s id ...] [-v]\n",
	    __progname);

	exit(1);
}

static int
pflow_to_id(const char *name)
{
	int ret, id;

	ret = sscanf(name, "pflow%d", &id);
	if (ret == 1)
		return (id);

	ret = sscanf(name, "%d", &id);
	if (ret == 1)
		return (id);

	return (-1);
}

struct pflowctl_list {
	int id;
};
#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct pflowctl_list, _field)
static struct snl_attr_parser ap_list[] = {
	{ .type = PFLOWNL_L_ID, .off = _OUT(id), .cb = snl_attr_get_int32 },
};
static struct snl_field_parser fp_list[] = {};
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER(list_parser, struct genlmsghdr, fp_list, ap_list);

static int
list(void)
{
	struct snl_state ss = {};
	struct snl_errmsg_data e = {};
	struct pflowctl_list l = {};
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	uint32_t seq_id;
	int family_id;

	snl_init(&ss, NETLINK_GENERIC);
	family_id = snl_get_genl_family(&ss, PFLOWNL_FAMILY_NAME);
	if (family_id == 0)
		errx(1, "pflow.ko is not loaded.");

	snl_init_writer(&ss, &nw);
	hdr = snl_create_genl_msg_request(&nw, family_id, PFLOWNL_CMD_LIST);

	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL)
		return (ENOMEM);
	seq_id = hdr->nlmsg_seq;

	snl_send_message(&ss, hdr);

	while ((hdr = snl_read_reply_multi(&ss, seq_id, &e)) != NULL) {
		if (! snl_parse_nlmsg(&ss, hdr, &list_parser, &l))
			continue;

		get(l.id);
	}

	if (e.error)
		errc(1, e.error, "failed to list");

	return (0);
}

struct pflowctl_create {
	int id;
};
#define	_IN(_field)	offsetof(struct genlmsghsdr, _field)
#define	_OUT(_field)	offsetof(struct pflowctl_create, _field)
static struct snl_attr_parser ap_create[] = {
	{ .type = PFLOWNL_CREATE_ID, .off = _OUT(id), .cb = snl_attr_get_int32 },
};
static struct snl_field_parser pf_create[] = {};
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER(create_parser, struct genlmsghdr, pf_create, ap_create);

static int
create(void)
{
	struct snl_state ss = {};
	struct snl_errmsg_data e = {};
	struct pflowctl_create c = {};
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	uint32_t seq_id;
	int family_id;

	snl_init(&ss, NETLINK_GENERIC);
	family_id = snl_get_genl_family(&ss, PFLOWNL_FAMILY_NAME);
	if (family_id == 0)
		errx(1, "pflow.ko is not loaded.");

	snl_init_writer(&ss, &nw);
	hdr = snl_create_genl_msg_request(&nw, family_id, PFLOWNL_CMD_CREATE);

	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL)
		return (ENOMEM);
	seq_id = hdr->nlmsg_seq;

	snl_send_message(&ss, hdr);

	while ((hdr = snl_read_reply_multi(&ss, seq_id, &e)) != NULL) {
		if (! snl_parse_nlmsg(&ss, hdr, &create_parser, &c))
			continue;

		printf("pflow%d\n", c.id);
	}

	if (e.error)
		errc(1, e.error, "failed to create");

	return (0);
}

static int
del(char *idstr)
{
	struct snl_state ss = {};
	struct snl_errmsg_data e = {};
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	int family_id;
	int id;

	id = pflow_to_id(idstr);
	if (id < 0)
		return (EINVAL);

	snl_init(&ss, NETLINK_GENERIC);
	family_id = snl_get_genl_family(&ss, PFLOWNL_FAMILY_NAME);
	if (family_id == 0)
		errx(1, "pflow.ko is not loaded.");

	snl_init_writer(&ss, &nw);
	hdr = snl_create_genl_msg_request(&nw, family_id, PFLOWNL_CMD_DEL);

	snl_add_msg_attr_s32(&nw, PFLOWNL_DEL_ID, id);

	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL)
		return (ENOMEM);

	snl_send_message(&ss, hdr);
	snl_read_reply_code(&ss, hdr->nlmsg_seq, &e);

	if (e.error)
		errc(1, e.error, "failed to delete");

	return (0);
}

struct pflowctl_sockaddr {
	union {
		struct sockaddr_in in;
		struct sockaddr_in6 in6;
		struct sockaddr_storage storage;
	};
};
static bool
pflowctl_post_sockaddr(struct snl_state* ss __unused, void *target)
{
	struct pflowctl_sockaddr *s = (struct pflowctl_sockaddr *)target;

	if (s->storage.ss_family == AF_INET)
		s->storage.ss_len = sizeof(struct sockaddr_in);
	else if (s->storage.ss_family == AF_INET6)
		s->storage.ss_len = sizeof(struct sockaddr_in6);
	else
		return (false);

	return (true);
}
#define _OUT(_field)	offsetof(struct pflowctl_sockaddr, _field)
static struct snl_attr_parser nla_p_sockaddr[] = {
	{ .type = PFLOWNL_ADDR_FAMILY, .off = _OUT(in.sin_family), .cb = snl_attr_get_uint8 },
	{ .type = PFLOWNL_ADDR_PORT, .off = _OUT(in.sin_port), .cb = snl_attr_get_uint16 },
	{ .type = PFLOWNL_ADDR_IP, .off = _OUT(in.sin_addr), .cb = snl_attr_get_in_addr },
	{ .type = PFLOWNL_ADDR_IP6, .off = _OUT(in6.sin6_addr), .cb = snl_attr_get_in6_addr },
};
SNL_DECLARE_ATTR_PARSER_EXT(sockaddr_parser, 0, nla_p_sockaddr, pflowctl_post_sockaddr);
#undef _OUT

struct pflowctl_get {
	int id;
	int version;
	struct pflowctl_sockaddr src;
	struct pflowctl_sockaddr dst;
	uint32_t obs_dom;
	uint8_t so_status;
};
#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct pflowctl_get, _field)
static struct snl_attr_parser ap_get[] = {
	{ .type = PFLOWNL_GET_ID, .off = _OUT(id), .cb = snl_attr_get_int32 },
	{ .type = PFLOWNL_GET_VERSION, .off = _OUT(version), .cb = snl_attr_get_int16 },
	{ .type = PFLOWNL_GET_SRC, .off = _OUT(src), .arg = &sockaddr_parser, .cb = snl_attr_get_nested },
	{ .type = PFLOWNL_GET_DST, .off = _OUT(dst), .arg = &sockaddr_parser, .cb = snl_attr_get_nested },
	{ .type = PFLOWNL_GET_OBSERVATION_DOMAIN, .off = _OUT(obs_dom), .cb = snl_attr_get_uint32 },
	{ .type = PFLOWNL_GET_SOCKET_STATUS, .off = _OUT(so_status), .cb = snl_attr_get_uint8 },
};
static struct snl_field_parser fp_get[] = {};
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER(get_parser, struct genlmsghdr, fp_get, ap_get);

static void
print_sockaddr(const char *prefix, const struct sockaddr_storage *s)
{
	char buf[INET6_ADDRSTRLEN];
	int error;

	if (s->ss_family != AF_INET && s->ss_family != AF_INET6)
		return;

	if (s->ss_family == AF_INET ||
	    s->ss_family == AF_INET6) {
		error = getnameinfo((const struct sockaddr *)s,
		    s->ss_len, buf, sizeof(buf), NULL, 0,
		    NI_NUMERICHOST);
		if (error)
			err(1, "sender: %s", gai_strerror(error));
	}

	printf("%s", prefix);
	switch (s->ss_family) {
	case AF_INET: {
		const struct sockaddr_in *sin = (const struct sockaddr_in *)s;
		if (sin->sin_addr.s_addr != INADDR_ANY) {
			printf("%s", buf);
			if (sin->sin_port != 0)
				printf(":%u", ntohs(sin->sin_port));
		}
		break;
	}
	case AF_INET6: {
		const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)s;
		if (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
			printf("[%s]", buf);
			if (sin6->sin6_port != 0)
				printf(":%u", ntohs(sin6->sin6_port));
		}
		break;
	}
	}
}

static int
get(int id)
{
	struct snl_state ss = {};
	struct snl_errmsg_data e = {};
	struct pflowctl_get g = {};
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	uint32_t seq_id;
	int family_id;

	snl_init(&ss, NETLINK_GENERIC);
	family_id = snl_get_genl_family(&ss, PFLOWNL_FAMILY_NAME);
	if (family_id == 0)
		errx(1, "pflow.ko is not loaded.");

	snl_init_writer(&ss, &nw);
	hdr = snl_create_genl_msg_request(&nw, family_id, PFLOWNL_CMD_GET);
	snl_add_msg_attr_s32(&nw, PFLOWNL_GET_ID, id);

	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL)
		return (ENOMEM);
	seq_id = hdr->nlmsg_seq;

	snl_send_message(&ss, hdr);

	while ((hdr = snl_read_reply_multi(&ss, seq_id, &e)) != NULL) {
		if (! snl_parse_nlmsg(&ss, hdr, &get_parser, &g))
			continue;

		printf("pflow%d: version %d domain %u", g.id, g.version, g.obs_dom);
		print_sockaddr(" src ", &g.src.storage);
		print_sockaddr(" dst ", &g.dst.storage);
		printf("\n");
		if (verbose) {
			printf("\tsocket: %s\n",
			    g.so_status ? "connected" : "disconnected");
		}
	}

	if (e.error)
		errc(1, e.error, "failed to get");

	return (0);
}

struct pflowctl_set {
	int id;
	uint16_t version;
	struct sockaddr_storage src;
	struct sockaddr_storage dst;
	uint32_t obs_dom;
};
static inline bool
snl_add_msg_attr_sockaddr(struct snl_writer *nw, int attrtype, struct sockaddr_storage *s)
{
	int off = snl_add_msg_attr_nested(nw, attrtype);

	snl_add_msg_attr_u8(nw, PFLOWNL_ADDR_FAMILY, s->ss_family);

	switch (s->ss_family) {
	case AF_INET: {
		const struct sockaddr_in *in = (const struct sockaddr_in *)s;
		snl_add_msg_attr_u16(nw, PFLOWNL_ADDR_PORT, in->sin_port);
		snl_add_msg_attr_ip4(nw, PFLOWNL_ADDR_IP, &in->sin_addr);
		break;
	}
	case AF_INET6: {
		const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *)s;
		snl_add_msg_attr_u16(nw, PFLOWNL_ADDR_PORT, in6->sin6_port);
		snl_add_msg_attr_ip6(nw, PFLOWNL_ADDR_IP6, &in6->sin6_addr);
		break;
	}
	default:
		return (false);
	}
	snl_end_attr_nested(nw, off);

	return (true);
}

static int
do_set(struct pflowctl_set *s)
{
	struct snl_state ss = {};
	struct snl_errmsg_data e = {};
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	int family_id;

	snl_init(&ss, NETLINK_GENERIC);
	family_id = snl_get_genl_family(&ss, PFLOWNL_FAMILY_NAME);
	if (family_id == 0)
		errx(1, "pflow.ko is not loaded.");

	snl_init_writer(&ss, &nw);
	snl_create_genl_msg_request(&nw, family_id, PFLOWNL_CMD_SET);

	snl_add_msg_attr_s32(&nw, PFLOWNL_SET_ID, s->id);
	if (s->version != 0)
		snl_add_msg_attr_u16(&nw, PFLOWNL_SET_VERSION, s->version);
	if (s->src.ss_len != 0)
		snl_add_msg_attr_sockaddr(&nw, PFLOWNL_SET_SRC, &s->src);
	if (s->dst.ss_len != 0)
		snl_add_msg_attr_sockaddr(&nw, PFLOWNL_SET_DST, &s->dst);
	if (s->obs_dom != 0)
		snl_add_msg_attr_u32(&nw, PFLOWNL_SET_OBSERVATION_DOMAIN, s->obs_dom);

	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL)
		return (1);

	snl_send_message(&ss, hdr);
	snl_read_reply_code(&ss, hdr->nlmsg_seq, &e);

	if (e.error)
		errc(1, e.error, "failed to set");

	return (0);
}

static void
pflowctl_addr(const char *val, struct sockaddr_storage *ss)
{
	struct addrinfo *res0;
	int error;
	bool flag;
	char *ip, *port;
	char buf[sysconf(_SC_HOST_NAME_MAX) + 1 + sizeof(":65535")];
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_DGRAM, /*dummy*/
		.ai_flags = AI_NUMERICHOST,
	};

	if (strlcpy(buf, val, sizeof(buf)) >= sizeof(buf))
		errx(1, "%s bad value", val);

	port = NULL;
	flag = *buf == '[';

	for (char *cp = buf; *cp; ++cp) {
		if (*cp == ']' && *(cp + 1) == ':' && flag) {
			*cp = '\0';
			*(cp + 1) = '\0';
			port = cp + 2;
			break;
		}
		if (*cp == ']' && *(cp + 1) == '\0' && flag) {
			*cp = '\0';
			port = NULL;
			break;
		}
		if (*cp == ':' && !flag) {
			*cp = '\0';
			port = cp + 1;
			break;
		}
	}

	ip = buf;
	if (flag)
		ip++;

	if ((error = getaddrinfo(ip, port, &hints, &res0)) != 0)
		errx(1, "error in parsing address string: %s",
		    gai_strerror(error));

	memcpy(ss, res0->ai_addr, res0->ai_addr->sa_len);
	freeaddrinfo(res0);
}

static int
set(char *idstr, int argc, char *argv[])
{
	struct pflowctl_set s = {};

	s.id = pflow_to_id(idstr);
	if (s.id < 0)
		return (EINVAL);

	while (argc > 0) {
		if (strcmp(argv[0], "src") == 0) {
			if (argc < 2)
				usage();

			pflowctl_addr(argv[1], &s.src);

			argc -= 2;
			argv += 2;
		} else if (strcmp(argv[0], "dst") == 0) {
			if (argc < 2)
				usage();

			pflowctl_addr(argv[1], &s.dst);

			argc -= 2;
			argv += 2;
		} else if (strcmp(argv[0], "proto") == 0) {
			if (argc < 2)
				usage();

			s.version = strtol(argv[1], NULL, 10);

			argc -= 2;
			argv += 2;
		} else if (strcmp(argv[0], "domain") == 0) {
			if (argc < 2)
				usage();

			s.obs_dom = strtol(argv[1], NULL, 10);

			argc -= 2;
			argv += 2;
		} else {
			usage();
		}
	}

	return (do_set(&s));
}

static const struct snl_hdr_parser *all_parsers[] = {
	&list_parser,
	&get_parser,
};

enum pflowctl_op_t {
	OP_HELP,
	OP_LIST,
	OP_CREATE,
	OP_DELETE,
	OP_SET,
};
int
main(int argc, char *argv[])
{
	int ch;
	enum pflowctl_op_t op = OP_HELP;
	char **set_args = NULL;
	size_t set_arg_count = 0;

	SNL_VERIFY_PARSERS(all_parsers);

	if (argc < 2)
		usage();

	while ((ch = getopt(argc, argv,
	    "lcd:s:v")) != -1) {
		switch (ch) {
		case 'l':
			op = OP_LIST;
			break;
		case 'c':
			op = OP_CREATE;
			break;
		case 'd':
			op = OP_DELETE;
			break;
		case 's':
			op = OP_SET;
			set_arg_count = argc - optind;
			set_args = argv + optind;
		case 'v':
			verbose = true;
			break;
		}
	}

	switch (op) {
	case OP_LIST:
		return (list());
	case OP_CREATE:
		return (create());
	case OP_DELETE:
		return (del(optarg));
	case OP_SET:
		return (set(optarg, set_arg_count, set_args));
	case OP_HELP:
		usage();
		break;
	}

	return (0);
}
