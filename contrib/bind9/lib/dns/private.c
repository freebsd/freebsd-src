/*
 * Copyright (C) 2009, 2012  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id$ */

#include "config.h"

#include <isc/result.h>
#include <isc/string.h>
#include <isc/types.h>
#include <isc/base64.h>

#include <dns/nsec3.h>
#include <dns/private.h>

/*
 * We need to build the relevant chain if there exists a NSEC/NSEC3PARAM
 * at the apex; normally only one or the other of NSEC/NSEC3PARAM will exist.
 *
 * If a NSEC3PARAM RRset exists then we will need to build a NSEC chain
 * if all the NSEC3PARAM records (and associated chains) are slated for
 * destruction and we have not been told to NOT build the NSEC chain.
 *
 * If the NSEC set exist then check to see if there is a request to create
 * a NSEC3 chain.
 *
 * If neither NSEC/NSEC3PARAM RRsets exist at the origin and the private
 * type exists then we need to examine it to determine if NSEC3 chain has
 * been requested to be built otherwise a NSEC chain needs to be built.
 */

#define REMOVE(x) (((x) & DNS_NSEC3FLAG_REMOVE) != 0)
#define CREATE(x) (((x) & DNS_NSEC3FLAG_CREATE) != 0)
#define NONSEC(x) (((x) & DNS_NSEC3FLAG_NONSEC) != 0)

#define CHECK(x) do {					\
			 result = (x);			\
			 if (result != ISC_R_SUCCESS)	\
				goto failure;		\
		 } while (0)

/*
 * Work out if 'param' should be ignored or not (i.e. it is in the process
 * of being removed).
 *
 * Note: we 'belt-and-braces' here by also checking for a CREATE private
 * record and keep the param record in this case.
 */

