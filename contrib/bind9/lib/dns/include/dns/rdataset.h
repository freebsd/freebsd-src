/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2003  Internet Software Consortium.
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

/* $Id: rdataset.h,v 1.41.2.5.2.6 2004/03/08 02:08:01 marka Exp $ */

#ifndef DNS_RDATASET_H
#define DNS_RDATASET_H 1

/*****
 ***** Module Info
 *****/

/*
 * DNS Rdataset
 *
 * A DNS rdataset is a handle that can be associated with a collection of
 * rdata all having a common owner name, class, and type.
 *
 * The dns_rdataset_t type is like a "virtual class".  To actually use
 * rdatasets, an implementation of the method suite (e.g. "slabbed rdata") is
 * required.
 *
 * XXX <more> XXX
 *
 * MP:
 *	Clients of this module must impose any required synchronization.
 *
 * Reliability:
 *	No anticipated impact.
 *
 * Resources:
 *	<TBS>
 *
 * Security:
 *	No anticipated impact.
 *
 * Standards:
 *	None.
 */

#include <isc/lang.h>
#include <isc/magic.h>

#include <dns/types.h>

ISC_LANG_BEGINDECLS

typedef struct dns_rdatasetmethods {
	void			(*disassociate)(dns_rdataset_t *rdataset);
	isc_result_t		(*first)(dns_rdataset_t *rdataset);
	isc_result_t		(*next)(dns_rdataset_t *rdataset);
	void			(*current)(dns_rdataset_t *rdataset,
					   dns_rdata_t *rdata);
	void			(*clone)(dns_rdataset_t *source,
					 dns_rdataset_t *target);
	unsigned int		(*count)(dns_rdataset_t *rdataset);
	isc_result_t		(*addnoqname)(dns_rdataset_t *rdataset,
					      dns_name_t *name);
	isc_result_t		(*getnoqname)(dns_rdataset_t *rdataset,
					      dns_name_t *name,
					      dns_rdataset_t *nsec,
					      dns_rdataset_t *nsecsig);
} dns_rdatasetmethods_t;

#define DNS_RDATASET_MAGIC	       ISC_MAGIC('D','N','S','R')
#define DNS_RDATASET_VALID(set)	       ISC_MAGIC_VALID(set, DNS_RDATASET_MAGIC)

/*
 * Direct use of this structure by clients is strongly discouraged, except
 * for the 'link' field which may be used however the client wishes.  The
 * 'private', 'current', and 'index' fields MUST NOT be changed by clients.
 * rdataset implementations may change any of the fields.
 */
struct dns_rdataset {
	unsigned int			magic;		/* XXX ? */
	dns_rdatasetmethods_t *		methods;
	ISC_LINK(dns_rdataset_t)	link;
	/*
	 * XXX do we need these, or should they be retrieved by methods?
	 * Leaning towards the latter, since they are not frequently required
	 * once you have the rdataset.
	 */
	dns_rdataclass_t		rdclass;
	dns_rdatatype_t			type;
	dns_ttl_t			ttl;
	dns_trust_t			trust;
	dns_rdatatype_t			covers;
	/*
	 * attributes
	 */
	unsigned int			attributes;
	/*
	 * the counter provides the starting point in the "cyclic" order.
	 * The value ISC_UINT32_MAX has a special meaning of "picking up a
	 * random value." in order to take care of databases that do not
	 * increment the counter.
	 */
	isc_uint32_t			count;
	/*
	 * These are for use by the rdataset implementation, and MUST NOT
	 * be changed by clients.
	 */
	void *				private1;
	void *				private2;
	void *				private3;
	unsigned int			privateuint4;
	void *				private5;
	void *				private6;
};

/*
 * _RENDERED:
 *	Used by message.c to indicate that the rdataset was rendered.
 *
 * _TTLADJUSTED:
 *	Used by message.c to indicate that the rdataset's rdata had differing
 *	TTL values, and the rdataset->ttl holds the smallest.
 */
#define DNS_RDATASETATTR_QUESTION	0x0001
#define DNS_RDATASETATTR_RENDERED	0x0002		/* Used by message.c */
#define DNS_RDATASETATTR_ANSWERED	0x0004		/* Used by server. */
#define DNS_RDATASETATTR_CACHE		0x0008		/* Used by resolver. */
#define DNS_RDATASETATTR_ANSWER		0x0010		/* Used by resolver. */
#define DNS_RDATASETATTR_ANSWERSIG	0x0020		/* Used by resolver. */
#define DNS_RDATASETATTR_EXTERNAL	0x0040		/* Used by resolver. */
#define DNS_RDATASETATTR_NCACHE		0x0080		/* Used by resolver. */
#define DNS_RDATASETATTR_CHAINING	0x0100		/* Used by resolver. */
#define DNS_RDATASETATTR_TTLADJUSTED	0x0200		/* Used by message.c */
#define DNS_RDATASETATTR_FIXEDORDER	0x0400
#define DNS_RDATASETATTR_RANDOMIZE	0x0800
#define DNS_RDATASETATTR_CHASE		0x1000		/* Used by resolver. */
#define DNS_RDATASETATTR_NXDOMAIN	0x2000
#define DNS_RDATASETATTR_NOQNAME	0x4000
#define DNS_RDATASETATTR_CHECKNAMES	0x8000		/* Used by resolver. */

