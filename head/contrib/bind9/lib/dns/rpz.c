/*
 * Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: rpz.c,v 1.7 2011-01-17 04:27:23 marka Exp $ */

/*! \file */

#include <config.h>

#include <isc/buffer.h>
#include <isc/mem.h>
#include <isc/net.h>
#include <isc/netaddr.h>
#include <isc/print.h>
#include <isc/stdlib.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/fixedname.h>
#include <dns/log.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdatastruct.h>
#include <dns/result.h>
#include <dns/rpz.h>
#include <dns/view.h>


/*
 * Parallel radix trees for databases of response policy IP addresses
 *
 * The radix or Patricia trees are somewhat specialized to handle response
 * policy addresses by representing the two test of IP IP addresses and name
 * server IP addresses in a single tree.
 *
 * Each leaf indicates that an IP address is listed in the IP address or the
 * name server IP address policy sub-zone (or both) of the corresponding
 * response response zone.  The policy data such as a CNAME or an A record
 * is kept in the policy zone.  After an IP address has been found in a radix
 * tree, the node in the policy zone's database is found by converting
 * the IP address to a domain name in a canonical form.
 *
 * The response policy zone canonical form of IPv6 addresses is one of:
 *	prefix.W.W.W.W.W.W.W.W
 *	prefix.WORDS.zz
 *	prefix.WORDS.zz.WORDS
 *	prefix.zz.WORDS
 *  where
 *	prefix	is the prefix length of the IPv6 address between 1 and 128
 *	W	is a number between 0 and 65535
 *	WORDS	is one or more numbers W separated with "."
 *	zz	corresponds to :: in the standard IPv6 text representation
 *
 * The canonical form of IPv4 addresses is:
 *	prefix.B.B.B.B
 *  where
 *	prefix	is the prefix length of the address between 1 and 32
 *	B	is a number between 0 and 255
 *
 * IPv4 addresses are distinguished from IPv6 addresses by having
 * 5 labels all of which are numbers, and a prefix between 1 and 32.
 */


/*
 * Use a private definition of IPv6 addresses because s6_addr32 is not
 * always defined and our IPv6 addresses are in non-standard byte order
 */
typedef isc_uint32_t		dns_rpz_cidr_word_t;
#define DNS_RPZ_CIDR_WORD_BITS	((int)sizeof(dns_rpz_cidr_word_t)*8)
#define DNS_RPZ_CIDR_KEY_BITS	((int)sizeof(dns_rpz_cidr_key_t)*8)
#define DNS_RPZ_CIDR_WORDS	(128/DNS_RPZ_CIDR_WORD_BITS)
typedef struct {
	dns_rpz_cidr_word_t	w[DNS_RPZ_CIDR_WORDS];
} dns_rpz_cidr_key_t;

#define ADDR_V4MAPPED		0xffff

#define DNS_RPZ_WORD_MASK(b)				\
	((b) == 0 ? (dns_rpz_cidr_word_t)(-1)		\
		  : ((dns_rpz_cidr_word_t)(-1)		\
		    << (DNS_RPZ_CIDR_WORD_BITS - (b))))

#define DNS_RPZ_IP_BIT(ip, bitno) \
	(1 & ((ip)->w[(bitno)/DNS_RPZ_CIDR_WORD_BITS] >> \
	    (DNS_RPZ_CIDR_WORD_BITS - 1 - ((bitno) % DNS_RPZ_CIDR_WORD_BITS))))

typedef struct dns_rpz_cidr_node	dns_rpz_cidr_node_t;
typedef isc_uint8_t			dns_rpz_cidr_flags_t;
struct dns_rpz_cidr_node {
	dns_rpz_cidr_node_t		*parent;
	dns_rpz_cidr_node_t		*child[2];
	dns_rpz_cidr_key_t		ip;
	dns_rpz_cidr_bits_t		bits;
	dns_rpz_cidr_flags_t		flags;
#define	DNS_RPZ_CIDR_FG_IP	 0x01	/* has IP data or is parent of IP */
#define	DNS_RPZ_CIDR_FG_IP_DATA	 0x02	/* has IP data */
#define	DNS_RPZ_CIDR_FG_NSIPv4	 0x04	/* has or is parent of NSIPv4 data */
#define	DNS_RPZ_CIDR_FG_NSIPv6	 0x08	/* has or is parent of NSIPv6 data */
#define	DNS_RPZ_CIDR_FG_NSIP_DATA 0x10	/* has NSIP data */
};

struct dns_rpz_cidr {
	isc_mem_t		*mctx;
	isc_boolean_t		had_nsdname;
	dns_rpz_cidr_node_t	*root;
	dns_name_t		ip_name;        /* RPZ_IP_ZONE.LOCALHOST. */
	dns_name_t		nsip_name;      /* RPZ_NSIP_ZONE.LOCALHOST. */
	dns_name_t		nsdname_name;	/* RPZ_NSDNAME_ZONE.LOCALHOST */
};


static isc_boolean_t		have_rpz_zones = ISC_FALSE;


