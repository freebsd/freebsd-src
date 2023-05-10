/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Alexander V. Chernikov <melifaro@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include "opt_inet.h"
#include "opt_inet6.h"
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/rmlock.h>
#include <sys/socket.h>

#include <machine/stdarg.h>

#include <net/if.h>
#include <net/route.h>
#include <net/route/nhop.h>

#include <net/route/route_ctl.h>
#include <netinet/in.h>
#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_var.h>
#include <netlink/netlink_route.h>

#define	DEBUG_MOD_NAME	nl_parser
#define	DEBUG_MAX_LEVEL	LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_INFO);

bool
nlmsg_report_err_msg(struct nl_pstate *npt, const char *fmt, ...)
{
	va_list ap;

	if (npt->err_msg != NULL)
		return (false);
	char *buf = npt_alloc(npt, NL_MAX_ERROR_BUF);
	if (buf == NULL)
		return (false);
	va_start(ap, fmt);
	vsnprintf(buf, NL_MAX_ERROR_BUF, fmt, ap);
	va_end(ap);

	npt->err_msg = buf;
	return (true);
}

bool
nlmsg_report_err_offset(struct nl_pstate *npt, uint32_t off)
{
	if (npt->err_off != 0)
		return (false);
	npt->err_off = off;
	return (true);
}

void
nlmsg_report_cookie(struct nl_pstate *npt, struct nlattr *nla)
{
	MPASS(nla->nla_type == NLMSGERR_ATTR_COOKIE);
	MPASS(nla->nla_len >= sizeof(struct nlattr));
	npt->cookie = nla;
}

void
nlmsg_report_cookie_u32(struct nl_pstate *npt, uint32_t val)
{
	struct nlattr *nla = npt_alloc(npt, sizeof(*nla) + sizeof(uint32_t));

	nla->nla_type = NLMSGERR_ATTR_COOKIE;
	nla->nla_len = sizeof(*nla) + sizeof(uint32_t);
	memcpy(nla + 1, &val, sizeof(uint32_t));
	nlmsg_report_cookie(npt, nla);
}

static const struct nlattr_parser *
search_states(const struct nlattr_parser *ps, int pslen, int key)
{
	int left_i = 0, right_i = pslen - 1;

	if (key < ps[0].type || key > ps[pslen - 1].type)
		return (NULL);

	while (left_i + 1 < right_i) {
		int mid_i = (left_i + right_i) / 2;
		if (key < ps[mid_i].type)
			right_i = mid_i;
		else if (key > ps[mid_i].type)
			left_i = mid_i + 1;
		else
			return (&ps[mid_i]);
	}
	if (ps[left_i].type == key)
		return (&ps[left_i]);
	else if (ps[right_i].type == key)
		return (&ps[right_i]);
	return (NULL);
}

int
nl_parse_attrs_raw(struct nlattr *nla_head, int len, const struct nlattr_parser *ps, int pslen,
    struct nl_pstate *npt, void *target)
{
	struct nlattr *nla = NULL;
	int error = 0;

	NL_LOG(LOG_DEBUG3, "parse %p remaining_len %d", nla_head, len);
	int orig_len = len;
	NLA_FOREACH(nla, nla_head, len) {
		NL_LOG(LOG_DEBUG3, ">> parsing %p attr_type %d len %d (rem %d)", nla, nla->nla_type, nla->nla_len, len);
		if (nla->nla_len < sizeof(struct nlattr)) {
			NLMSG_REPORT_ERR_MSG(npt, "Invalid attr %p type %d len: %d",
			    nla, nla->nla_type, nla->nla_len);
			uint32_t off = (char *)nla - (char *)npt->hdr;
			nlmsg_report_err_offset(npt, off);
			return (EINVAL);
		}

		int nla_type = nla->nla_type & NLA_TYPE_MASK;
		const struct nlattr_parser *s = search_states(ps, pslen, nla_type);
		if (s != NULL) {
			void *ptr = (void *)((char *)target + s->off);
			error = s->cb(nla, npt, s->arg, ptr);
			if (error != 0) {
				uint32_t off = (char *)nla - (char *)npt->hdr;
				nlmsg_report_err_offset(npt, off);
				NL_LOG(LOG_DEBUG3, "parse failed att offset %u", off);
				return (error);
			}
		} else {
			/* Ignore non-specified attributes */
			NL_LOG(LOG_DEBUG3, "ignoring attr %d", nla->nla_type);
		}
	}
	if (len >= sizeof(struct nlattr)) {
		nla = (struct nlattr *)((char *)nla_head + (orig_len - len));
		NL_LOG(LOG_DEBUG3, " >>> end %p attr_type %d len %d", nla,
		    nla->nla_type, nla->nla_len);
	}
	NL_LOG(LOG_DEBUG3, "end parse: %p remaining_len %d", nla, len);

