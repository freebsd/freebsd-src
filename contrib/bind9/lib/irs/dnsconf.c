/*
 * Copyright (C) 2009  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: dnsconf.c,v 1.3 2009-09-02 23:48:02 tbox Exp $ */

/*! \file */

#include <config.h>

#include <string.h>

#include <isc/base64.h>
#include <isc/buffer.h>
#include <isc/file.h>
#include <isc/mem.h>
#include <isc/util.h>

#include <isccfg/dnsconf.h>

#include <dns/fixedname.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdatastruct.h>

#include <irs/dnsconf.h>

#define IRS_DNSCONF_MAGIC		ISC_MAGIC('D', 'c', 'f', 'g')
#define IRS_DNSCONF_VALID(c)		ISC_MAGIC_VALID(c, IRS_DNSCONF_MAGIC)

/*!
 * configuration data structure
 */

struct irs_dnsconf {
	unsigned int magic;
	isc_mem_t *mctx;
	irs_dnsconf_dnskeylist_t trusted_keylist;
};

static isc_result_t
configure_dnsseckeys(irs_dnsconf_t *conf, cfg_obj_t *cfgobj,
		     dns_rdataclass_t rdclass)
{
	isc_mem_t *mctx = conf->mctx;
	const cfg_obj_t *keys = NULL;
	const cfg_obj_t *key, *keylist;
	dns_fixedname_t fkeyname;
	dns_name_t *keyname_base, *keyname;
	const cfg_listelt_t *element, *element2;
	isc_result_t result;
	isc_uint32_t flags, proto, alg;
	const char *keystr, *keynamestr;
	unsigned char keydata[4096];
	isc_buffer_t keydatabuf_base, *keydatabuf;
	dns_rdata_dnskey_t keystruct;
	unsigned char rrdata[4096];
	isc_buffer_t rrdatabuf;
	isc_region_t r;
	isc_buffer_t namebuf;
	irs_dnsconf_dnskey_t *keyent;

	cfg_map_get(cfgobj, "trusted-keys", &keys);
	if (keys == NULL)
		return (ISC_R_SUCCESS);

	for (element = cfg_list_first(keys);
	     element != NULL;
	     element = cfg_list_next(element)) {
		keylist = cfg_listelt_value(element);
		for (element2 = cfg_list_first(keylist);
		     element2 != NULL;
		     element2 = cfg_list_next(element2))
		{
			keydatabuf = NULL;
			keyname = NULL;

			key = cfg_listelt_value(element2);

			flags = cfg_obj_asuint32(cfg_tuple_get(key, "flags"));
			proto = cfg_obj_asuint32(cfg_tuple_get(key,
							       "protocol"));
			alg = cfg_obj_asuint32(cfg_tuple_get(key,
							     "algorithm"));
			keynamestr = cfg_obj_asstring(cfg_tuple_get(key,
								    "name"));

			keystruct.common.rdclass = rdclass;
			keystruct.common.rdtype = dns_rdatatype_dnskey;
			keystruct.mctx = NULL;
			ISC_LINK_INIT(&keystruct.common, link);

			if (flags > 0xffff)
				return (ISC_R_RANGE);
			if (proto > 0xff)
				return (ISC_R_RANGE);
			if (alg > 0xff)
				return (ISC_R_RANGE);
			keystruct.flags = (isc_uint16_t)flags;
			keystruct.protocol = (isc_uint8_t)proto;
			keystruct.algorithm = (isc_uint8_t)alg;

			isc_buffer_init(&keydatabuf_base, keydata,
					sizeof(keydata));
			isc_buffer_init(&rrdatabuf, rrdata, sizeof(rrdata));

			/* Configure key value */
			keystr = cfg_obj_asstring(cfg_tuple_get(key, "key"));
			result = isc_base64_decodestring(keystr,
							 &keydatabuf_base);
			if (result != ISC_R_SUCCESS)
				return (result);
			isc_buffer_usedregion(&keydatabuf_base, &r);
			keystruct.datalen = r.length;
			keystruct.data = r.base;

			result = dns_rdata_fromstruct(NULL,
						      keystruct.common.rdclass,
						      keystruct.common.rdtype,
						      &keystruct, &rrdatabuf);
			if (result != ISC_R_SUCCESS)
				return (result);
			isc_buffer_usedregion(&rrdatabuf, &r);
			result = isc_buffer_allocate(mctx, &keydatabuf,
						     r.length);
			if (result != ISC_R_SUCCESS)
				return (result);
			result = isc_buffer_copyregion(keydatabuf, &r);
			if (result != ISC_R_SUCCESS)
				goto cleanup;

			/* Configure key name */
			dns_fixedname_init(&fkeyname);
			keyname_base = dns_fixedname_name(&fkeyname);
			isc_buffer_init(&namebuf, keynamestr,
					strlen(keynamestr));
			isc_buffer_add(&namebuf, strlen(keynamestr));
			result = dns_name_fromtext(keyname_base, &namebuf,
						   dns_rootname, 0, NULL);
			if (result != ISC_R_SUCCESS)
				return (result);
			keyname = isc_mem_get(mctx, sizeof(*keyname));
			if (keyname == NULL) {
				result = ISC_R_NOMEMORY;
				goto cleanup;
			}
			dns_name_init(keyname, NULL);
			result = dns_name_dup(keyname_base, mctx, keyname);
			if (result != ISC_R_SUCCESS)
				goto cleanup;

			/* Add the key data to the list */
			keyent = isc_mem_get(mctx, sizeof(*keyent));
			if (keyent == NULL) {
				dns_name_free(keyname, mctx);
				result = ISC_R_NOMEMORY;
				goto cleanup;
			}
			keyent->keyname = keyname;
			keyent->keydatabuf = keydatabuf;

			ISC_LIST_APPEND(conf->trusted_keylist, keyent, link);
		}
	}

	return (ISC_R_SUCCESS);

 cleanup:
	if (keydatabuf != NULL)
		isc_buffer_free(&keydatabuf);
	if (keyname != NULL)
		isc_mem_put(mctx, keyname, sizeof(*keyname));

	return (result);
}