const char *
dns_rpz_type2str(dns_rpz_type_t type)
{
	switch (type) {
	case DNS_RPZ_TYPE_QNAME:
		return ("QNAME");
	case DNS_RPZ_TYPE_IP:
		return ("IP");
	case DNS_RPZ_TYPE_NSIP:
		return ("NSIP");
	case DNS_RPZ_TYPE_NSDNAME:
		return ("NSDNAME");
	case DNS_RPZ_TYPE_BAD:
		break;
	}
	FATAL_ERROR(__FILE__, __LINE__,
		    "impossible response policy zone type %d", type);
	return ("impossible");
}



dns_rpz_policy_t
dns_rpz_str2policy(const char *str)
{
	if (str == NULL)
		return (DNS_RPZ_POLICY_ERROR);
	if (!strcasecmp(str, "given"))
		return (DNS_RPZ_POLICY_GIVEN);
	if (!strcasecmp(str, "no-op"))
		return (DNS_RPZ_POLICY_NO_OP);
	if (!strcasecmp(str, "nxdomain"))
		return (DNS_RPZ_POLICY_NXDOMAIN);
	if (!strcasecmp(str, "nodata"))
		return (DNS_RPZ_POLICY_NODATA);
	if (!strcasecmp(str, "cname"))
		return (DNS_RPZ_POLICY_CNAME);
	return (DNS_RPZ_POLICY_ERROR);
}



/*
 * Free the radix tree of a response policy database.
 */
void
dns_rpz_cidr_free(dns_rpz_cidr_t **cidrp) {
	dns_rpz_cidr_node_t *cur, *child, *parent;
	dns_rpz_cidr_t *cidr;

	REQUIRE(cidrp != NULL);

	cidr = *cidrp;
	if (cidr == NULL)
		return;

	cur = cidr->root;
	while (cur != NULL) {
		/* Depth first. */
		child = cur->child[0];
		if (child != NULL) {
			cur = child;
			continue;
		}
		child = cur->child[1];
		if (child != NULL) {
			cur = child;
			continue;
		}

		/* Delete this leaf and go up. */
		parent = cur->parent;
		if (parent == NULL)
			cidr->root = NULL;
		else
			parent->child[parent->child[1] == cur] = NULL;
		isc_mem_put(cidr->mctx, cur, sizeof(*cur));
		cur = parent;
	}

	dns_name_free(&cidr->ip_name, cidr->mctx);
	dns_name_free(&cidr->nsip_name, cidr->mctx);
	dns_name_free(&cidr->nsdname_name, cidr->mctx);
	isc_mem_put(cidr->mctx, cidr, sizeof(*cidr));
	*cidrp = NULL;
}



/*
 * Forget a view's list of policy zones.
 */
void
dns_rpz_view_destroy(dns_view_t *view) {
	dns_rpz_zone_t *zone;

	REQUIRE(view != NULL);

	while (!ISC_LIST_EMPTY(view->rpz_zones)) {
		zone = ISC_LIST_HEAD(view->rpz_zones);
		ISC_LIST_UNLINK(view->rpz_zones, zone, link);
		if (dns_name_dynamic(&zone->origin))
			dns_name_free(&zone->origin, view->mctx);
		if (dns_name_dynamic(&zone->nsdname))
			dns_name_free(&zone->nsdname, view->mctx);
		if (dns_name_dynamic(&zone->cname))
			dns_name_free(&zone->cname, view->mctx);
		isc_mem_put(view->mctx, zone, sizeof(*zone));
	}
}

/*
 * Note that we have at least one response policy zone.
 * It would be better for something to tell the rbtdb code that the
 * zone is in at least one view's list of policy zones.
 */
void
dns_rpz_set_need(isc_boolean_t need)
{
	have_rpz_zones = need;
}


isc_boolean_t
dns_rpz_needed(void)
{
	return (have_rpz_zones);
}



/*
 * Start a new radix tree for a response policy zone.
 */
isc_result_t
dns_rpz_new_cidr(isc_mem_t *mctx, dns_name_t *origin,
		 dns_rpz_cidr_t **rbtdb_cidr)
{
	isc_result_t result;
	dns_rpz_cidr_t *cidr;

	REQUIRE(rbtdb_cidr != NULL && *rbtdb_cidr == NULL);

	/*
	 * Only if there is at least one response policy zone.
	 */
	if (!have_rpz_zones)
		return (ISC_R_SUCCESS);

	cidr = isc_mem_get(mctx, sizeof(*cidr));
	if (cidr == NULL)
		return (ISC_R_NOMEMORY);
	memset(cidr, 0, sizeof(*cidr));
	cidr->mctx = mctx;

	dns_name_init(&cidr->ip_name, NULL);
	result = dns_name_fromstring2(&cidr->ip_name, DNS_RPZ_IP_ZONE, origin,
				      DNS_NAME_DOWNCASE, mctx);
	if (result != ISC_R_SUCCESS) {
		isc_mem_put(mctx, cidr, sizeof(*cidr));
		return (result);
	}

	dns_name_init(&cidr->nsip_name, NULL);
	result = dns_name_fromstring2(&cidr->nsip_name, DNS_RPZ_NSIP_ZONE,
				      origin, DNS_NAME_DOWNCASE, mctx);
	if (result != ISC_R_SUCCESS) {
		dns_name_free(&cidr->ip_name, mctx);
		isc_mem_put(mctx, cidr, sizeof(*cidr));
		return (result);
	}

	dns_name_init(&cidr->nsdname_name, NULL);
	result = dns_name_fromstring2(&cidr->nsdname_name, DNS_RPZ_NSDNAME_ZONE,
				      origin, DNS_NAME_DOWNCASE, mctx);
	if (result != ISC_R_SUCCESS) {
		dns_name_free(&cidr->nsip_name, mctx);
		dns_name_free(&cidr->ip_name, mctx);
		isc_mem_put(mctx, cidr, sizeof(*cidr));
		return (result);
	}

	*rbtdb_cidr = cidr;
	return (ISC_R_SUCCESS);
}


