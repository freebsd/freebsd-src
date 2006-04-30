/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
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

/* $Id: dnssec.h,v 1.21.12.5 2004/03/08 09:04:35 marka Exp $ */

#ifndef DNS_DNSSEC_H
#define DNS_DNSSEC_H 1

#include <isc/lang.h>
#include <isc/stdtime.h>

#include <dns/types.h>

#include <dst/dst.h>

ISC_LANG_BEGINDECLS

isc_result_t
dns_dnssec_keyfromrdata(dns_name_t *name, dns_rdata_t *rdata, isc_mem_t *mctx,
			dst_key_t **key);
/*
 *	Creates a DST key from a DNS record.  Basically a wrapper around
 *	dst_key_fromdns().
 *
 *	Requires:
 *		'name' is not NULL
 *		'rdata' is not NULL
 *		'mctx' is not NULL
 *		'key' is not NULL
 *		'*key' is NULL
 *
 *	Returns:
 *		ISC_R_SUCCESS
 *		ISC_R_NOMEMORY
 *		DST_R_INVALIDPUBLICKEY
 *		various errors from dns_name_totext
 */

isc_result_t
dns_dnssec_sign(dns_name_t *name, dns_rdataset_t *set, dst_key_t *key,
		isc_stdtime_t *inception, isc_stdtime_t *expire,
		isc_mem_t *mctx, isc_buffer_t *buffer, dns_rdata_t *sigrdata);
/*
 *	Generates a SIG record covering this rdataset.  This has no effect
 *	on existing SIG records.
 *
 *	Requires:
 *		'name' (the owner name of the record) is a valid name
 *		'set' is a valid rdataset
 *		'key' is a valid key
 *		'inception' is not NULL
 *		'expire' is not NULL
 *		'mctx' is not NULL
 *		'buffer' is not NULL
 *		'sigrdata' is not NULL
 *
 *	Returns:
 *		ISC_R_SUCCESS
 *		ISC_R_NOMEMORY
 *		ISC_R_NOSPACE
 *		DNS_R_INVALIDTIME - the expiration is before the inception
 *		DNS_R_KEYUNAUTHORIZED - the key cannot sign this data (either
 *			it is not a zone key or its flags prevent
 *			authentication)
 *		DST_R_*
 */

isc_result_t
dns_dnssec_verify(dns_name_t *name, dns_rdataset_t *set, dst_key_t *key,
		  isc_boolean_t ignoretime, isc_mem_t *mctx,
		  dns_rdata_t *sigrdata);

isc_result_t
dns_dnssec_verify2(dns_name_t *name, dns_rdataset_t *set, dst_key_t *key,
		   isc_boolean_t ignoretime, isc_mem_t *mctx,
		   dns_rdata_t *sigrdata, dns_name_t *wild);
/*
 *	Verifies the SIG record covering this rdataset signed by a specific
 *	key.  This does not determine if the key's owner is authorized to
 *	sign this record, as this requires a resolver or database.
 *	If 'ignoretime' is ISC_TRUE, temporal validity will not be checked.
 *
 *	Requires:
 *		'name' (the owner name of the record) is a valid name
 *		'set' is a valid rdataset
 *		'key' is a valid key
 *		'mctx' is not NULL
 *		'sigrdata' is a valid rdata containing a SIG record
 *		'wild' if non-NULL then is a valid and has a buffer.
 *
 *	Returns:
 *		ISC_R_SUCCESS
 *		ISC_R_NOMEMORY
 *		DNS_R_FROMWILDCARD - the signature is valid and is from
 *			a wildcard expansion.  dns_dnssec_verify2() only.
 *			'wild' contains the name of the wildcard if non-NULL.
 *		DNS_R_SIGINVALID - the signature fails to verify
 *		DNS_R_SIGEXPIRED - the signature has expired
 *		DNS_R_SIGFUTURE - the signature's validity period has not begun
 *		DNS_R_KEYUNAUTHORIZED - the key cannot sign this data (either
 *			it is not a zone key or its flags prevent
 *			authentication)
 *		DST_R_*
 */

isc_result_t
dns_dnssec_findzonekeys(dns_db_t *db, dns_dbversion_t *ver, dns_dbnode_t *node,
			dns_name_t *name, isc_mem_t *mctx,
			unsigned int maxkeys, dst_key_t **keys,
			unsigned int *nkeys);
isc_result_t
dns_dnssec_findzonekeys2(dns_db_t *db, dns_dbversion_t *ver,
			 dns_dbnode_t *node, dns_name_t *name,
			 const char *directory, isc_mem_t *mctx,
			 unsigned int maxkeys, dst_key_t **keys,
			 unsigned int *nkeys);
/*
 * 	Finds a set of zone keys.
 * 	XXX temporary - this should be handled in dns_zone_t.
 */

isc_result_t
dns_dnssec_signmessage(dns_message_t *msg, dst_key_t *key);
/*
 *	Signs a message with a SIG(0) record.  This is implicitly called by
 *	dns_message_renderend() if msg->sig0key is not NULL.
 *
 *	Requires:
 *		'msg' is a valid message
 *		'key' is a valid key that can be used for signing
 *
 *	Returns:
 *		ISC_R_SUCCESS
 *		ISC_R_NOMEMORY
 *		DST_R_*
 */

isc_result_t
dns_dnssec_verifymessage(isc_buffer_t *source, dns_message_t *msg,
			 dst_key_t *key);
/*
 *	Verifies a message signed by a SIG(0) record.  This is not
 *	called implicitly by dns_message_parse().  If dns_message_signer()
 *	is called before dns_dnssec_verifymessage(), it will return
 *	DNS_R_NOTVERIFIEDYET.  dns_dnssec_verifymessage() will set
 *	the verified_sig0 flag in msg if the verify succeeds, and
 *	the sig0status field otherwise.
 *
 *	Requires:
 *		'source' is a valid buffer containing the unparsed message
 *		'msg' is a valid message
 *		'key' is a valid key
 *
 *	Returns:
 *		ISC_R_SUCCESS
 *		ISC_R_NOMEMORY
 *		ISC_R_NOTFOUND - no SIG(0) was found
 *		DNS_R_SIGINVALID - the SIG record is not well-formed or
 *				   was not generated by the key.
 *		DST_R_*
 */

ISC_LANG_ENDDECLS

#endif /* DNS_DNSSEC_H */