	return (0);
}

void
nl_get_attrs_bmask_raw(struct nlattr *nla_head, int len, struct nlattr_bmask *bm)
{
	struct nlattr *nla = NULL;

	BIT_ZERO(NL_ATTR_BMASK_SIZE, bm);

	NLA_FOREACH(nla, nla_head, len) {
		if (nla->nla_len < sizeof(struct nlattr))
			return;
		int nla_type = nla->nla_type & NLA_TYPE_MASK;
		if (nla_type < NL_ATTR_BMASK_SIZE)
			BIT_SET(NL_ATTR_BMASK_SIZE, nla_type, bm);
		else
			NL_LOG(LOG_DEBUG2, "Skipping type %d in the mask: too short",
			    nla_type);
	}
}

bool
nl_has_attr(const struct nlattr_bmask *bm, unsigned int nla_type)
{
	MPASS(nla_type < NL_ATTR_BMASK_SIZE);

	return (BIT_ISSET(NL_ATTR_BMASK_SIZE, nla_type, bm));
}

int
nlattr_get_flag(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	if (__predict_false(NLA_DATA_LEN(nla) != 0)) {
		NLMSG_REPORT_ERR_MSG(npt, "nla type %d size(%u) is not a flag",
		    nla->nla_type, NLA_DATA_LEN(nla));
		return (EINVAL);
	}

	*((uint8_t *)target) = 1;
	return (0);
}

static struct sockaddr *
parse_rta_ip4(void *rta_data, struct nl_pstate *npt, int *perror)
{
	struct sockaddr_in *sin;

	sin = (struct sockaddr_in *)npt_alloc_sockaddr(npt, sizeof(struct sockaddr_in));
	if (__predict_false(sin == NULL)) {
		*perror = ENOBUFS;
		return (NULL);
	}
	sin->sin_len = sizeof(struct sockaddr_in);
	sin->sin_family = AF_INET;
	memcpy(&sin->sin_addr, rta_data, sizeof(struct in_addr));
	return ((struct sockaddr *)sin);
}

static struct sockaddr *
parse_rta_ip6(void *rta_data, struct nl_pstate *npt, int *perror)
{
	struct sockaddr_in6 *sin6;

	sin6 = (struct sockaddr_in6 *)npt_alloc_sockaddr(npt, sizeof(struct sockaddr_in6));
	if (__predict_false(sin6 == NULL)) {
		*perror = ENOBUFS;
		return (NULL);
	}
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	sin6->sin6_family = AF_INET6;
	memcpy(&sin6->sin6_addr, rta_data, sizeof(struct in6_addr));
	return ((struct sockaddr *)sin6);
}

static struct sockaddr *
parse_rta_ip(struct rtattr *rta, struct nl_pstate *npt, int *perror)
{
	void *rta_data = NL_RTA_DATA(rta);
	int rta_len = NL_RTA_DATA_LEN(rta);

	if (rta_len == sizeof(struct in_addr)) {
		return (parse_rta_ip4(rta_data, npt, perror));
	} else if (rta_len == sizeof(struct in6_addr)) {
		return (parse_rta_ip6(rta_data, npt, perror));
	} else {
		NLMSG_REPORT_ERR_MSG(npt, "unknown IP len: %d for rta type %d",
		    rta_len, rta->rta_type);
		*perror = ENOTSUP;
		return (NULL);
	}
	return (NULL);
}

int
nlattr_get_ip(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	int error = 0;

	struct sockaddr *sa = parse_rta_ip((struct rtattr *)nla, npt, &error);

	*((struct sockaddr **)target) = sa;
	return (error);
}

