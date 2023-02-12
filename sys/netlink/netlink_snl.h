/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
#ifndef	_NETLINK_NETLINK_SNL_H_
#define	_NETLINK_NETLINK_SNL_H_

/*
 * Simple Netlink Library
 */

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netlink/netlink.h>


#define _roundup2(x, y)         (((x)+((y)-1))&(~((y)-1)))

#define NETLINK_ALIGN_SIZE      sizeof(uint32_t)
#define NETLINK_ALIGN(_len)     _roundup2(_len, NETLINK_ALIGN_SIZE)

#define NLA_ALIGN_SIZE          sizeof(uint32_t)
#define	NLA_HDRLEN		((int)sizeof(struct nlattr))
#define	NLA_DATA_LEN(_nla)	((int)((_nla)->nla_len - NLA_HDRLEN))
#define	NLA_DATA(_nla)		NL_ITEM_DATA(_nla, NLA_HDRLEN)
#define	NLA_DATA_CONST(_nla)	NL_ITEM_DATA_CONST(_nla, NLA_HDRLEN)

#define	NLA_TYPE(_nla)		((_nla)->nla_type & 0x3FFF)

#define NLA_NEXT(_attr) (struct nlattr *)(void *)((char *)_attr + NLA_ALIGN(_attr->nla_len))

#define	_NLA_END(_start, _len)	((char *)(_start) + (_len))
#define NLA_FOREACH(_attr, _start, _len)      \
        for (_attr = (_start);		\
		((char *)_attr < _NLA_END(_start, _len)) && \
		((char *)NLA_NEXT(_attr) <= _NLA_END(_start, _len));	\
		_attr =  NLA_NEXT(_attr))

#define	NL_ARRAY_LEN(_a)	(sizeof(_a) / sizeof((_a)[0]))

struct linear_buffer {
	char		*base;	/* Base allocated memory pointer */
	uint32_t	offset;	/* Currently used offset */
	uint32_t	size;	/* Total buffer size */
};

static inline char *
lb_allocz(struct linear_buffer *lb, int len)
{
	len = roundup2(len, sizeof(uint64_t));
	if (lb->offset + len > lb->size)
		return (NULL);
	void *data = (void *)(lb->base + lb->offset);
	lb->offset += len;
	return (data);
}

static inline void
lb_clear(struct linear_buffer *lb)
{
	memset(lb->base, 0, lb->offset);
	lb->offset = 0;
}

struct snl_state {
	int fd;
	char *buf;
	size_t off;
	size_t bufsize;
	size_t datalen;
	uint32_t seq;
	bool init_done;
	struct linear_buffer lb;
};
#define	SCRATCH_BUFFER_SIZE	1024

typedef void snl_parse_field_f(struct snl_state *ss, void *hdr, void *target);
struct snl_field_parser {
	uint16_t		off_in;
	uint16_t		off_out;
	snl_parse_field_f	*cb;
};

typedef bool snl_parse_attr_f(struct snl_state *ss, struct nlattr *attr,
    const void *arg, void *target);
struct snl_attr_parser {
	uint16_t		type;	/* Attribute type */
	uint16_t		off;	/* field offset in the target structure */
	snl_parse_attr_f	*cb;	/* parser function to call */
	const void		*arg;	/* Optional argument parser */
};

struct snl_hdr_parser {
	int			hdr_off; /* aligned header size */
	int			fp_size;
	int			np_size;
	const struct snl_field_parser	*fp; /* array of header field parsers */
	const struct snl_attr_parser	*np; /* array of attribute parsers */
};

#define	SNL_DECLARE_PARSER(_name, _t, _fp, _np)		\
static const struct snl_hdr_parser _name = {		\
	.hdr_off = sizeof(_t),				\
	.fp = &((_fp)[0]),				\
	.np = &((_np)[0]),				\
	.fp_size = NL_ARRAY_LEN(_fp),			\
	.np_size = NL_ARRAY_LEN(_np),			\
}

#define	SNL_DECLARE_ATTR_PARSER(_name, _np)		\
static const struct snl_hdr_parser _name = {		\
	.np = &((_np)[0]),				\
	.np_size = NL_ARRAY_LEN(_np),			\
}