/*
 * _OMITDNSSEC:
 * 	Omit DNSSEC records when rendering ncache records.
 */
#define DNS_RDATASETTOWIRE_OMITDNSSEC	0x0001

void
dns_rdataset_init(dns_rdataset_t *rdataset);
/*
 * Make 'rdataset' a valid, disassociated rdataset.
 *
 * Requires:
 *	'rdataset' is not NULL.
 *
 * Ensures:
 *	'rdataset' is a valid, disassociated rdataset.
 */

void
dns_rdataset_invalidate(dns_rdataset_t *rdataset);
/*
 * Invalidate 'rdataset'.
 *
 * Requires:
 *	'rdataset' is a valid, disassociated rdataset.
 *
 * Ensures:
 *	If assertion checking is enabled, future attempts to use 'rdataset'
 *	without initializing it will cause an assertion failure.
 */

void
dns_rdataset_disassociate(dns_rdataset_t *rdataset);
/*
 * Disassociate 'rdataset' from its rdata, allowing it to be reused.
 *
 * Notes:
 *	The client must ensure it has no references to rdata in the rdataset
 *	before disassociating.
 *
 * Requires:
 *	'rdataset' is a valid, associated rdataset.
 *
 * Ensures:
 *	'rdataset' is a valid, disassociated rdataset.
 */

isc_boolean_t
dns_rdataset_isassociated(dns_rdataset_t *rdataset);
/*
 * Is 'rdataset' associated?
 *
 * Requires:
 *	'rdataset' is a valid rdataset.
 *
 * Returns:
 *	ISC_TRUE			'rdataset' is associated.
 *	ISC_FALSE			'rdataset' is not associated.
 */

void
dns_rdataset_makequestion(dns_rdataset_t *rdataset, dns_rdataclass_t rdclass,
			  dns_rdatatype_t type);
/*
 * Make 'rdataset' a valid, associated, question rdataset, with a
 * question class of 'rdclass' and type 'type'.
 *
 * Notes:
 *	Question rdatasets have a class and type, but no rdata.
 *
 * Requires:
 *	'rdataset' is a valid, disassociated rdataset.
 *
 * Ensures:
 *	'rdataset' is a valid, associated, question rdataset.
 */

void
dns_rdataset_clone(dns_rdataset_t *source, dns_rdataset_t *target);
/*
 * Make 'target' refer to the same rdataset as 'source'.
 *
 * Requires:
 *	'source' is a valid, associated rdataset.
 *
 *	'target' is a valid, dissociated rdataset.
 *
 * Ensures:
 *	'target' references the same rdataset as 'source'.
 */

unsigned int
dns_rdataset_count(dns_rdataset_t *rdataset);
/*
 * Return the number of records in 'rdataset'.
 *
 * Requires:
 *	'rdataset' is a valid, associated rdataset.
 *
 * Returns:
 *	The number of records in 'rdataset'.
 */

isc_result_t
dns_rdataset_first(dns_rdataset_t *rdataset);
/*
 * Move the rdata cursor to the first rdata in the rdataset (if any).
 *
 * Requires:
 *	'rdataset' is a valid, associated rdataset.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOMORE			There are no rdata in the set.
 */

isc_result_t
dns_rdataset_next(dns_rdataset_t *rdataset);
/*
 * Move the rdata cursor to the next rdata in the rdataset (if any).
 *
 * Requires:
 *	'rdataset' is a valid, associated rdataset.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOMORE			There are no more rdata in the set.
 */

void
dns_rdataset_current(dns_rdataset_t *rdataset, dns_rdata_t *rdata);
/*
 * Make 'rdata' refer to the current rdata.
 *
 * Notes:
 *
 *	The data returned in 'rdata' is valid for the life of the
 *	rdataset; in particular, subsequent changes in the cursor position
 *	do not invalidate 'rdata'.
 *
 * Requires:
 *	'rdataset' is a valid, associated rdataset.
 *
 *	The rdata cursor of 'rdataset' is at a valid location (i.e. the
 *	result of last call to a cursor movement command was ISC_R_SUCCESS).
 *
 * Ensures:
 *	'rdata' refers to the rdata at the rdata cursor location of
 *	'rdataset'.
 */

isc_result_t
dns_rdataset_totext(dns_rdataset_t *rdataset,
		    dns_name_t *owner_name,
		    isc_boolean_t omit_final_dot,
		    isc_boolean_t question,
		    isc_buffer_t *target);
/*
 * Convert 'rdataset' to text format, storing the result in 'target'.
 *
 * Notes:
 *	The rdata cursor position will be changed.
 *
 *	The 'question' flag should normally be ISC_FALSE.  If it is 
 *	ISC_TRUE, the TTL and rdata fields are not printed.  This is 
 *	for use when printing an rdata representing a question section.
 *
 *	This interface is deprecated; use dns_master_rdatasettottext()
 * 	and/or dns_master_questiontotext() instead.
 *
 * Requires:
 *	'rdataset' is a valid rdataset.
 *
 *	'rdataset' is not empty.
 */

