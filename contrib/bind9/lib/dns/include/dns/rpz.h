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

/* $Id: rpz.h,v 1.3 2011-01-13 04:59:26 tbox Exp $ */

#ifndef DNS_RPZ_H
#define DNS_RPZ_H 1

#include <isc/lang.h>

#include <dns/fixedname.h>
#include <dns/rdata.h>
#include <dns/types.h>

ISC_LANG_BEGINDECLS

#define DNS_RPZ_IP_ZONE		"rpz-ip"
#define DNS_RPZ_NSIP_ZONE	"rpz-nsip"
#define DNS_RPZ_NSDNAME_ZONE	"rpz-nsdname"

typedef isc_uint8_t		dns_rpz_cidr_bits_t;

typedef enum {
	DNS_RPZ_TYPE_BAD,
	DNS_RPZ_TYPE_QNAME,
	DNS_RPZ_TYPE_IP,
	DNS_RPZ_TYPE_NSIP,
	DNS_RPZ_TYPE_NSDNAME
} dns_rpz_type_t;

/*
 * Require DNS_RPZ_POLICY_NO_OP < DNS_RPZ_POLICY_NXDOMAIN <
 *	   DNS_RPZ_POLICY_NODATA < DNS_RPZ_POLICY_CNAME.
 */
typedef enum {
	DNS_RPZ_POLICY_GIVEN = 0,	/* 'given': what something else says */
	DNS_RPZ_POLICY_NO_OP = 1,	/* 'no-op': do not rewrite */
	DNS_RPZ_POLICY_NXDOMAIN = 2,	/* 'nxdomain': answer with NXDOMAIN */
	DNS_RPZ_POLICY_NODATA = 3,	/* 'nodata': answer with ANCOUNT=0 */
	DNS_RPZ_POLICY_CNAME = 4,	/* 'cname x': answer with x's rrsets */
	DNS_RPZ_POLICY_RECORD = 5,
	DNS_RPZ_POLICY_MISS,
	DNS_RPZ_POLICY_ERROR
} dns_rpz_policy_t;

/*
 * Specify a response policy zone.
 */
typedef struct dns_rpz_zone dns_rpz_zone_t;

struct dns_rpz_zone {
	ISC_LINK(dns_rpz_zone_t) link;
	int			 num;
	dns_name_t		 origin;  /* Policy zone name */
	dns_name_t		 nsdname; /* RPZ_NSDNAME_ZONE.origin */
	dns_rpz_policy_t	 policy;  /* RPZ_POLICY_GIVEN or override */
	dns_name_t		 cname;	  /* override name for
					     RPZ_POLICY_CNAME */
};

/*
 * Radix trees for response policy IP addresses.
 */
typedef struct dns_rpz_cidr	dns_rpz_cidr_t;

/*
 * context for finding the best policy
 */
typedef struct {
	unsigned int		state;
# define DNS_RPZ_REWRITTEN	0x0001
# define DNS_RPZ_DONE_QNAME	0x0002
# define DNS_RPZ_DONE_A	 	0x0004
# define DNS_RPZ_RECURSING	0x0008
# define DNS_RPZ_HAVE_IP 	0x0010
# define DNS_RPZ_HAVE_NSIPv4	0x0020
# define DNS_RPZ_HAVE_NSIPv6	0x0040
# define DNS_RPZ_HAD_NSDNAME	0x0080
	/*
	 * Best match so far.
	 */
	struct {
		dns_rpz_type_t		type;
		dns_rpz_zone_t		*rpz;
		dns_rpz_cidr_bits_t	prefix;
		dns_rpz_policy_t	policy;
		dns_ttl_t		ttl;
		isc_result_t		result;
		dns_zone_t		*zone;
		dns_db_t		*db;
		dns_dbnode_t		*node;
		dns_rdataset_t		*rdataset;
	} m;
	/*
	 * State for chasing NS names and addresses including recursion.
	 */
	struct {
		unsigned int		label;
		dns_db_t		*db;
		dns_rdataset_t		*ns_rdataset;
		dns_rdatatype_t		r_type;
		isc_result_t		r_result;
		dns_rdataset_t		*r_rdataset;
	} ns;
	/*
	 * State of real query while recursing for NSIP or NSDNAME.
	 */
	struct {
		isc_result_t		result;
		isc_boolean_t		is_zone;
		isc_boolean_t		authoritative;
		dns_zone_t		*zone;
		dns_db_t		*db;
		dns_dbnode_t		*node;
		dns_rdataset_t		*rdataset;
		dns_rdataset_t		*sigrdataset;
		dns_rdatatype_t		qtype;
	} q;
	dns_name_t		*qname;
	dns_name_t		*r_name;
	dns_name_t		*fname;
	dns_fixedname_t		_qnamef;
	dns_fixedname_t		_r_namef;
	dns_fixedname_t		_fnamef;
} dns_rpz_st_t;

#define DNS_RPZ_TTL_DEFAULT		5

/*
 * So various response policy zone messages can be turned up or down.
 */
#define DNS_RPZ_ERROR_LEVEL	ISC_LOG_WARNING
#define DNS_RPZ_INFO_LEVEL	ISC_LOG_INFO
#define DNS_RPZ_DEBUG_LEVEL1	ISC_LOG_DEBUG(1)
#define DNS_RPZ_DEBUG_LEVEL2	ISC_LOG_DEBUG(2)

const char *
dns_rpz_type2str(dns_rpz_type_t type);

dns_rpz_policy_t
dns_rpz_str2policy(const char *str);

void
dns_rpz_set_need(isc_boolean_t need);

isc_boolean_t
dns_rpz_needed(void);

void
dns_rpz_cidr_free(dns_rpz_cidr_t **cidr);

void
dns_rpz_view_destroy(dns_view_t *view);

isc_result_t
dns_rpz_new_cidr(isc_mem_t *mctx, dns_name_t *origin,
		 dns_rpz_cidr_t **rbtdb_cidr);
void
dns_rpz_enabled(dns_rpz_cidr_t *cidr, dns_rpz_st_t *st);

void
dns_rpz_cidr_deleteip(dns_rpz_cidr_t *cidr, dns_name_t *name);

void
dns_rpz_cidr_addip(dns_rpz_cidr_t *cidr, dns_name_t *name);

isc_result_t
dns_rpz_cidr_find(dns_rpz_cidr_t *cidr, const isc_netaddr_t *netaddr,
		  dns_rpz_type_t type, dns_name_t *canon_name,
		  dns_name_t *search_name, dns_rpz_cidr_bits_t *prefix);

dns_rpz_policy_t
dns_rpz_decode_cname(dns_rdataset_t *, dns_name_t *selfname);

#endif /* DNS_RPZ_H */