/*
 * See if a policy zone has IP, NSIP, or NSDNAME rules or records.
 */
void
dns_rpz_enabled(dns_rpz_cidr_t *cidr, dns_rpz_st_t *st) {
	if (cidr->root != NULL &&
	    (cidr->root->flags & DNS_RPZ_CIDR_FG_IP) != 0)
		st->state |= DNS_RPZ_HAVE_IP;
	if (cidr->root != NULL &&
	    (cidr->root->flags & DNS_RPZ_CIDR_FG_NSIPv4) != 0)
		st->state |= DNS_RPZ_HAVE_NSIPv4;
	if (cidr->root != NULL &&
	    (cidr->root->flags & DNS_RPZ_CIDR_FG_NSIPv6) != 0)
		st->state |= DNS_RPZ_HAVE_NSIPv6;
	if (cidr->had_nsdname)
		st->state |= DNS_RPZ_HAD_NSDNAME;
}

static inline dns_rpz_cidr_flags_t
get_flags(const dns_rpz_cidr_key_t *ip, dns_rpz_cidr_bits_t prefix,
	dns_rpz_type_t rpz_type)
{
	if (rpz_type == DNS_RPZ_TYPE_NSIP) {
		if (prefix >= 96 &&
		    ip->w[0] == 0 && ip->w[1] == 0 &&
		    ip->w[2] == ADDR_V4MAPPED)
			return (DNS_RPZ_CIDR_FG_NSIP_DATA |
				DNS_RPZ_CIDR_FG_NSIPv4);
		else
			return (DNS_RPZ_CIDR_FG_NSIP_DATA |
				DNS_RPZ_CIDR_FG_NSIPv6);
	} else {
		return (DNS_RPZ_CIDR_FG_IP | DNS_RPZ_CIDR_FG_IP_DATA);
	}
}



/*
 * Mark a node as having IP or NSIP data and all of its parents
 * as members of the IP or NSIP tree.
 */
static void
set_node_flags(dns_rpz_cidr_node_t *node, dns_rpz_type_t rpz_type) {
	dns_rpz_cidr_flags_t flags;

	flags = get_flags(&node->ip, node->bits, rpz_type);
	node->flags |= flags;
	flags &= ~(DNS_RPZ_CIDR_FG_NSIP_DATA | DNS_RPZ_CIDR_FG_IP_DATA);
	for (;;) {
		node = node->parent;
		if (node == NULL)
			return;
		node->flags |= flags;
	}
}



/*
 * Make a radix tree node.
 */
static dns_rpz_cidr_node_t *
new_node(dns_rpz_cidr_t *cidr, const dns_rpz_cidr_key_t *ip,
	 dns_rpz_cidr_bits_t bits, dns_rpz_cidr_flags_t flags)
{
	dns_rpz_cidr_node_t *node;
	int i, words, wlen;

	node = isc_mem_get(cidr->mctx, sizeof(*node));
	if (node == NULL)
		return (NULL);
	memset(node, 0, sizeof(*node));

	node->flags = flags & ~(DNS_RPZ_CIDR_FG_IP_DATA |
				DNS_RPZ_CIDR_FG_NSIP_DATA);

	node->bits = bits;
	words = bits / DNS_RPZ_CIDR_WORD_BITS;
	wlen = bits % DNS_RPZ_CIDR_WORD_BITS;
	i = 0;
	while (i < words) {
		node->ip.w[i] = ip->w[i];
		++i;
	}
	if (wlen != 0) {
		node->ip.w[i] = ip->w[i] & DNS_RPZ_WORD_MASK(wlen);
		++i;
	}
	while (i < DNS_RPZ_CIDR_WORDS)
		node->ip.w[i++] = 0;

	return (node);
}



static void
badname(int level, dns_name_t *name, const char *comment)
{
	char printname[DNS_NAME_FORMATSIZE];

	if (isc_log_wouldlog(dns_lctx, level)) {
		dns_name_format(name, printname, sizeof(printname));
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_RBTDB, level,
			      "invalid response policy name \"%s\"%s",
			      printname, comment);
	}
}