static void
snl_free(struct snl_state *ss)
{
	if (ss->init_done) {
		close(ss->fd);
		if (ss->buf != NULL)
			free(ss->buf);
		if (ss->lb.base != NULL)
			free(ss->lb.base);
	}
}

static inline bool
snl_init(struct snl_state *ss, int netlink_family)
{
	memset(ss, 0, sizeof(*ss));

	ss->fd = socket(AF_NETLINK, SOCK_RAW, netlink_family);
	if (ss->fd == -1)
		return (false);
	ss->init_done = true;

	int rcvbuf;
	socklen_t optlen = sizeof(rcvbuf);
	if (getsockopt(ss->fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &optlen) == -1) {
		snl_free(ss);
		return (false);
	}

	ss->bufsize = rcvbuf;
	ss->buf = malloc(ss->bufsize);
	if (ss->buf == NULL) {
		snl_free(ss);
		return (false);
	}

	ss->lb.size = SCRATCH_BUFFER_SIZE;
	ss->lb.base = calloc(1, ss->lb.size);
	if (ss->lb.base == NULL) {
		snl_free(ss);
		return (false);
	}

	return (true);
}

static inline void *
snl_allocz(struct snl_state *ss, int len)
{
	return (lb_allocz(&ss->lb, len));
}

static inline void
snl_clear_lb(struct snl_state *ss)
{
	lb_clear(&ss->lb);
}

static inline bool
snl_send(struct snl_state *ss, void *data, int sz)
{
	return (send(ss->fd, data, sz, 0) == sz);
}

static inline uint32_t
snl_get_seq(struct snl_state *ss)
{
	return (++ss->seq);
}

static inline struct nlmsghdr *
snl_read_message(struct snl_state *ss)
{
	if (ss->off == ss->datalen) {
		struct sockaddr_nl nladdr;
		struct iovec iov = {
			.iov_base = ss->buf,
			.iov_len = ss->bufsize,
		};
		struct msghdr msg = {
			.msg_name = &nladdr,
			.msg_namelen = sizeof(nladdr),
			.msg_iov = &iov,
			.msg_iovlen = 1,
		};
		ss->off = 0;
		ss->datalen = 0;
		for (;;) {
			ssize_t datalen = recvmsg(ss->fd, &msg, 0);
			if (datalen > 0) {
				ss->datalen = datalen;
				break;
			} else if (errno != EINTR)
				return (NULL);
		}
	}
	struct nlmsghdr *hdr = (struct nlmsghdr *)(void *)&ss->buf[ss->off];
	ss->off += NLMSG_ALIGN(hdr->nlmsg_len);
	return (hdr);
}

/*
 * Checks that attributes are sorted by attribute type.
 */
static inline void
snl_verify_parsers(const struct snl_hdr_parser **parser, int count)
{
	for (int i = 0; i < count; i++) {
		const struct snl_hdr_parser *p = parser[i];
		int attr_type = 0;
		for (int j = 0; j < p->np_size; j++) {
			assert(p->np[j].type > attr_type);
			attr_type = p->np[j].type;
		}
	}
}
#define	SNL_VERIFY_PARSERS(_p)	snl_verify_parsers((_p), NL_ARRAY_LEN(_p))

static const struct snl_attr_parser *
find_parser(const struct snl_attr_parser *ps, int pslen, int key)
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

static inline bool
snl_parse_attrs_raw(struct snl_state *ss, struct nlattr *nla_head, int len,
    const struct snl_attr_parser *ps, int pslen, void *target)
{
	struct nlattr *nla;

	NLA_FOREACH(nla, nla_head, len) {
		if (nla->nla_len < sizeof(struct nlattr))
			return (false);
		int nla_type = nla->nla_type & NLA_TYPE_MASK;
		const struct snl_attr_parser *s = find_parser(ps, pslen, nla_type);
		if (s != NULL) {
			void *ptr = (void *)((char *)target + s->off);
			if (!s->cb(ss, nla, s->arg, ptr))
				return (false);
		}
	}
	return (true);
}

static inline bool
snl_parse_attrs(struct snl_state *ss, struct nlmsghdr *hdr, int hdrlen,
    const struct snl_attr_parser *ps, int pslen, void *target)
{
	int off = NLMSG_HDRLEN + NETLINK_ALIGN(hdrlen);
	int len = hdr->nlmsg_len - off;
	struct nlattr *nla_head = (struct nlattr *)(void *)((char *)hdr + off);

	return (snl_parse_attrs_raw(ss, nla_head, len, ps, pslen, target));
}