static struct sockaddr *
parse_rta_via(struct rtattr *rta, struct nl_pstate *npt, int *perror)
{
	struct rtvia *via = NL_RTA_DATA(rta);
	int data_len = NL_RTA_DATA_LEN(rta);

	if (__predict_false(data_len) < sizeof(struct rtvia)) {
		NLMSG_REPORT_ERR_MSG(npt, "undersized RTA_VIA(%d) attr: len %d",
		    rta->rta_type, data_len);
		*perror = EINVAL;
		return (NULL);
	}
	data_len -= offsetof(struct rtvia, rtvia_addr);

	switch (via->rtvia_family) {
	case AF_INET:
		if (__predict_false(data_len < sizeof(struct in_addr))) {
			*perror = EINVAL;
			return (NULL);
		}
		return (parse_rta_ip4(via->rtvia_addr, npt, perror));
	case AF_INET6:
		if (__predict_false(data_len < sizeof(struct in6_addr))) {
			*perror = EINVAL;
			return (NULL);
		}
		return (parse_rta_ip6(via->rtvia_addr, npt, perror));
	default:
		*perror = ENOTSUP;
		return (NULL);
	}
}

int
nlattr_get_ipvia(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	int error = 0;

	struct sockaddr *sa = parse_rta_via((struct rtattr *)nla, npt, &error);

	*((struct sockaddr **)target) = sa;
	return (error);
}


int
nlattr_get_uint8(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	if (__predict_false(NLA_DATA_LEN(nla) != sizeof(uint8_t))) {
		NLMSG_REPORT_ERR_MSG(npt, "nla type %d size(%u) is not uint8",
		    nla->nla_type, NLA_DATA_LEN(nla));
		return (EINVAL);
	}
	*((uint16_t *)target) = *((const uint16_t *)NL_RTA_DATA_CONST(nla));
	return (0);
}

int
nlattr_get_uint16(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	if (__predict_false(NLA_DATA_LEN(nla) != sizeof(uint16_t))) {
		NLMSG_REPORT_ERR_MSG(npt, "nla type %d size(%u) is not uint16",
		    nla->nla_type, NLA_DATA_LEN(nla));
		return (EINVAL);
	}
	*((uint16_t *)target) = *((const uint16_t *)NL_RTA_DATA_CONST(nla));
	return (0);
}

int
nlattr_get_uint32(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	if (__predict_false(NLA_DATA_LEN(nla) != sizeof(uint32_t))) {
		NLMSG_REPORT_ERR_MSG(npt, "nla type %d size(%u) is not uint32",
		    nla->nla_type, NLA_DATA_LEN(nla));
		return (EINVAL);
	}
	*((uint32_t *)target) = *((const uint32_t *)NL_RTA_DATA_CONST(nla));
	return (0);
}

int
nlattr_get_uint64(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	if (__predict_false(NLA_DATA_LEN(nla) != sizeof(uint64_t))) {
		NLMSG_REPORT_ERR_MSG(npt, "nla type %d size(%u) is not uint64",
		    nla->nla_type, NLA_DATA_LEN(nla));
		return (EINVAL);
	}
	memcpy(target, NL_RTA_DATA_CONST(nla), sizeof(uint64_t));
	return (0);
}

int
nlattr_get_in_addr(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	if (__predict_false(NLA_DATA_LEN(nla) != sizeof(in_addr_t))) {
		NLMSG_REPORT_ERR_MSG(npt, "nla type %d size(%u) is not in_addr_t",
		    nla->nla_type, NLA_DATA_LEN(nla));
		return (EINVAL);
	}
	memcpy(target, NLA_DATA_CONST(nla), sizeof(in_addr_t));
	return (0);
}

int
nlattr_get_in6_addr(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	if (__predict_false(NLA_DATA_LEN(nla) != sizeof(struct in6_addr))) {
		NLMSG_REPORT_ERR_MSG(npt, "nla type %d size(%u) is not struct in6_addr",
		    nla->nla_type, NLA_DATA_LEN(nla));
		return (EINVAL);
	}
	memcpy(target, NLA_DATA_CONST(nla), sizeof(struct in6_addr));
	return (0);
}