/*
 * Convert an IP address from radix tree binary (host byte order) to
 * to its canonical response policy domain name and its name in the
 * policy zone.
 */
static isc_result_t
ip2name(dns_rpz_cidr_t *cidr, const dns_rpz_cidr_key_t *tgt_ip,
	dns_rpz_cidr_bits_t tgt_prefix, dns_rpz_type_t type,
	dns_name_t *canon_name, dns_name_t *search_name)
{
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif
	int w[DNS_RPZ_CIDR_WORDS*2];
	char str[1+8+1+INET6_ADDRSTRLEN+1];
	isc_buffer_t buffer;
	dns_name_t *name;
	isc_result_t result;
	isc_boolean_t zeros;
	int i, n, len;

	if (tgt_prefix > 96 &&
	    tgt_ip->w[0] == 0 &&
	    tgt_ip->w[1] == 0 &&
	    tgt_ip->w[2] == ADDR_V4MAPPED) {
		len = snprintf(str, sizeof(str), "%d.%d.%d.%d.%d",
			       tgt_prefix - 96,
			       tgt_ip->w[3] & 0xff,
			       (tgt_ip->w[3]>>8) & 0xff,
			       (tgt_ip->w[3]>>16) & 0xff,
			       (tgt_ip->w[3]>>24) & 0xff);
		if (len == -1 || len > (int)sizeof(str))
			return (ISC_R_FAILURE);
	} else {
		for (i = 0; i < DNS_RPZ_CIDR_WORDS; i++) {
			w[i*2+1] = ((tgt_ip->w[DNS_RPZ_CIDR_WORDS-1-i] >> 16)
				    & 0xffff);
			w[i*2] = tgt_ip->w[DNS_RPZ_CIDR_WORDS-1-i] & 0xffff;
		}
		zeros = ISC_FALSE;
		len = snprintf(str, sizeof(str), "%d", tgt_prefix);
		if (len == -1)
			return (ISC_R_FAILURE);
		i = 0;
		while (i < DNS_RPZ_CIDR_WORDS * 2) {
			if (w[i] != 0 || zeros
			    || i >= DNS_RPZ_CIDR_WORDS * 2 - 1
			    || w[i+1] != 0) {
				INSIST((size_t)len <= sizeof(str));
				n = snprintf(&str[len], sizeof(str) - len,
					     ".%x", w[i++]);
				if (n < 0)
					return (ISC_R_FAILURE);
				len += n;
			} else {
				zeros = ISC_TRUE;
				INSIST((size_t)len <= sizeof(str));
				n = snprintf(&str[len], sizeof(str) - len,
					     ".zz");
				if (n < 0)
					return (ISC_R_FAILURE);
				len += n;
				i += 2;
				while (i < DNS_RPZ_CIDR_WORDS * 2 && w[i] == 0)
					++i;
			}
			if (len > (int)sizeof(str))
				return (ISC_R_FAILURE);
		}
	}

	if (canon_name != NULL) {
		isc__buffer_init(&buffer, str, sizeof(str));
		isc__buffer_add(&buffer, len);
		result = dns_name_fromtext(canon_name, &buffer,
					   dns_rootname, 0, NULL);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	if (search_name != NULL) {
		isc__buffer_init(&buffer, str, sizeof(str));
		isc__buffer_add(&buffer, len);
		if (type == DNS_RPZ_TYPE_NSIP)
			name = &cidr->nsip_name;
		else
			name = &cidr->ip_name;
		result = dns_name_fromtext(search_name, &buffer, name, 0, NULL);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	return (ISC_R_SUCCESS);
}



/*
 * Decide which kind of IP address response policy zone a name is in.
 */
static dns_rpz_type_t
set_type(dns_rpz_cidr_t *cidr, dns_name_t *name) {

	if (dns_name_issubdomain(name, &cidr->ip_name))
		return (DNS_RPZ_TYPE_IP);

	/*
	 * Require `./configure --enable-rpz-nsip` and nsdname
	 * until consistency problems are resolved.
	 */
#ifdef ENABLE_RPZ_NSIP
	if (dns_name_issubdomain(name, &cidr->nsip_name))
		return (DNS_RPZ_TYPE_NSIP);
#endif

#ifdef ENABLE_RPZ_NSDNAME
	if (dns_name_issubdomain(name, &cidr->nsdname_name))
		return (DNS_RPZ_TYPE_NSDNAME);
#endif

	return (DNS_RPZ_TYPE_QNAME);
}



/*
 * Convert an IP address from canonical response policy domain name form
 * to radix tree binary (host byte order).
 */
static isc_result_t
name2ipkey(dns_rpz_cidr_t *cidr, int level, dns_name_t *src_name,
	   dns_rpz_type_t type, dns_rpz_cidr_key_t *tgt_ip,
	   dns_rpz_cidr_bits_t *tgt_prefix)
{
	isc_buffer_t buffer;
	unsigned char data[DNS_NAME_MAXWIRE+1];
	dns_fixedname_t fname;
	dns_name_t *name;
	const char *cp, *end;
	char *cp2;
	int ip_labels;
	dns_rpz_cidr_bits_t bits;
	unsigned long prefix, l;
	int i;

	/*
	 * Need at least enough labels for the shortest name,
	 * :: or 128.*.RPZ_x_ZONE.rpz.LOCALHOST.
	 */
	ip_labels = dns_name_countlabels(src_name);
	ip_labels -= dns_name_countlabels(&cidr->ip_name);
	ip_labels--;
	if (ip_labels < 1) {
		badname(level, src_name, ", too short");
		return (ISC_R_FAILURE);
	}

	/*
	 * Get text for the IP address without RPZ_x_ZONE.rpz.LOCALHOST.
	 */
	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	dns_name_split(src_name, dns_name_countlabels(&cidr->ip_name),
		       name, NULL);
	isc_buffer_init(&buffer, data, sizeof(data));
	dns_name_totext(name, ISC_TRUE, &buffer);
	isc_buffer_putuint8(&buffer, '\0');
	cp = isc_buffer_base(&buffer);

	prefix = strtoul(cp, &cp2, 10);
	if (prefix < 1U || prefix > 128U || *cp2 != '.') {
		badname(level, src_name, ", bad prefix length");
		return (ISC_R_FAILURE);
	}
	cp = cp2+1;

	end = isc_buffer_used(&buffer);
	if (ip_labels == 4 && !strchr(cp, 'z')) {
		/*
		 * Convert an IPv4 address
		 * from the form "prefix.w.z.y.x"
		 */
		if (prefix > 32U) {
			badname(level, src_name, "; bad IPv4 prefix length");
			return (ISC_R_FAILURE);
		}
		prefix += 96;
		*tgt_prefix = (dns_rpz_cidr_bits_t)prefix;
		tgt_ip->w[0] = 0;
		tgt_ip->w[1] = 0;
		tgt_ip->w[2] = ADDR_V4MAPPED;
		tgt_ip->w[3] = 0;
		for (i = 0; i < 32; i += 8) {
			l = strtoul(cp, &cp2, 10);
			if (l > 255U || (*cp2 != '.' && *cp2 != '\0')) {
				badname(level, src_name, "; bad IPv4 address");
				return (ISC_R_FAILURE);
			}
			tgt_ip->w[3] |= l << i;
			cp = cp2 + 1;
		}
	} else {
		/*
		 * Convert a text IPv6 address.
		 */
		*tgt_prefix = (dns_rpz_cidr_bits_t)prefix;
		for (i = 0;
		     ip_labels > 0 && i < DNS_RPZ_CIDR_WORDS * 2;
		     ip_labels--) {
			if (cp[0] == 'z' && cp[1] == 'z' &&
			    (cp[2] == '.' || cp[2] == '\0') &&
			    i <= 6) {
				do {
					if ((i & 1) == 0)
					    tgt_ip->w[3-i/2] = 0;
					++i;
				} while (ip_labels + i <= 8);
				cp += 3;
			} else {
				l = strtoul(cp, &cp2, 16);
				if (l > 0xffffu ||
				    (*cp2 != '.' && *cp2 != '\0')) {
					badname(level, src_name, "");
					return (ISC_R_FAILURE);
				}
				if ((i & 1) == 0)
					tgt_ip->w[3-i/2] = l;
				else
					tgt_ip->w[3-i/2] |= l << 16;
				i++;
				cp = cp2 + 1;
			}
		}
	}
	if (cp != end) {
		badname(level, src_name, "");
		return (ISC_R_FAILURE);
	}

	/*
	 * Check for 1s after the prefix length.
	 */
	bits = (dns_rpz_cidr_bits_t)prefix;
	while (bits < DNS_RPZ_CIDR_KEY_BITS) {
		dns_rpz_cidr_word_t aword;

		i = bits % DNS_RPZ_CIDR_WORD_BITS;
		aword = tgt_ip->w[bits / DNS_RPZ_CIDR_WORD_BITS];
		if ((aword & ~DNS_RPZ_WORD_MASK(i)) != 0) {
			badname(level, src_name, "; wrong prefix length");
			return (ISC_R_FAILURE);
		}
		bits -= i;
		bits += DNS_RPZ_CIDR_WORD_BITS;
	}

	/*
	 * Convert the IPv6 address back to a canonical policy domain name
	 * to ensure that it is in canonical form.
	 */
	if (ISC_R_SUCCESS != ip2name(cidr, tgt_ip, (dns_rpz_cidr_bits_t)prefix,
				     type, NULL, name) ||
	    !dns_name_equal(src_name, name)) {
		badname(level, src_name, "; not canonical");
		return (ISC_R_FAILURE);
	}

	return (ISC_R_SUCCESS);
}



/*
 * find first differing bit
 */
static int
ffbit(dns_rpz_cidr_word_t w) {
	int bit;

	if (w == 0)
		return (DNS_RPZ_CIDR_WORD_BITS);
	for (bit = 0; (w & (1U << (DNS_RPZ_CIDR_WORD_BITS-1))) == 0; bit++)
		w <<= 1;
	return (bit);
}



/*
 * find the first differing bit in two keys
 */
static int
diff_keys(const dns_rpz_cidr_key_t *key1, dns_rpz_cidr_bits_t bits1,
	  const dns_rpz_cidr_key_t *key2, dns_rpz_cidr_bits_t bits2)
{
	dns_rpz_cidr_word_t delta;
	dns_rpz_cidr_bits_t maxbit, bit;
	int i;

	maxbit = ISC_MIN(bits1, bits2);

	/*
	 * find the first differing words
	 */
	for (i = 0, bit = 0;
	     bit <= maxbit;
	     i++, bit += DNS_RPZ_CIDR_WORD_BITS) {
		delta = key1->w[i] ^ key2->w[i];
		if (delta != 0) {
			bit += ffbit(delta);
			break;
		}
	}
	return (ISC_MIN(bit, maxbit));
}



/*
 * Search a radix tree for an IP address for ordinary lookup
 *	or for a CIDR block adding or deleting an entry
 * The tree read (for simple search) or write lock must be held by the caller.
 *
 * return ISC_R_SUCCESS, ISC_R_NOTFOUND, DNS_R_PARTIALMATCH, ISC_R_EXISTS,
 *	ISC_R_NOMEMORY
 */
static isc_result_t
search(dns_rpz_cidr_t *cidr, const dns_rpz_cidr_key_t *tgt_ip,
       dns_rpz_cidr_bits_t tgt_prefix, dns_rpz_type_t type,
       isc_boolean_t create,
       dns_rpz_cidr_node_t **found)		/* NULL or longest match node */
{
	dns_rpz_cidr_node_t *cur, *parent, *child, *new_parent, *sibling;
	int cur_num, child_num;
	dns_rpz_cidr_bits_t dbit;
	dns_rpz_cidr_flags_t flags, data_flag;
	isc_result_t find_result;

	flags = get_flags(tgt_ip, tgt_prefix, type);
	data_flag = flags & (DNS_RPZ_CIDR_FG_IP_DATA |
			     DNS_RPZ_CIDR_FG_NSIP_DATA);

	find_result = ISC_R_NOTFOUND;
	if (found != NULL)
		*found = NULL;
	cur = cidr->root;
	parent = NULL;
	cur_num = 0;
	for (;;) {
		if (cur == NULL) {
			/*
			 * No child so we cannot go down.  Fail or
			 * add the target as a child of the current parent.
			 */
			if (!create)
				return (find_result);
			child = new_node(cidr, tgt_ip, tgt_prefix, 0);
			if (child == NULL)
				return (ISC_R_NOMEMORY);
			if (parent == NULL)
				cidr->root = child;
			else
				parent->child[cur_num] = child;
			child->parent = parent;
			set_node_flags(child, type);
			if (found != NULL)
				*found = cur;
			return (ISC_R_SUCCESS);
		}

		/*
		 * Pretend a node not in the correct tree does not exist
		 * if we are not adding to the tree,
		 * If we are adding, then continue down to eventually
		 * add a node and mark/put this node in the correct tree.
		 */
		if ((cur->flags & flags) == 0 && !create)
			return (find_result);

		dbit = diff_keys(tgt_ip, tgt_prefix, &cur->ip, cur->bits);
		/*
		 * dbit <= tgt_prefix and dbit <= cur->bits always.
		 * We are finished searching if we matched all of the target.
		 */
		if (dbit == tgt_prefix) {
			if (tgt_prefix == cur->bits) {
				/*
				 * The current node matches the target exactly.
				 * It is the answer if it has data.
				 */
				if ((cur->flags & data_flag) != 0) {
					if (create)
						return (ISC_R_EXISTS);
					if (found != NULL)
						*found = cur;
					return (ISC_R_SUCCESS);
				} else if (create) {
					/*
					 * The node had no data but does now.
					 */
					set_node_flags(cur, type);
					if (found != NULL)
						*found = cur;
					return (ISC_R_SUCCESS);
				}
				return (find_result);
			}

			/*
			 * We know tgt_prefix < cur_bits which means that
			 * the target is shorter than the current node.
			 * Add the target as the current node's parent.
			 */
			if (!create)
				return (find_result);

			new_parent = new_node(cidr, tgt_ip, tgt_prefix,
					      cur->flags);
			if (new_parent == NULL)
				return (ISC_R_NOMEMORY);
			new_parent->parent = parent;
			if (parent == NULL)
				cidr->root = new_parent;
			else
				parent->child[cur_num] = new_parent;
			child_num = DNS_RPZ_IP_BIT(&cur->ip, tgt_prefix+1);
			new_parent->child[child_num] = cur;
			cur->parent = new_parent;
			set_node_flags(new_parent, type);
			if (found != NULL)
				*found = new_parent;
			return (ISC_R_SUCCESS);
		}

		if (dbit == cur->bits) {
			/*
			 * We have a partial match by matching of all of the
			 * current node but only part of the target.
			 * Try to go down.
			 */
			if ((cur->flags & data_flag) != 0) {
				find_result = DNS_R_PARTIALMATCH;
				if (found != NULL)
					*found = cur;
			}

			parent = cur;
			cur_num = DNS_RPZ_IP_BIT(tgt_ip, dbit);
			cur = cur->child[cur_num];
			continue;
		}


		/*
		 * dbit < tgt_prefix and dbit < cur->bits,
		 * so we failed to match both the target and the current node.
		 * Insert a fork of a parent above the current node and
		 * add the target as a sibling of the current node
		 */
		if (!create)
			return (find_result);

		sibling = new_node(cidr, tgt_ip, tgt_prefix, 0);
		if (sibling == NULL)
			return (ISC_R_NOMEMORY);
		new_parent = new_node(cidr, tgt_ip, dbit, cur->flags);
		if (new_parent == NULL) {
			isc_mem_put(cidr->mctx, sibling, sizeof(*sibling));
			return (ISC_R_NOMEMORY);
		}
		new_parent->parent = parent;
		if (parent == NULL)
			cidr->root = new_parent;
		else
			parent->child[cur_num] = new_parent;
		child_num = DNS_RPZ_IP_BIT(tgt_ip, dbit);
		new_parent->child[child_num] = sibling;
		new_parent->child[1-child_num] = cur;
		cur->parent = new_parent;
		sibling->parent = new_parent;
		set_node_flags(sibling, type);
		if (found != NULL)
			*found = sibling;
		return (ISC_R_SUCCESS);
	}
}



/*
 * Add an IP address to the radix tree of a response policy database.
 *	The tree write lock must be held by the caller.
 */
void
dns_rpz_cidr_addip(dns_rpz_cidr_t *cidr, dns_name_t *name)
{
	dns_rpz_cidr_key_t tgt_ip;
	dns_rpz_cidr_bits_t tgt_prefix;
	dns_rpz_type_t type;

	if (cidr == NULL)
		return;

	/*
	 * no worries if the new name is not an IP address
	 */
	type = set_type(cidr, name);
	switch (type) {
	case DNS_RPZ_TYPE_IP:
	case DNS_RPZ_TYPE_NSIP:
		break;
	case DNS_RPZ_TYPE_NSDNAME:
		cidr->had_nsdname = ISC_TRUE;
		return;
	case DNS_RPZ_TYPE_QNAME:
	case DNS_RPZ_TYPE_BAD:
		return;
	}
	if (ISC_R_SUCCESS != name2ipkey(cidr, DNS_RPZ_ERROR_LEVEL, name,
					type, &tgt_ip, &tgt_prefix))
		return;

	if (ISC_R_EXISTS == search(cidr, &tgt_ip, tgt_prefix, type,
				   ISC_TRUE, NULL) &&
	    isc_log_wouldlog(dns_lctx, DNS_RPZ_ERROR_LEVEL)) {
		char printname[DNS_NAME_FORMATSIZE];

		dns_name_format(name, printname, sizeof(printname));
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_RBTDB, DNS_RPZ_ERROR_LEVEL,
			      "duplicate response policy name \"%s\"",
			      printname);
	}
}



/*
 * Delete an IP address from the radix tree of a response policy database.
 *	The tree write lock must be held by the caller.
 */
void
dns_rpz_cidr_deleteip(dns_rpz_cidr_t *cidr, dns_name_t *name) {
	dns_rpz_cidr_key_t tgt_ip;
	dns_rpz_cidr_bits_t tgt_prefix;
	dns_rpz_type_t type;
	dns_rpz_cidr_node_t *tgt = NULL, *parent, *child;
	dns_rpz_cidr_flags_t flags, data_flag;

	if (cidr == NULL)
		return;

	/*
	 * Decide which kind of policy zone IP address it is, if either
	 * and then find its node.
	 */
	type = set_type(cidr, name);
	switch (type) {
	case DNS_RPZ_TYPE_IP:
	case DNS_RPZ_TYPE_NSIP:
		break;
	case DNS_RPZ_TYPE_NSDNAME:
		/*
		 * We cannot easily count nsdnames because
		 * internal rbt nodes get deleted.
		 */
		return;
	case DNS_RPZ_TYPE_QNAME:
	case DNS_RPZ_TYPE_BAD:
		return;
	}

	/*
	 * Do not get excited about the deletion of interior rbt nodes.
	 */
	if (ISC_R_SUCCESS != name2ipkey(cidr, DNS_RPZ_DEBUG_LEVEL2, name,
					type, &tgt_ip, &tgt_prefix))
		return;
	if (ISC_R_SUCCESS != search(cidr, &tgt_ip, tgt_prefix, type,
				    ISC_FALSE, &tgt)) {
		if (isc_log_wouldlog(dns_lctx, DNS_RPZ_ERROR_LEVEL)) {
			char printname[DNS_NAME_FORMATSIZE];

			dns_name_format(name, printname, sizeof(printname));
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_RBTDB, DNS_RPZ_ERROR_LEVEL,
				      "missing response policy node \"%s\"",
				      printname);
		}
		return;
	}

	/*
	 * Mark the node and its parents to reflect the deleted IP address.
	 */
	flags = get_flags(&tgt_ip, tgt_prefix, type);
	data_flag = flags & (DNS_RPZ_CIDR_FG_IP_DATA |
			      DNS_RPZ_CIDR_FG_NSIP_DATA);
	tgt->flags &= ~data_flag;
	for (parent = tgt; parent != NULL; parent = parent->parent) {
		if ((parent->flags & data_flag) != 0 ||
		    (parent->child[0] != NULL &&
		     (parent->child[0]->flags & flags) != 0) ||
		    (parent->child[1] != NULL &&
		     (parent->child[1]->flags & flags) != 0))
			break;
		parent->flags &= ~flags;
	}

	/*
	 * We might need to delete 2 nodes.
	 */
	do {
		/*
		 * The node is now useless if it has no data of its own
		 * and 0 or 1 children.  We are finished if it is not useless.
		 */
		if ((child = tgt->child[0]) != NULL) {
			if (tgt->child[1] != NULL)
				return;
		} else {
			child = tgt->child[1];
		}
		if ((tgt->flags & (DNS_RPZ_CIDR_FG_IP_DATA |
				 DNS_RPZ_CIDR_FG_NSIP_DATA)) != 0)
			return;

		/*
		 * Replace the pointer to this node in the parent with
		 * the remaining child or NULL.
		 */
		parent = tgt->parent;
		if (parent == NULL) {
			cidr->root = child;
		} else {
			parent->child[parent->child[1] == tgt] = child;
		}
		/*
		 * If the child exists fix up its parent pointer.
		 */
		if (child != NULL)
			child->parent = parent;
		isc_mem_put(cidr->mctx, tgt, sizeof(*tgt));

		tgt = parent;
	} while (tgt != NULL);
}



