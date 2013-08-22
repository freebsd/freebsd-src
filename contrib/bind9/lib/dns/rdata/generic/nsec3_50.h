/*
 * Copyright (C) 2008, 2011, 2012  Internet Systems Consortium, Inc. ("ISC")
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


#ifndef GENERIC_NSEC3_50_H
#define GENERIC_NSEC3_50_H 1

/* $Id$ */

/*!
 * \brief Per RFC 5155 */

#include <isc/iterated_hash.h>

typedef struct dns_rdata_nsec3 {
	dns_rdatacommon_t	common;
	isc_mem_t		*mctx;
	dns_hash_t		hash;
	unsigned char		flags;
	dns_iterations_t	iterations;
	unsigned char		salt_length;
	unsigned char		next_length;
	isc_uint16_t		len;
	unsigned char		*salt;
	unsigned char		*next;
	unsigned char		*typebits;
} dns_rdata_nsec3_t;

/*
 * The corresponding NSEC3 interval is OPTOUT indicating possible
 * insecure delegations.
 */
#define DNS_NSEC3FLAG_OPTOUT 0x01U

/*%
 * The following flags are used in the private-type record (implemented in
 * lib/dns/private.c) which is used to store NSEC3PARAM data during the
 * time when it is not legal to have an actual NSEC3PARAM record in the
 * zone.  They are defined here because the private-type record uses the
 * same flags field for the OPTOUT flag above and for the private flags
 * below.  XXX: This should be considered for refactoring.
 */

/*%
 * Non-standard, private type only.
 *
 * Create a corresponding NSEC3 chain.
 * Once the NSEC3 chain is complete this flag will be removed to signal
 * that there is a complete chain.
 *
 * This flag is automatically set when a NSEC3PARAM record is added to
 * the zone via UPDATE.
 *
 * NSEC3PARAM records containing this flag should never be published,
 * but if they are, they should be ignored by RFC 5155 compliant
 * nameservers.
 */
#define DNS_NSEC3FLAG_CREATE 0x80U

/*%
 * Non-standard, private type only.
 *
 * The corresponding NSEC3 set is to be removed once the NSEC chain
 * has been generated.
 *
 * This flag is automatically set when the last active NSEC3PARAM record
 * is removed from the zone via UPDATE.
 *
 * NSEC3PARAM records containing this flag should never be published,
 * but if they are, they should be ignored by RFC 5155 compliant
 * nameservers.
 */
#define DNS_NSEC3FLAG_REMOVE 0x40U

/*%
 * Non-standard, private type only.
 *
 * When set with the CREATE flag, a corresponding NSEC3 chain will be
 * created when the zone becomes capable of supporting one (i.e., when it
 * has a DNSKEY RRset containing at least one NSEC3-capable algorithm).
 * Without this flag, NSEC3 chain creation would be attempted immediately,
 * fail, and the private type record would be removed.  With it, the NSEC3
 * parameters are stored until they can be used.  When the zone has the
 * necessary prerequisites for NSEC3, then the INITIAL flag can be cleared,
 * and the record will be cleaned up normally.
 *
 * NSEC3PARAM records containing this flag should never be published, but
 * if they are, they should be ignored by RFC 5155 compliant nameservers.
 */
#define DNS_NSEC3FLAG_INITIAL 0x20U

/*%
 * Non-standard, private type only.
 *
 * Prevent the creation of a NSEC chain before the last NSEC3 chain
 * is removed.  This will normally only be set when the zone is
 * transitioning from secure with NSEC3 chains to insecure.
 *
 * NSEC3PARAM records containing this flag should never be published,
 * but if they are, they should be ignored by RFC 5155 compliant
 * nameservers.
 */
#define DNS_NSEC3FLAG_NONSEC 0x10U

#endif /* GENERIC_NSEC3_50_H */