isc_result_t
dns_rdataset_towire(dns_rdataset_t *rdataset,
		    dns_name_t *owner_name,
		    dns_compress_t *cctx,
		    isc_buffer_t *target,
		    unsigned int options,
		    unsigned int *countp);
/*
 * Convert 'rdataset' to wire format, compressing names as specified
 * in 'cctx', and storing the result in 'target'.
 *
 * Notes:
 *	The rdata cursor position will be changed.
 *
 *	The number of RRs added to target will be added to *countp.
 *
 * Requires:
 *	'rdataset' is a valid rdataset.
 *
 *	'rdataset' is not empty.
 *
 *	'countp' is a valid pointer.
 *
 * Ensures:
 *	On a return of ISC_R_SUCCESS, 'target' contains a wire format
 *	for the data contained in 'rdataset'.  Any error return leaves
 *	the buffer unchanged.
 *
 *	*countp has been incremented by the number of RRs added to
 *	target.
 *
 * Returns:
 *	ISC_R_SUCCESS		- all ok
 *	ISC_R_NOSPACE		- 'target' doesn't have enough room
 *
 *	Any error returned by dns_rdata_towire(), dns_rdataset_next(),
 *	dns_name_towire().
 */

isc_result_t
dns_rdataset_towiresorted(dns_rdataset_t *rdataset,
			  dns_name_t *owner_name,
			  dns_compress_t *cctx,
			  isc_buffer_t *target,
			  dns_rdatasetorderfunc_t order,
			  void *order_arg,
			  unsigned int options,
			  unsigned int *countp);
/*
 * Like dns_rdataset_towire(), but sorting the rdatasets according to
 * the integer value returned by 'order' when called witih the rdataset
 * and 'order_arg' as arguments.
 *
 * Requires:
 *	All the requirements of dns_rdataset_towire(), and
 *	that order_arg is NULL if and only if order is NULL.
 */

isc_result_t
dns_rdataset_towirepartial(dns_rdataset_t *rdataset,
			   dns_name_t *owner_name,
			   dns_compress_t *cctx,
			   isc_buffer_t *target,
			   dns_rdatasetorderfunc_t order,
			   void *order_arg,
			   unsigned int options,
			   unsigned int *countp,
			   void **state);
/*
 * Like dns_rdataset_towiresorted() except that a partial rdataset
 * may be written.
 *
 * Requires:
 *	All the requirements of dns_rdataset_towiresorted().
 *	If 'state' is non NULL then the current position in the
 *	rdataset will be remembered if the rdataset in not
 *	completely written and should be passed on on subsequent
 *	calls (NOT CURRENTLY IMPLEMENTED).
 *
 * Returns:
 *	ISC_R_SUCCESS if all of the records were written.
 *	ISC_R_NOSPACE if unable to fit in all of the records. *countp
 *		      will be updated to reflect the number of records
 *		      written.
 */


isc_result_t
dns_rdataset_additionaldata(dns_rdataset_t *rdataset,
			    dns_additionaldatafunc_t add, void *arg);
/*
 * For each rdata in rdataset, call 'add' for each name and type in the
 * rdata which is subject to additional section processing.
 *
 * Requires:
 *
 *	'rdataset' is a valid, non-question rdataset.
 *
 *	'add' is a valid dns_additionaldatafunc_t
 *
 * Ensures:
 *
 *	If successful, dns_rdata_additionaldata() will have been called for
 *	each rdata in 'rdataset'.
 *
 *	If a call to dns_rdata_additionaldata() is not successful, the
 *	result returned will be the result of dns_rdataset_additionaldata().
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *
 *	Any error that dns_rdata_additionaldata() can return.
 */

isc_result_t
dns_rdataset_getnoqname(dns_rdataset_t *rdataset, dns_name_t *name,
			dns_rdataset_t *nsec, dns_rdataset_t *nsecsig);
/*
 * Return the noqname proof for this record.
 *
 * Requires:
 *	'rdataset' to be valid and DNS_RDATASETATTR_NOQNAME to be set.
 *	'name' to be valid.
 *	'nsec' and 'nsecsig' to be valid and not associated.
 */

isc_result_t
dns_rdataset_addnoqname(dns_rdataset_t *rdataset, dns_name_t *name);
/*
 * Associate a noqname proof with this record.
 * Sets DNS_RDATASETATTR_NOQNAME if successful.
 * Adjusts the 'rdataset->ttl' to minimum of the 'rdataset->ttl' and
 * the 'nsec' and 'rrsig(nsec)' ttl.
 *
 * Requires:
 *	'rdataset' to be valid and DNS_RDATASETATTR_NOQNAME to be set.
 *	'name' to be valid and have NSEC and RRSIG(NSEC) rdatasets.
 */

ISC_LANG_ENDDECLS

#endif /* DNS_RDATASET_H */