/*
 * Caller must hold tree lock.
 * Return  ISC_R_NOTFOUND
 *	or ISC_R_SUCCESS and the found entry's canonical and search names
 *	    and its prefix length
 */
isc_result_t
dns_rpz_cidr_find(dns_rpz_cidr_t *cidr, const isc_netaddr_t *netaddr,
		  dns_rpz_type_t type, dns_name_t *canon_name,
		  dns_name_t *search_name, dns_rpz_cidr_bits_t *prefix)
{
	dns_rpz_cidr_key_t tgt_ip;
	isc_result_t result;
	dns_rpz_cidr_node_t *found;
	int i;

	/*
	 * Convert IP address to CIDR tree key.
	 */
	if (netaddr->family == AF_INET) {
		tgt_ip.w[0] = 0;
		tgt_ip.w[1] = 0;
		tgt_ip.w[2] = ADDR_V4MAPPED;
		tgt_ip.w[3] = ntohl(netaddr->type.in.s_addr);
	} else if (netaddr->family == AF_INET6) {
		dns_rpz_cidr_key_t src_ip6;

		/*
		 * Given the int aligned struct in_addr member of netaddr->type
		 * one could cast netaddr->type.in6 to dns_rpz_cidr_key_t *,
		 * but there are objections.
		 */
		memcpy(src_ip6.w, &netaddr->type.in6, sizeof(src_ip6.w));
		for (i = 0; i < 4; i++) {
			tgt_ip.w[i] = ntohl(src_ip6.w[i]);
		}
	} else {
		return (ISC_R_NOTFOUND);
	}

	result = search(cidr, &tgt_ip, 128, type, ISC_FALSE, &found);
	if (result != ISC_R_SUCCESS && result != DNS_R_PARTIALMATCH)
		return (result);

	*prefix = found->bits;
	return (ip2name(cidr, &found->ip, found->bits, type,
			canon_name, search_name));
}



