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

/* $Id: tsig.h,v 1.40.2.2.8.3 2004/03/08 09:04:39 marka Exp $ */

#ifndef DNS_TSIG_H
#define DNS_TSIG_H 1

#include <isc/lang.h>
#include <isc/refcount.h>
#include <isc/rwlock.h>
#include <isc/stdtime.h>

#include <dns/types.h>
#include <dns/name.h>

#include <dst/dst.h>

/*
 * Algorithms.
 */
LIBDNS_EXTERNAL_DATA extern dns_name_t *dns_tsig_hmacmd5_name;
#define DNS_TSIG_HMACMD5_NAME		dns_tsig_hmacmd5_name
LIBDNS_EXTERNAL_DATA extern dns_name_t *dns_tsig_gssapi_name;
#define DNS_TSIG_GSSAPI_NAME		dns_tsig_gssapi_name
LIBDNS_EXTERNAL_DATA extern dns_name_t *dns_tsig_gssapims_name;
#define DNS_TSIG_GSSAPIMS_NAME		dns_tsig_gssapims_name

/*
 * Default fudge value.
 */
#define DNS_TSIG_FUDGE			300

struct dns_tsig_keyring {
	dns_rbt_t *keys;
	isc_rwlock_t lock;
	isc_mem_t *mctx;
};

struct dns_tsigkey {
	/* Unlocked */
	unsigned int		magic;		/* Magic number. */
	isc_mem_t		*mctx;
	dst_key_t		*key;		/* Key */
	dns_name_t		name;		/* Key name */
	dns_name_t		*algorithm;	/* Algorithm name */
	dns_name_t		*creator;	/* name that created secret */
	isc_boolean_t		generated;	/* was this generated? */
	isc_stdtime_t		inception;	/* start of validity period */
	isc_stdtime_t		expire;		/* end of validity period */
	dns_tsig_keyring_t	*ring;		/* the enclosing keyring */
	isc_refcount_t		refs;		/* reference counter */
};

#define dns_tsigkey_identity(tsigkey) \
	((tsigkey)->generated ? ((tsigkey)->creator) : (&((tsigkey)->name)))

ISC_LANG_BEGINDECLS

isc_result_t
dns_tsigkey_create(dns_name_t *name, dns_name_t *algorithm,
		   unsigned char *secret, int length, isc_boolean_t generated,
		   dns_name_t *creator, isc_stdtime_t inception,
		   isc_stdtime_t expire, isc_mem_t *mctx,
		   dns_tsig_keyring_t *ring, dns_tsigkey_t **key);

isc_result_t
dns_tsigkey_createfromkey(dns_name_t *name, dns_name_t *algorithm,
			  dst_key_t *dstkey, isc_boolean_t generated,
			  dns_name_t *creator, isc_stdtime_t inception,
			  isc_stdtime_t expire, isc_mem_t *mctx,
			  dns_tsig_keyring_t *ring, dns_tsigkey_t **key);
/*
 *	Creates a tsig key structure and saves it in the keyring.  If key is
 *	not NULL, *key will contain a copy of the key.  The keys validity
 *	period is specified by (inception, expire), and will not expire if
 *	inception == expire.  If the key was generated, the creating identity,
 *	if there is one, should be in the creator parameter.  Specifying an
 *	unimplemented algorithm will cause failure only if dstkey != NULL; this
 *	allows a transient key with an invalid algorithm to exist long enough
 *	to generate a BADKEY response.
 *
 *	Requires:
 *		'name' is a valid dns_name_t
 *		'algorithm' is a valid dns_name_t
 *		'secret' is a valid pointer
 *		'length' is an integer >= 0
 *		'key' is a valid dst key or NULL
 *		'creator' points to a valid dns_name_t or is NULL
 *		'mctx' is a valid memory context
 *		'ring' is a valid TSIG keyring or NULL
 *		'key' or '*key' must be NULL
 *
 *	Returns:
 *		ISC_R_SUCCESS
 *		ISC_R_EXISTS - a key with this name already exists
 *		ISC_R_NOTIMPLEMENTED - algorithm is not implemented
 *		ISC_R_NOMEMORY
 */