static inline bool
snl_parse_header(struct snl_state *ss, void *hdr, int len,
    const struct snl_hdr_parser *parser, void *target)
{
	/* Extract fields first (if any) */
	for (int i = 0; i < parser->fp_size; i++) {
		const struct snl_field_parser *fp = &parser->fp[i];
		void *src = (char *)hdr + fp->off_in;
		void *dst = (char *)target + fp->off_out;

		fp->cb(ss, src, dst);
	}

	struct nlattr *nla_head = (struct nlattr *)(void *)((char *)hdr + parser->hdr_off);
	bool result = snl_parse_attrs_raw(ss, nla_head, len - parser->hdr_off,
	    parser->np, parser->np_size, target);

	return (result);
}

static inline bool
snl_parse_nlmsg(struct snl_state *ss, struct nlmsghdr *hdr,
    const struct snl_hdr_parser *parser, void *target)
{
	return (snl_parse_header(ss, hdr + 1, hdr->nlmsg_len - sizeof(*hdr), parser, target));
}

static inline bool
snl_attr_get_flag(struct snl_state *ss __unused, struct nlattr *nla, void *target)
{
	if (NLA_DATA_LEN(nla) == 0) {
		*((uint8_t *)target) = 1;
		return (true);
	}
	return (false);
}

static inline bool
snl_attr_get_uint16(struct snl_state *ss __unused, struct nlattr *nla,
    const void *arg __unused, void *target)
{
	if (NLA_DATA_LEN(nla) == sizeof(uint16_t)) {
		*((uint16_t *)target) = *((const uint16_t *)NLA_DATA_CONST(nla));
		return (true);
	}
	return (false);
}

static inline bool
snl_attr_get_uint32(struct snl_state *ss __unused, struct nlattr *nla,
    const void *arg __unused, void *target)
{
	if (NLA_DATA_LEN(nla) == sizeof(uint32_t)) {
		*((uint32_t *)target) = *((const uint32_t *)NLA_DATA_CONST(nla));
		return (true);
	}
	return (false);
}

static inline bool
snl_attr_get_string(struct snl_state *ss __unused, struct nlattr *nla,
    const void *arg __unused, void *target)
{
	size_t maxlen = NLA_DATA_LEN(nla);

	if (strnlen((char *)NLA_DATA(nla), maxlen) < maxlen) {
		*((char **)target) = (char *)NLA_DATA(nla);
		return (true);
	}
	return (false);
}

static inline bool
snl_attr_get_stringn(struct snl_state *ss, struct nlattr *nla,
    const void *arg __unused, void *target)
{
	int maxlen = NLA_DATA_LEN(nla);

	char *buf = snl_allocz(ss, maxlen + 1);
	if (buf == NULL)
		return (false);
	buf[maxlen] = '\0';
	memcpy(buf, NLA_DATA(nla), maxlen);

	*((char **)target) = buf;
	return (true);
}

static inline bool
snl_attr_get_nested(struct snl_state *ss, struct nlattr *nla, const void *arg, void *target)
{
	const struct snl_hdr_parser *p = (const struct snl_hdr_parser *)arg;

	/* Assumes target points to the beginning of the structure */
	return (snl_parse_header(ss, NLA_DATA(nla), NLA_DATA_LEN(nla), p, target));
}

static inline bool
snl_attr_get_nla(struct snl_state *ss __unused, struct nlattr *nla, void *target)
{
	*((struct nlattr **)target) = nla;
	return (true);
}

static inline void
snl_field_get_uint8(struct snl_state *ss __unused, void *src, void *target)
{
	*((uint8_t *)target) = *((uint8_t *)src);
}

static inline void
snl_field_get_uint16(struct snl_state *ss __unused, void *src, void *target)
{
	*((uint16_t *)target) = *((uint16_t *)src);
}

static inline void
snl_field_get_uint32(struct snl_state *ss __unused, void *src, void *target)
{
	*((uint32_t *)target) = *((uint32_t *)src);
}

#endif