static isc_boolean_t
ignore(dns_rdata_t *param, dns_rdataset_t *privateset) {
	isc_result_t result;

	for (result = dns_rdataset_first(privateset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(privateset)) {
		unsigned char buf[DNS_NSEC3PARAM_BUFFERSIZE];
		dns_rdata_t private = DNS_RDATA_INIT;
		dns_rdata_t rdata = DNS_RDATA_INIT;

		dns_rdataset_current(privateset, &private);
		if (!dns_nsec3param_fromprivate(&private, &rdata,
						buf, sizeof(buf)))
			continue;
		/*
		 * We are going to create a new NSEC3 chain so it
		 * doesn't matter if we are removing this one.
		 */
		if (CREATE(rdata.data[1]))
			return (ISC_FALSE);
		if (rdata.data[0] != param->data[0] ||
		    rdata.data[2] != param->data[2] ||
		    rdata.data[3] != param->data[3] ||
		    rdata.data[4] != param->data[4] ||
		    memcmp(&rdata.data[5], &param->data[5], param->data[4]))
			continue;
		/*
		 * The removal of this NSEC3 chain does NOT cause a
		 * NSEC chain to be created so we don't need to tell
		 * the caller that it will be removed.
		 */
		if (NONSEC(rdata.data[1]))
			return (ISC_FALSE);
		return (ISC_TRUE);
	}
	return (ISC_FALSE);
}

isc_result_t
dns_private_chains(dns_db_t *db, dns_dbversion_t *ver,
		   dns_rdatatype_t privatetype,
		   isc_boolean_t *build_nsec, isc_boolean_t *build_nsec3)
{
	dns_dbnode_t *node;
	dns_rdataset_t nsecset, nsec3paramset, privateset;
	isc_boolean_t nsec3chain;
	isc_boolean_t signing;
	isc_result_t result;
	unsigned char buf[DNS_NSEC3PARAM_BUFFERSIZE];
	unsigned int count;

	node = NULL;
	dns_rdataset_init(&nsecset);
	dns_rdataset_init(&nsec3paramset);
	dns_rdataset_init(&privateset);

	CHECK(dns_db_getoriginnode(db, &node));

	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_nsec,
				     0, (isc_stdtime_t) 0, &nsecset, NULL);

	if (result != ISC_R_SUCCESS && result != ISC_R_NOTFOUND)
		goto failure;

	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_nsec3param,
				     0, (isc_stdtime_t) 0, &nsec3paramset,
				     NULL);
	if (result != ISC_R_SUCCESS && result != ISC_R_NOTFOUND)
		goto failure;

	if (dns_rdataset_isassociated(&nsecset) &&
	    dns_rdataset_isassociated(&nsec3paramset)) {
		if (build_nsec != NULL)
			*build_nsec = ISC_TRUE;
		if (build_nsec3 != NULL)
			*build_nsec3 = ISC_TRUE;
		goto success;
	}

	if (privatetype != (dns_rdatatype_t)0) {
		result = dns_db_findrdataset(db, node, ver, privatetype,
					     0, (isc_stdtime_t) 0,
					     &privateset, NULL);
		if (result != ISC_R_SUCCESS && result != ISC_R_NOTFOUND)
			goto failure;
	}

	/*
	 * Look to see if we also need to be creating a NSEC3 chains.
	 */
	if (dns_rdataset_isassociated(&nsecset)) {
		if (build_nsec != NULL)
			*build_nsec = ISC_TRUE;
		if (build_nsec3 != NULL)
			*build_nsec3 = ISC_FALSE;
		if (!dns_rdataset_isassociated(&privateset))
			goto success;
		for (result = dns_rdataset_first(&privateset);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(&privateset)) {
			unsigned char buf[DNS_NSEC3PARAM_BUFFERSIZE];
			dns_rdata_t private = DNS_RDATA_INIT;
			dns_rdata_t rdata = DNS_RDATA_INIT;

			dns_rdataset_current(&privateset, &private);
			if (!dns_nsec3param_fromprivate(&private, &rdata,
							buf, sizeof(buf)))
				continue;
			if (REMOVE(rdata.data[1]))
				continue;
			if (build_nsec3 != NULL)
				*build_nsec3 = ISC_TRUE;
			break;
		}
		goto success;
	}

	if (dns_rdataset_isassociated(&nsec3paramset)) {
		if (build_nsec3 != NULL)
			*build_nsec3 = ISC_TRUE;
		if (build_nsec != NULL)
			*build_nsec = ISC_FALSE;
		if (!dns_rdataset_isassociated(&privateset))
			goto success;
		/*
		 * If we are in the process of building a new NSEC3 chain
		 * then we don't need to build a NSEC chain.
		 */
		for (result = dns_rdataset_first(&privateset);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(&privateset)) {
			dns_rdata_t private = DNS_RDATA_INIT;
			dns_rdata_t rdata = DNS_RDATA_INIT;

			dns_rdataset_current(&privateset, &private);
			if (!dns_nsec3param_fromprivate(&private, &rdata,
							buf, sizeof(buf)))
				continue;
			if (CREATE(rdata.data[1]))
				goto success;
		}

		/*
		 * Check to see if there will be a active NSEC3CHAIN once
		 * the changes queued complete.
		 */
		count = 0;
		for (result = dns_rdataset_first(&nsec3paramset);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(&nsec3paramset)) {
			dns_rdata_t rdata = DNS_RDATA_INIT;

			/*
			 * If there is more that one NSEC3 chain present then
			 * we don't need to construct a NSEC chain.
			 */
			if (++count > 1)
				goto success;
			dns_rdataset_current(&nsec3paramset, &rdata);
			if (ignore(&rdata, &privateset))
				continue;
			/*
			 * We still have a good NSEC3 chain or we are
			 * not creating a NSEC chain as NONSEC is set.
			 */
			goto success;
		}

		/*
		 * The last NSEC3 chain is being removed and does not have
		 * have NONSEC set.
		 */
		if (build_nsec != NULL)
			*build_nsec = ISC_TRUE;
		goto success;
	}

	if (build_nsec != NULL)
		*build_nsec = ISC_FALSE;
	if (build_nsec3 != NULL)
		*build_nsec3 = ISC_FALSE;
	if (!dns_rdataset_isassociated(&privateset))
		goto success;

	signing = ISC_FALSE;
	nsec3chain = ISC_FALSE;

	for (result = dns_rdataset_first(&privateset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&privateset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;
		dns_rdata_t private = DNS_RDATA_INIT;

		dns_rdataset_current(&privateset, &private);
		if (!dns_nsec3param_fromprivate(&private, &rdata,
						buf, sizeof(buf))) {
			/*
			 * Look for record that says we are signing the
			 * zone with a key.
			 */
			if (private.length == 5 && private.data[0] != 0 &&
			    private.data[3] == 0 && private.data[4] == 0)
				signing = ISC_TRUE;
		} else {
			if (CREATE(rdata.data[1]))
				nsec3chain = ISC_TRUE;
		}
	}

	if (signing) {
		if (nsec3chain) {
			if (build_nsec3 != NULL)
				*build_nsec3 = ISC_TRUE;
		} else {
			if (build_nsec != NULL)
				*build_nsec = ISC_TRUE;
		}
	}

 success:
	result = ISC_R_SUCCESS;
 failure:
	if (dns_rdataset_isassociated(&nsecset))
		dns_rdataset_disassociate(&nsecset);
	if (dns_rdataset_isassociated(&nsec3paramset))
		dns_rdataset_disassociate(&nsec3paramset);
	if (dns_rdataset_isassociated(&privateset))
		dns_rdataset_disassociate(&privateset);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	return (result);
}