void
dns_tsigkey_attach(dns_tsigkey_t *source, dns_tsigkey_t **targetp);
/*
 *	Attach '*targetp' to 'source'.
 *
 *	Requires:
 *		'key' is a valid TSIG key
 *
 *	Ensures:
 *		*targetp is attached to source.
 */

void
dns_tsigkey_detach(dns_tsigkey_t **keyp);
/*
 *	Detaches from the tsig key structure pointed to by '*key'.
 *
 *	Requires:
 *		'keyp' is not NULL and '*keyp' is a valid TSIG key
 *
 *	Ensures:
 *		'keyp' points to NULL
 */

void
dns_tsigkey_setdeleted(dns_tsigkey_t *key);
/*
 *	Prevents this key from being used again.  It will be deleted when
 *	no references exist.
 *
 *	Requires:
 *		'key' is a valid TSIG key on a keyring
 */

isc_result_t
dns_tsig_sign(dns_message_t *msg);
/*
 *	Generates a TSIG record for this message
 *
 *	Requires:
 *		'msg' is a valid message
 *		'msg->tsigkey' is a valid TSIG key
 *		'msg->tsig' is NULL
 *
 *	Returns:
 *		ISC_R_SUCCESS
 *		ISC_R_NOMEMORY
 *		ISC_R_NOSPACE
 *		DNS_R_EXPECTEDTSIG
 *			- this is a response & msg->querytsig is NULL
 */

isc_result_t
dns_tsig_verify(isc_buffer_t *source, dns_message_t *msg,
		dns_tsig_keyring_t *ring1, dns_tsig_keyring_t *ring2);
/*
 *	Verifies the TSIG record in this message
 *
 *	Requires:
 *		'source' is a valid buffer containing the unparsed message
 *		'msg' is a valid message
 *		'msg->tsigkey' is a valid TSIG key if this is a response
 *		'msg->tsig' is NULL
 *		'msg->querytsig' is not NULL if this is a response
 *		'ring1' and 'ring2' are each either a valid keyring or NULL
 *
 *	Returns:
 *		ISC_R_SUCCESS
 *		ISC_R_NOMEMORY
 *		DNS_R_EXPECTEDTSIG - A TSIG was expected but not seen
 *		DNS_R_UNEXPECTEDTSIG - A TSIG was seen but not expected
 *		DNS_R_TSIGERRORSET - the TSIG verified but ->error was set
 *				     and this is a query
 *		DNS_R_CLOCKSKEW - the TSIG failed to verify because of
 *				  the time was out of the allowed range.
 *		DNS_R_TSIGVERIFYFAILURE - the TSIG failed to verify
 *		DNS_R_EXPECTEDRESPONSE - the message was set over TCP and
 *					 should have been a response,
 *					 but was not.
 */

isc_result_t
dns_tsigkey_find(dns_tsigkey_t **tsigkey, dns_name_t *name,
		 dns_name_t *algorithm, dns_tsig_keyring_t *ring);
/*
 *	Returns the TSIG key corresponding to this name and (possibly)
 *	algorithm.  Also increments the key's reference counter.
 *
 *	Requires:
 *		'tsigkey' is not NULL
 *		'*tsigkey' is NULL
 *		'name' is a valid dns_name_t
 *		'algorithm' is a valid dns_name_t or NULL
 *		'ring' is a valid keyring
 *
 *	Returns:
 *		ISC_R_SUCCESS
 *		ISC_R_NOTFOUND
 */


isc_result_t
dns_tsigkeyring_create(isc_mem_t *mctx, dns_tsig_keyring_t **ringp);
/*
 *	Create an empty TSIG key ring.
 *
 *	Requires:
 *		'mctx' is not NULL
 *		'ringp' is not NULL, and '*ringp' is NULL
 *
 *	Returns:
 *		ISC_R_SUCCESS
 *		ISC_R_NOMEMORY
 */


void
dns_tsigkeyring_destroy(dns_tsig_keyring_t **ringp);
/*
 *	Destroy a TSIG key ring.
 *
 *	Requires:
 *		'ringp' is not NULL
 */

ISC_LANG_ENDDECLS

#endif /* DNS_TSIG_H */