static int
nlattr_get_ifp_internal(struct nlattr *nla, struct nl_pstate *npt,
    void *target, bool zero_ok)
{
	if (__predict_false(NLA_DATA_LEN(nla) != sizeof(uint32_t))) {
		NLMSG_REPORT_ERR_MSG(npt, "nla type %d size(%u) is not uint32",
		    nla->nla_type, NLA_DATA_LEN(nla));
		return (EINVAL);
	}
	uint32_t ifindex = *((const uint32_t *)NLA_DATA_CONST(nla));

	if (ifindex == 0 && zero_ok) {
		*((struct ifnet **)target) = NULL;
		return (0);
	}

	NET_EPOCH_ASSERT();

	struct ifnet *ifp = ifnet_byindex(ifindex);
	if (__predict_false(ifp == NULL)) {
		NLMSG_REPORT_ERR_MSG(npt, "nla type %d: ifindex %u invalid",
		    nla->nla_type, ifindex);
		return (ENOENT);
	}
	*((struct ifnet **)target) = ifp;
	NL_LOG(LOG_DEBUG3, "nla type %d: ifindex %u -> %s", nla->nla_type,
	    ifindex, if_name(ifp));

	return (0);
}

int
nlattr_get_ifp(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	return (nlattr_get_ifp_internal(nla, npt, target, false));
}

int
nlattr_get_ifpz(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	return (nlattr_get_ifp_internal(nla, npt, target, true));
}

int
nlattr_get_string(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	int maxlen = NLA_DATA_LEN(nla);

	if (__predict_false(strnlen((char *)NLA_DATA(nla), maxlen) >= maxlen)) {
		NLMSG_REPORT_ERR_MSG(npt, "nla type %d size(%u) is not NULL-terminated",
		    nla->nla_type, maxlen);
		return (EINVAL);
	}

	*((char **)target) = (char *)NLA_DATA(nla);
	return (0);
}

int
nlattr_get_stringn(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	int maxlen = NLA_DATA_LEN(nla);

	char *buf = npt_alloc(npt, maxlen + 1);
	if (buf == NULL)
		return (ENOMEM);
	buf[maxlen] = '\0';
	memcpy(buf, NLA_DATA(nla), maxlen);

	*((char **)target) = buf;
	return (0);
}
int
nlattr_get_nla(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	NL_LOG(LOG_DEBUG3, "STORING %p len %d", nla, nla->nla_len);
	*((struct nlattr **)target) = nla;
	return (0);
}

int
nlattr_get_nested(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	const struct nlhdr_parser *p = (const struct nlhdr_parser *)arg;
	int error;

	/* Assumes target points to the beginning of the structure */
	error = nl_parse_header(NLA_DATA(nla), NLA_DATA_LEN(nla), p, npt, target);
	return (error);
}

int
nlf_get_ifp(void *src, struct nl_pstate *npt, void *target)
{
	int ifindex = *((const int *)src);

	NET_EPOCH_ASSERT();

	struct ifnet *ifp = ifnet_byindex(ifindex);
	if (ifp == NULL) {
		NL_LOG(LOG_DEBUG, "ifindex %u invalid", ifindex);
		return (ENOENT);
	}
	*((struct ifnet **)target) = ifp;

	return (0);
}

int
nlf_get_ifpz(void *src, struct nl_pstate *npt, void *target)
{
	int ifindex = *((const int *)src);

	NET_EPOCH_ASSERT();

	struct ifnet *ifp = ifnet_byindex(ifindex);
	if (ifindex != 0 && ifp == NULL) {
		NL_LOG(LOG_DEBUG, "ifindex %u invalid", ifindex);
		return (ENOENT);
	}
	*((struct ifnet **)target) = ifp;

	return (0);
}

int
nlf_get_u8(void *src, struct nl_pstate *npt, void *target)
{
	uint8_t val = *((const uint8_t *)src);

	*((uint8_t *)target) = val;

	return (0);
}

int
nlf_get_u8_u32(void *src, struct nl_pstate *npt, void *target)
{
	*((uint32_t *)target) = *((const uint8_t *)src);
	return (0);
}

int
nlf_get_u16(void *src, struct nl_pstate *npt, void *target)
{
	*((uint16_t *)target) = *((const uint16_t *)src);
	return (0);
}

int
nlf_get_u32(void *src, struct nl_pstate *npt, void *target)
{
	*((uint32_t *)target) = *((const uint32_t *)src);
	return (0);
}