isc_result_t
irs_dnsconf_load(isc_mem_t *mctx, const char *filename, irs_dnsconf_t **confp)
{
	irs_dnsconf_t *conf;
	cfg_parser_t *parser = NULL;
	cfg_obj_t *cfgobj = NULL;
	isc_result_t result = ISC_R_SUCCESS;

	REQUIRE(confp != NULL && *confp == NULL);

	conf = isc_mem_get(mctx, sizeof(*conf));
	if (conf == NULL)
		return (ISC_R_NOMEMORY);

	conf->mctx = mctx;
	ISC_LIST_INIT(conf->trusted_keylist);

	/*
	 * If the specified file does not exist, we'll simply with an empty
	 * configuration.
	 */
	if (!isc_file_exists(filename))
		goto cleanup;

	result = cfg_parser_create(mctx, NULL, &parser);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = cfg_parse_file(parser, filename, &cfg_type_dnsconf,
				&cfgobj);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = configure_dnsseckeys(conf, cfgobj, dns_rdataclass_in);

 cleanup:
	if (parser != NULL) {
		if (cfgobj != NULL)
			cfg_obj_destroy(parser, &cfgobj);
		cfg_parser_destroy(&parser);
	}

	conf->magic = IRS_DNSCONF_MAGIC;

	if (result == ISC_R_SUCCESS)
		*confp = conf;
	else
		irs_dnsconf_destroy(&conf);

	return (result);
}

void
irs_dnsconf_destroy(irs_dnsconf_t **confp) {
	irs_dnsconf_t *conf;
	irs_dnsconf_dnskey_t *keyent;

	REQUIRE(confp != NULL);
	conf = *confp;
	REQUIRE(IRS_DNSCONF_VALID(conf));

	while ((keyent = ISC_LIST_HEAD(conf->trusted_keylist)) != NULL) {
		ISC_LIST_UNLINK(conf->trusted_keylist, keyent, link);

		isc_buffer_free(&keyent->keydatabuf);
		dns_name_free(keyent->keyname, conf->mctx);
		isc_mem_put(conf->mctx, keyent->keyname, sizeof(dns_name_t));
		isc_mem_put(conf->mctx, keyent, sizeof(*keyent));
	}

	isc_mem_put(conf->mctx, conf, sizeof(*conf));

	*confp = NULL;
}

irs_dnsconf_dnskeylist_t *
irs_dnsconf_gettrustedkeys(irs_dnsconf_t *conf) {
	REQUIRE(IRS_DNSCONF_VALID(conf));

	return (&conf->trusted_keylist);
}