/*
 * Translate CNAME rdata to a QNAME response policy action.
 */
dns_rpz_policy_t
dns_rpz_decode_cname(dns_rdataset_t *rdataset, dns_name_t *selfname) {
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_cname_t cname;
	isc_result_t result;

	result = dns_rdataset_first(rdataset);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	dns_rdataset_current(rdataset, &rdata);
	result = dns_rdata_tostruct(&rdata, &cname, NULL);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	dns_rdata_reset(&rdata);

	/*
	 * CNAME . means NXDOMAIN
	 */
	if (dns_name_equal(&cname.cname, dns_rootname))
		return (DNS_RPZ_POLICY_NXDOMAIN);

	/*
	 * CNAME *. means NODATA
	 */
	if (dns_name_countlabels(&cname.cname) == 2
	    && dns_name_iswildcard(&cname.cname))
		return (DNS_RPZ_POLICY_NODATA);

	/*
	 * 128.1.0.127.rpz-ip CNAME  128.1.0.0.127. means "do not rewrite"
	 */
	if (selfname != NULL && dns_name_equal(&cname.cname, selfname))
		return (DNS_RPZ_POLICY_NO_OP);

	/*
	 * evil.com CNAME garden.net rewrites www.evil.com to www.garden.net.
	 */
	return (DNS_RPZ_POLICY_RECORD);
}
