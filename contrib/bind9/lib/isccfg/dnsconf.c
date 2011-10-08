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

/* $Id: dnsconf.c,v 1.4 2009-09-02 23:48:03 tbox Exp $ */

/*! \file */

#include <config.h>

#include <isccfg/cfg.h>
#include <isccfg/grammar.h>

/*%
 * A trusted key, as used in the "trusted-keys" statement.
 */
static cfg_tuplefielddef_t trustedkey_fields[] = {
	{ "name", &cfg_type_astring, 0 },
	{ "flags", &cfg_type_uint32, 0 },
	{ "protocol", &cfg_type_uint32, 0 },
	{ "algorithm", &cfg_type_uint32, 0 },
	{ "key", &cfg_type_qstring, 0 },
	{ NULL, NULL, 0 }
};

static cfg_type_t cfg_type_trustedkey = {
	"trustedkey", cfg_parse_tuple, cfg_print_tuple, cfg_doc_tuple,
	&cfg_rep_tuple, trustedkey_fields
};

static cfg_type_t cfg_type_trustedkeys = {
	"trusted-keys", cfg_parse_bracketed_list, cfg_print_bracketed_list,
	cfg_doc_bracketed_list, &cfg_rep_list, &cfg_type_trustedkey
};

/*%
 * Clauses that can be found within the top level of the dns.conf
 * file only.
 */
static cfg_clausedef_t
dnsconf_clauses[] = {
	{ "trusted-keys", &cfg_type_trustedkeys, CFG_CLAUSEFLAG_MULTI },
	{ NULL, NULL, 0 }
};

/*% The top-level dns.conf syntax. */

static cfg_clausedef_t *
dnsconf_clausesets[] = {
	dnsconf_clauses,
	NULL
};

LIBISCCFG_EXTERNAL_DATA cfg_type_t cfg_type_dnsconf = {
	"dnsconf", cfg_parse_mapbody, cfg_print_mapbody, cfg_doc_mapbody,
	&cfg_rep_map, dnsconf_clausesets
};
