/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1998-2003  Internet Software Consortium.
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

/* $Id: name.h,v 1.95.2.3.2.12 2004/09/08 00:29:34 marka Exp $ */

#ifndef DNS_NAME_H
#define DNS_NAME_H 1

/*****
 ***** Module Info
 *****/

/*
 * DNS Names and Labels
 *
 * Provides facilities for manipulating DNS names and labels, including
 * conversions to and from wire format and text format.
 *
 * Given the large number of names possible in a nameserver, and because
 * names occur in rdata, it was important to come up with a very efficient
 * way of storing name data, but at the same time allow names to be
 * manipulated.  The decision was to store names in uncompressed wire format,
 * and not to make them fully abstracted objects; i.e. certain parts of the
 * server know names are stored that way.  This saves a lot of memory, and
 * makes adding names to messages easy.  Having much of the server know
 * the representation would be perilous, and we certainly don't want each
 * user of names to be manipulating such a low-level structure.  This is
 * where the Names and Labels module comes in.  The module allows name or
 * label handles to be created and attached to uncompressed wire format
 * regions.  All name operations and conversions are done through these
 * handles.
 *
 * MP:
 *	Clients of this module must impose any required synchronization.
 *
 * Reliability:
 *	This module deals with low-level byte streams.  Errors in any of
 *	the functions are likely to crash the server or corrupt memory.
 *
 * Resources:
 *	None.
 *
 * Security:
 *
 *	*** WARNING ***
 *
 *	dns_name_fromwire() deals with raw network data.  An error in
 *	this routine could result in the failure or hijacking of the server.
 *
 * Standards:
 *	RFC 1035
 *	Draft EDNS0 (0)
 *	Draft Binary Labels (2)
 *
 */

/***
 *** Imports
 ***/

#include <stdio.h>

#include <isc/boolean.h>
#include <isc/lang.h>
#include <isc/magic.h>
#include <isc/region.h>		/* Required for storage size of dns_label_t. */

#include <dns/types.h>

ISC_LANG_BEGINDECLS

/*****
 ***** Labels
 *****
 ***** A 'label' is basically a region.  It contains one DNS wire format
 ***** label of type 00 (ordinary).
 *****/

/*****
 ***** Names
 *****
 ***** A 'name' is a handle to a binary region.  It contains a sequence of one
 ***** or more DNS wire format labels of type 00 (ordinary).
 ***** Note that all names are not required to end with the root label,
 ***** as they are in the actual DNS wire protocol.
 *****/

/***
 *** Compression pointer chaining limit
 ***/

#define DNS_POINTER_MAXHOPS		16

/***
 *** Types
 ***/

/*
 * Clients are strongly discouraged from using this type directly,  with
 * the exception of the 'link' and 'list' fields which may be used directly
 * for whatever purpose the client desires.
 */
struct dns_name {
	unsigned int			magic;
	unsigned char *			ndata;
	unsigned int			length;
	unsigned int			labels;
	unsigned int			attributes;
	unsigned char *			offsets;
	isc_buffer_t *			buffer;
	ISC_LINK(dns_name_t)		link;
	ISC_LIST(dns_rdataset_t)	list;
};

#define DNS_NAME_MAGIC			ISC_MAGIC('D','N','S','n')

#define DNS_NAMEATTR_ABSOLUTE		0x0001
#define DNS_NAMEATTR_READONLY		0x0002
#define DNS_NAMEATTR_DYNAMIC		0x0004
#define DNS_NAMEATTR_DYNOFFSETS		0x0008
/*
 * Attributes below 0x0100 reserved for name.c usage.
 */
#define DNS_NAMEATTR_CACHE		0x0100		/* Used by resolver. */
#define DNS_NAMEATTR_ANSWER		0x0200		/* Used by resolver. */
#define DNS_NAMEATTR_NCACHE		0x0400		/* Used by resolver. */
#define DNS_NAMEATTR_CHAINING		0x0800		/* Used by resolver. */
#define DNS_NAMEATTR_CHASE		0x1000		/* Used by resolver. */
#define DNS_NAMEATTR_WILDCARD		0x2000		/* Used by server. */

#define DNS_NAME_DOWNCASE		0x0001
#define DNS_NAME_CHECKNAMES		0x0002		/* Used by rdata. */
#define DNS_NAME_CHECKNAMESFAIL		0x0004		/* Used by rdata. */
#define DNS_NAME_CHECKREVERSE		0x0008		/* Used by rdata. */

LIBDNS_EXTERNAL_DATA extern dns_name_t *dns_rootname;
LIBDNS_EXTERNAL_DATA extern dns_name_t *dns_wildcardname;

/*
 * Standard size of a wire format name
 */
#define DNS_NAME_MAXWIRE 255

/***
 *** Initialization
 ***/

void
dns_name_init(dns_name_t *name, unsigned char *offsets);
/*
 * Initialize 'name'.
 *
 * Notes:
 *	'offsets' is never required to be non-NULL, but specifying a
 *	dns_offsets_t for 'offsets' will improve the performance of most
 *	name operations if the name is used more than once.
 *
 * Requires:
 *	'name' is not NULL and points to a struct dns_name.
 *
 *	offsets == NULL or offsets is a dns_offsets_t.
 *
 * Ensures:
 *	'name' is a valid name.
 *	dns_name_countlabels(name) == 0
 *	dns_name_isabsolute(name) == ISC_FALSE
 */

void
dns_name_reset(dns_name_t *name);
/*
 * Reinitialize 'name'.
 *
 * Notes:
 *	This function distinguishes itself from dns_name_init() in two
 *	key ways:
 *
 *	+ If any buffer is associated with 'name' (via dns_name_setbuffer()
 *	  or by being part of a dns_fixedname_t) the link to the buffer
 *	  is retained but the buffer itself is cleared.
 *
 *	+ Of the attributes associated with 'name', all are retained except
 *	  DNS_NAMEATTR_ABSOLUTE.
 *
 * Requires:
 *	'name' is a valid name.
 *
 * Ensures:
 *	'name' is a valid name.
 *	dns_name_countlabels(name) == 0
 *	dns_name_isabsolute(name) == ISC_FALSE
 */

void
dns_name_invalidate(dns_name_t *name);
/*
 * Make 'name' invalid.
 *
 * Requires:
 *	'name' is a valid name.
 *
 * Ensures:
 *	If assertion checking is enabled, future attempts to use 'name'
 *	without initializing it will cause an assertion failure.
 *
 *	If the name had a dedicated buffer, that association is ended.
 */


/***
 *** Dedicated Buffers
 ***/

void
dns_name_setbuffer(dns_name_t *name, isc_buffer_t *buffer);
/*
 * Dedicate a buffer for use with 'name'.
 *
 * Notes:
 *	Specification of a target buffer in dns_name_fromwire(),
 *	dns_name_fromtext(), and dns_name_concatentate() is optional if
 *	'name' has a dedicated buffer.
 *
 *	The caller must not write to buffer until the name has been
 *	invalidated or is otherwise known not to be in use.
 *
 *	If buffer is NULL and the name previously had a dedicated buffer,
 *	than that buffer is no longer dedicated to use with this name.
 *	The caller is responsible for ensuring that the storage used by
 *	the name remains valid.
 *
 * Requires:
 *	'name' is a valid name.
 *
 *	'buffer' is a valid binary buffer and 'name' doesn't have a
 *	dedicated buffer already, or 'buffer' is NULL.
 */

isc_boolean_t
dns_name_hasbuffer(const dns_name_t *name);
/*
 * Does 'name' have a dedicated buffer?
 *
 * Requires:
 *	'name' is a valid name.
 *
 * Returns:
 *	ISC_TRUE	'name' has a dedicated buffer.
 *	ISC_FALSE	'name' does not have a dedicated buffer.
 */


/***
 *** Properties
 ***/

isc_boolean_t
dns_name_isabsolute(const dns_name_t *name);
/*
 * Does 'name' end in the root label?
 *
 * Requires:
 *	'name' is a valid name
 *
 * Returns:
 *	TRUE		The last label in 'name' is the root label.
 *	FALSE		The last label in 'name' is not the root label.
 */

isc_boolean_t
dns_name_iswildcard(const dns_name_t *name);
/*
 * Is 'name' a wildcard name?
 *
 * Requires:
 *	'name' is a valid name
 *
 *	dns_name_countlabels(name) > 0
 *
 * Returns:
 *	TRUE		The least significant label of 'name' is '*'.
 *	FALSE		The least significant label of 'name' is not '*'.
 */

unsigned int
dns_name_hash(dns_name_t *name, isc_boolean_t case_sensitive);
/*
 * Provide a hash value for 'name'.
 *
 * Note: if 'case_sensitive' is ISC_FALSE, then names which differ only in
 * case will have the same hash value.
 *
 * Requires:
 *	'name' is a valid name
 *
 * Returns:
 *	A hash value
 */

unsigned int
dns_name_fullhash(dns_name_t *name, isc_boolean_t case_sensitive);
/*
 * Provide a hash value for 'name'.  Unlike dns_name_hash(), this function
 * always takes into account of the entire name to calculate the hash value.
 *
 * Note: if 'case_sensitive' is ISC_FALSE, then names which differ only in
 * case will have the same hash value.
 *
 * Requires:
 *	'name' is a valid name
 *
 * Returns:
 *	A hash value
 */

unsigned int
dns_name_hashbylabel(dns_name_t *name, isc_boolean_t case_sensitive);
/*
 * Provide a hash value for 'name', where the hash value is the sum
 * of the hash values of each label.
 *
 * Note: if 'case_sensitive' is ISC_FALSE, then names which differ only in
 * case will have the same hash value.
 *
 * Requires:
 *	'name' is a valid name
 *
 * Returns:
 *	A hash value
 */

/***
 *** Comparisons
 ***/

dns_namereln_t
dns_name_fullcompare(const dns_name_t *name1, const dns_name_t *name2,
		     int *orderp, unsigned int *nlabelsp);
/*
 * Determine the relative ordering under the DNSSEC order relation of
 * 'name1' and 'name2', and also determine the hierarchical
 * relationship of the names.
 *
 * Note: It makes no sense for one of the names to be relative and the
 * other absolute.  If both names are relative, then to be meaningfully
 * compared the caller must ensure that they are both relative to the
 * same domain.
 *
 * Requires:
 *	'name1' is a valid name
 *
 *	dns_name_countlabels(name1) > 0
 *
 *	'name2' is a valid name
 *
 *	dns_name_countlabels(name2) > 0
 *
 *	orderp and nlabelsp are valid pointers.
 *
 *	Either name1 is absolute and name2 is absolute, or neither is.
 *
 * Ensures:
 *
 *	*orderp is < 0 if name1 < name2, 0 if name1 = name2, > 0 if
 *	name1 > name2.
 *
 *	*nlabelsp is the number of common significant labels.
 *
 * Returns:
 *	dns_namereln_none		There's no hierarchical relationship
 *					between name1 and name2.
 *	dns_namereln_contains		name1 properly contains name2; i.e.
 *					name2 is a proper subdomain of name1.
 *	dns_namereln_subdomain		name1 is a proper subdomain of name2.
 *	dns_namereln_equal		name1 and name2 are equal.
 *	dns_namereln_commonancestor	name1 and name2 share a common
 *					ancestor.
 */

int
dns_name_compare(const dns_name_t *name1, const dns_name_t *name2);
/*
 * Determine the relative ordering under the DNSSEC order relation of
 * 'name1' and 'name2'.
 *
 * Note: It makes no sense for one of the names to be relative and the
 * other absolute.  If both names are relative, then to be meaningfully
 * compared the caller must ensure that they are both relative to the
 * same domain.
 *
 * Requires:
 *	'name1' is a valid name
 *
 *	'name2' is a valid name
 *
 *	Either name1 is absolute and name2 is absolute, or neither is.
 *
 * Returns:
 *	< 0		'name1' is less than 'name2'
 *	0		'name1' is equal to 'name2'
 *	> 0		'name1' is greater than 'name2'
 */

isc_boolean_t
dns_name_equal(const dns_name_t *name1, const dns_name_t *name2);
/*
 * Are 'name1' and 'name2' equal?
 *
 * Notes:
 *	Because it only needs to test for equality, dns_name_equal() can be
 *	significantly faster than dns_name_fullcompare() or dns_name_compare().
 *
 *	Offsets tables are not used in the comparision.
 *
 * 	It makes no sense for one of the names to be relative and the
 *	other absolute.  If both names are relative, then to be meaningfully
 * 	compared the caller must ensure that they are both relative to the
 * 	same domain.
 *
 * Requires:
 *	'name1' is a valid name
 *
 *	'name2' is a valid name
 *
 *	Either name1 is absolute and name2 is absolute, or neither is.
 *
 * Returns:
 *	ISC_TRUE	'name1' and 'name2' are equal
 *	ISC_FALSE	'name1' and 'name2' are not equal
 */

int
dns_name_rdatacompare(const dns_name_t *name1, const dns_name_t *name2);
/*
 * Compare two names as if they are part of rdata in DNSSEC canonical
 * form.
 *
 * Requires:
 *	'name1' is a valid absolute name
 *
 *	dns_name_countlabels(name1) > 0
 *
 *	'name2' is a valid absolute name
 *
 *	dns_name_countlabels(name2) > 0
 *
 * Returns:
 *	< 0		'name1' is less than 'name2'
 *	0		'name1' is equal to 'name2'
 *	> 0		'name1' is greater than 'name2'
 */

isc_boolean_t
dns_name_issubdomain(const dns_name_t *name1, const dns_name_t *name2);
/*
 * Is 'name1' a subdomain of 'name2'?
 *
 * Notes:
 *	name1 is a subdomain of name2 if name1 is contained in name2, or
 *	name1 equals name2.
 *
 *	It makes no sense for one of the names to be relative and the
 *	other absolute.  If both names are relative, then to be meaningfully
 *	compared the caller must ensure that they are both relative to the
 *	same domain.
 *
 * Requires:
 *	'name1' is a valid name
 *
 *	'name2' is a valid name
 *
 *	Either name1 is absolute and name2 is absolute, or neither is.
 *
 * Returns:
 *	TRUE		'name1' is a subdomain of 'name2'
 *	FALSE		'name1' is not a subdomain of 'name2'
 */

isc_boolean_t
dns_name_matcheswildcard(const dns_name_t *name, const dns_name_t *wname);
/*
 * Does 'name' match the wildcard specified in 'wname'?
 *
 * Notes:
 *	name matches the wildcard specified in wname if all labels
 *	following the wildcard in wname are identical to the same number
 *	of labels at the end of name.
 *
 *	It makes no sense for one of the names to be relative and the
 *	other absolute.  If both names are relative, then to be meaningfully
 *	compared the caller must ensure that they are both relative to the
 *	same domain.
 *
 * Requires:
 *	'name' is a valid name
 *
 *	dns_name_countlabels(name) > 0
 *
 *	'wname' is a valid name
 *
 *	dns_name_countlabels(wname) > 0
 *
 *	dns_name_iswildcard(wname) is true
 *
 *	Either name is absolute and wname is absolute, or neither is.
 *
 * Returns:
 *	TRUE		'name' matches the wildcard specified in 'wname'
 *	FALSE		'name' does not match the wildcard specified in 'wname'
 */

/***
 *** Labels
 ***/

unsigned int
dns_name_countlabels(const dns_name_t *name);
/*
 * How many labels does 'name' have?
 *
 * Notes:
 *	In this case, as in other places, a 'label' is an ordinary label.
 *
 * Requires:
 *	'name' is a valid name
 *
 * Ensures:
 *	The result is <= 128.
 *
 * Returns:
 *	The number of labels in 'name'.
 */

void
dns_name_getlabel(const dns_name_t *name, unsigned int n, dns_label_t *label);
/*
 * Make 'label' refer to the 'n'th least significant label of 'name'.
 *
 * Notes:
 *	Numbering starts at 0.
 *
 *	Given "rc.vix.com.", the label 0 is "rc", and label 3 is the
 *	root label.
 *
 *	'label' refers to the same memory as 'name', so 'name' must not
 *	be changed while 'label' is still in use.
 *
 * Requires:
 *	n < dns_name_countlabels(name)
 */

void
dns_name_getlabelsequence(const dns_name_t *source, unsigned int first,
			  unsigned int n, dns_name_t *target);
/*
 * Make 'target' refer to the 'n' labels including and following 'first'
 * in 'source'.
 *
 * Notes:
 *	Numbering starts at 0.
 *
 *	Given "rc.vix.com.", the label 0 is "rc", and label 3 is the
 *	root label.
 *
 *	'target' refers to the same memory as 'source', so 'source'
 *	must not be changed while 'target' is still in use.
 *
 * Requires:
 *	'source' and 'target' are valid names.
 *
 *	first < dns_name_countlabels(name)
 *
 *	first + n <= dns_name_countlabels(name)
 */


void
dns_name_clone(dns_name_t *source, dns_name_t *target);
/*
 * Make 'target' refer to the same name as 'source'.
 *
 * Notes:
 *
 *	'target' refers to the same memory as 'source', so 'source'
 *	must not be changed while 'target' is still in use.
 *
 *	This call is functionally equivalent to:
 *
 *		dns_name_getlabelsequence(source, 0,
 *					  dns_name_countlabels(source),
 *					  target);
 *
 *	but is more efficient.  Also, dns_name_clone() works even if 'source'
 *	is empty.
 *
 * Requires:
 *
 *	'source' is a valid name.
 *
 *	'target' is a valid name that is not read-only.
 */

/***
 *** Conversions
 ***/

void
dns_name_fromregion(dns_name_t *name, const isc_region_t *r);
/*
 * Make 'name' refer to region 'r'.
 *
 * Note:
 *	If the conversion encounters a root label before the end of the
 *	region the conversion stops and the length is set to the length
 *	so far converted.  A maximum of 255 bytes is converted.
 *
 * Requires:
 *	The data in 'r' is a sequence of one or more type 00 or type 01000001
 *	labels.
 */

void
dns_name_toregion(dns_name_t *name, isc_region_t *r);
/*
 * Make 'r' refer to 'name'.
 *
 * Requires:
 *
 *	'name' is a valid name.
 *
 *	'r' is a valid region.
 */

isc_result_t
dns_name_fromwire(dns_name_t *name, isc_buffer_t *source,
		  dns_decompress_t *dctx, unsigned int options,
		  isc_buffer_t *target);
/*
 * Copy the possibly-compressed name at source (active region) into target,
 * decompressing it.
 *
 * Notes:
 *	Decompression policy is controlled by 'dctx'.
 *
 *	If DNS_NAME_DOWNCASE is set, any uppercase letters in 'source' will be
 *	downcased when they are copied into 'target'.
 *
 * Security:
 *
 *	*** WARNING ***
 *
 *	This routine will often be used when 'source' contains raw network
 *	data.  A programming error in this routine could result in a denial
 *	of service, or in the hijacking of the server.
 *
 * Requires:
 *
 *	'name' is a valid name.
 *
 *	'source' is a valid buffer and the first byte of the active
 *	region should be the first byte of a DNS wire format domain name.
 *
 *	'target' is a valid buffer or 'target' is NULL and 'name' has
 *	a dedicated buffer.
 *
 *	'dctx' is a valid decompression context.
 *
 * Ensures:
 *
 *	If result is success:
 *	 	If 'target' is not NULL, 'name' is attached to it.
 *
 *		Uppercase letters are downcased in the copy iff
 *		DNS_NAME_DOWNCASE is set in options.
 *
 *		The current location in source is advanced, and the used space
 *		in target is updated.
 *
 * Result:
 *	Success
 *	Bad Form: Label Length
 *	Bad Form: Unknown Label Type
 *	Bad Form: Name Length
 *	Bad Form: Compression type not allowed
 *	Bad Form: Bad compression pointer
 *	Bad Form: Input too short
 *	Resource Limit: Too many compression pointers
 *	Resource Limit: Not enough space in buffer
 */

isc_result_t
dns_name_towire(dns_name_t *name, dns_compress_t *cctx, isc_buffer_t *target);
/*
 * Convert 'name' into wire format, compressing it as specified by the
 * compression context 'cctx', and storing the result in 'target'.
 *
 * Notes:
 *	If the compression context allows global compression, then the
 *	global compression table may be updated.
 *
 * Requires:
 *	'name' is a valid name
 *
 *	dns_name_countlabels(name) > 0
 *
 *	dns_name_isabsolute(name) == TRUE
 *
 *	target is a valid buffer.
 *
 *	Any offsets specified in a global compression table are valid
 *	for buffer.
 *
 * Ensures:
 *
 *	If the result is success:
 *
 *		The used space in target is updated.
 *
 * Returns:
 *	Success
 *	Resource Limit: Not enough space in buffer
 */

isc_result_t
dns_name_fromtext(dns_name_t *name, isc_buffer_t *source,
		  dns_name_t *origin, unsigned int options,
		  isc_buffer_t *target);
/*
 * Convert the textual representation of a DNS name at source
 * into uncompressed wire form stored in target.
 *
 * Notes:
 *	Relative domain names will have 'origin' appended to them
 *	unless 'origin' is NULL, in which case relative domain names
 *	will remain relative.
 *
 *	If DNS_NAME_DOWNCASE is set in 'options', any uppercase letters
 *	in 'source' will be downcased when they are copied into 'target'.
 *
 * Requires:
 *
 *	'name' is a valid name.
 *
 *	'source' is a valid buffer.
 *
 *	'target' is a valid buffer or 'target' is NULL and 'name' has
 *	a dedicated buffer.
 *
 * Ensures:
 *
 *	If result is success:
 *	 	If 'target' is not NULL, 'name' is attached to it.
 *
 *		Uppercase letters are downcased in the copy iff
 *		DNS_NAME_DOWNCASE is set in 'options'.
 *
 *		The current location in source is advanced, and the used space
 *		in target is updated.
 *
 * Result:
 *	ISC_R_SUCCESS
 *	DNS_R_EMPTYLABEL
 *	DNS_R_LABELTOOLONG
 *	DNS_R_BADESCAPE
 *	(DNS_R_BADBITSTRING: should not be returned)
 *	(DNS_R_BITSTRINGTOOLONG: should not be returned)
 *	DNS_R_BADDOTTEDQUAD
 *	ISC_R_NOSPACE
 *	ISC_R_UNEXPECTEDEND
 */

isc_result_t
dns_name_totext(dns_name_t *name, isc_boolean_t omit_final_dot,
		isc_buffer_t *target);
/*
 * Convert 'name' into text format, storing the result in 'target'.
 *
 * Notes:
 *	If 'omit_final_dot' is true, then the final '.' in absolute
 *	names other than the root name will be omitted.
 *
 *	If dns_name_countlabels == 0, the name will be "@", representing the
 *	current origin as described by RFC 1035.
 *
 *	The name is not NUL terminated.
 *
 * Requires:
 *
 *	'name' is a valid name
 *
 *	'target' is a valid buffer.
 *
 *	if dns_name_isabsolute == FALSE, then omit_final_dot == FALSE
 *
 * Ensures:
 *
 *	If the result is success:
 *
 *		The used space in target is updated.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOSPACE
 */

#define DNS_NAME_MAXTEXT 1023
/*
 * The maximum length of the text representation of a domain
 * name as generated by dns_name_totext().  This does not
 * include space for a terminating NULL.
 *
 * This definition is conservative - the actual maximum 
 * is 1004, derived as follows:
 *
 *   A backslash-decimal escaped character takes 4 bytes.
 *   A wire-encoded name can be up to 255 bytes and each
 *   label is one length byte + at most 63 bytes of data.
 *   Maximizing the label lengths gives us a name of
 *   three 63-octet labels, one 61-octet label, and the
 *   root label:
 *
 *      1 + 63 + 1 + 63 + 1 + 63 + 1 + 61 + 1 = 255
 *
 *   When printed, this is (3 * 63 + 61) * 4
 *   bytes for the escaped label data + 4 bytes for the
 *   dot terminating each label = 1004 bytes total.
 */

isc_result_t
dns_name_tofilenametext(dns_name_t *name, isc_boolean_t omit_final_dot,
			isc_buffer_t *target);
/*
 * Convert 'name' into an alternate text format appropriate for filenames,
 * storing the result in 'target'.  The name data is downcased, guaranteeing
 * that the filename does not depend on the case of the converted name.
 *
 * Notes:
 *	If 'omit_final_dot' is true, then the final '.' in absolute
 *	names other than the root name will be omitted.
 *
 *	The name is not NUL terminated.
 *
 * Requires:
 *
 *	'name' is a valid absolute name
 *
 *	'target' is a valid buffer.
 *
 * Ensures:
 *
 *	If the result is success:
 *
 *		The used space in target is updated.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOSPACE
 */

isc_result_t
dns_name_downcase(dns_name_t *source, dns_name_t *name,
		  isc_buffer_t *target);
/*
 * Downcase 'source'.
 *
 * Requires:
 *
 *	'source' and 'name' are valid names.
 *
 *	If source == name, then
 *
 *		'source' must not be read-only
 *
 *	Otherwise,
 *
 *		'target' is a valid buffer or 'target' is NULL and
 *		'name' has a dedicated buffer.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOSPACE
 *
 * Note: if source == name, then the result will always be ISC_R_SUCCESS.
 */

isc_result_t
dns_name_concatenate(dns_name_t *prefix, dns_name_t *suffix,
		     dns_name_t *name, isc_buffer_t *target);
/*
 *	Concatenate 'prefix' and 'suffix'.
 *
 * Requires:
 *
 *	'prefix' is a valid name or NULL.
 *
 *	'suffix' is a valid name or NULL.
 *
 *	'name' is a valid name or NULL.
 *
 *	'target' is a valid buffer or 'target' is NULL and 'name' has
 *	a dedicated buffer.
 *
 *	If 'prefix' is absolute, 'suffix' must be NULL or the empty name.
 *
 * Ensures:
 *
 *	On success,
 *	 	If 'target' is not NULL and 'name' is not NULL, then 'name'
 *		is attached to it.
 *
 *		The used space in target is updated.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOSPACE
 *	DNS_R_NAMETOOLONG
 */

void
dns_name_split(dns_name_t *name, unsigned int suffixlabels,
	       dns_name_t *prefix, dns_name_t *suffix);
/*
 *
 * Split 'name' into two pieces on a label boundary.
 *
 * Notes:
 *      'name' is split such that 'suffix' holds the most significant
 *      'suffixlabels' labels.  All other labels are stored in 'prefix'. 
 *
 *	Copying name data is avoided as much as possible, so 'prefix'
 *	and 'suffix' will end up pointing at the data for 'name'.
 *
 *	It is legitimate to pass a 'prefix' or 'suffix' that has
 *	its name data stored someplace other than the dedicated buffer.
 *	This is useful to avoid name copying in the calling function.
 *
 *	It is also legitimate to pass a 'prefix' or 'suffix' that is
 *	the same dns_name_t as 'name'.
 *
 * Requires:
 *	'name' is a valid name.
 *
 * 	'suffixlabels' cannot exceed the number of labels in 'name'.
 *
 *	'prefix' is a valid name or NULL, and cannot be read-only.
 *
 *	'suffix' is a valid name or NULL, and cannot be read-only.
 *
 *	If non-NULL, 'prefix' and 'suffix' must have dedicated buffers.
 *
 *	'prefix' and 'suffix' cannot point to the same buffer.
 *
 * Ensures:
 *
 *	On success:
 *		If 'prefix' is not NULL it will contain the least significant
 *		labels.
 *
 *		If 'suffix' is not NULL it will contain the most significant
 *		labels.  dns_name_countlabels(suffix) will be equal to
 *		suffixlabels.
 *
 *	On failure:
 *		Either 'prefix' or 'suffix' is invalidated (depending
 *		on which one the problem was encountered with).
 *
 * Returns:
 *	ISC_R_SUCCESS	No worries.  (This function should always success).
 */

isc_result_t
dns_name_dup(dns_name_t *source, isc_mem_t *mctx, dns_name_t *target);
/*
 * Make 'target' a dynamically allocated copy of 'source'.
 *
 * Requires:
 *
 *	'source' is a valid non-empty name.
 *
 *	'target' is a valid name that is not read-only.
 *
 *	'mctx' is a valid memory context.
 */

isc_result_t
dns_name_dupwithoffsets(dns_name_t *source, isc_mem_t *mctx,
			dns_name_t *target);
/*
 * Make 'target' a read-only dynamically allocated copy of 'source'.
 * 'target' will also have a dynamically allocated offsets table.
 *
 * Requires:
 *
 *	'source' is a valid non-empty name.
 *
 *	'target' is a valid name that is not read-only.
 *
 *	'target' has no offsets table.
 *
 *	'mctx' is a valid memory context.
 */

void
dns_name_free(dns_name_t *name, isc_mem_t *mctx);
/*
 * Free 'name'.
 *
 * Requires:
 *
 *	'name' is a valid name created previously in 'mctx' by dns_name_dup().
 *
 *	'mctx' is a valid memory context.
 *
 * Ensures:
 *
 *	All dynamic resources used by 'name' are freed and the name is
 *	invalidated.
 */

isc_result_t
dns_name_digest(dns_name_t *name, dns_digestfunc_t digest, void *arg);
/*
 * Send 'name' in DNSSEC canonical form to 'digest'.
 *
 * Requires:
 *
 *	'name' is a valid name.
 *
 *	'digest' is a valid dns_digestfunc_t.
 *
 * Ensures:
 *
 *	If successful, the DNSSEC canonical form of 'name' will have been
 *	sent to 'digest'.
 *
 *	If digest() returns something other than ISC_R_SUCCESS, that result
 *	will be returned as the result of dns_name_digest().
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *
 *	Many other results are possible if not successful.
 *
 */

isc_boolean_t
dns_name_dynamic(dns_name_t *name);
/*
 * Returns whether there is dynamic memory associated with this name.
 *
 * Requires:
 *
 *	'name' is a valid name.
 *
 * Returns:
 *
 *	'ISC_TRUE' if the name is dynamic othewise 'ISC_FALSE'.
 */

isc_result_t
dns_name_print(dns_name_t *name, FILE *stream);
/*
 * Print 'name' on 'stream'.
 *
 * Requires:
 *
 *	'name' is a valid name.
 *
 *	'stream' is a valid stream.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *
 *	Any error that dns_name_totext() can return.
 */

void
dns_name_format(dns_name_t *name, char *cp, unsigned int size);
/*
 * Format 'name' as text appropriate for use in log messages.
 *
 * Store the formatted name at 'cp', writing no more than
 * 'size' bytes.  The resulting string is guaranteed to be
 * null terminated.
 *
 * The formatted name will have a terminating dot only if it is
 * the root.
 *
 * This function cannot fail, instead any errors are indicated
 * in the returned text.
 *
 * Requires:
 *
 *	'name' is a valid name.
 *
 *	'cp' points a valid character array of size 'size'.
 *
 *	'size' > 0.
 *
 */

#define DNS_NAME_FORMATSIZE (DNS_NAME_MAXTEXT + 1)
/*
 * Suggested size of buffer passed to dns_name_format().
 * Includes space for the terminating NULL.
 */

isc_result_t
dns_name_copy(dns_name_t *source, dns_name_t *dest, isc_buffer_t *target);
/*
 * Makes 'dest' refer to a copy of the name in 'source'.  The data are
 * either copied to 'target' or the dedicated buffer in 'dest'.
 *
 * Requires:
 * 	'source' is a valid name.
 *
 * 	'dest' is an initialized name with a dedicated buffer.
 *
 * 	'target' is NULL or an initialized buffer.
 *
 * 	Either dest has a dedicated buffer or target != NULL.
 *
 * Ensures:
 *
 *	On success, the used space in target is updated.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOSPACE
 */

isc_boolean_t
dns_name_ishostname(const dns_name_t *name, isc_boolean_t wildcard);
/*
 * Return if 'name' is a valid hostname.  RFC 952 / RFC 1123.
 * If 'wildcard' is ISC_TRUE then allow the first label of name to
 * be a wildcard.
 * The root is also accepted.
 *
 * Requires:
 *	'name' to be valid.
 */
 

isc_boolean_t
dns_name_ismailbox(const dns_name_t *name);
/*
 * Return if 'name' is a valid mailbox.  RFC 821.
 *
 * Requires:
 *	'name' to be valid.
 */

ISC_LANG_ENDDECLS

/***
 *** High Peformance Macros
 ***/

/*
 * WARNING:  Use of these macros by applications may require recompilation
 *           of the application in some situations where calling the function
 *           would not.
 *
 * WARNING:  No assertion checking is done for these macros.
 */

#define DNS_NAME_INIT(n, o) \
do { \
	(n)->magic = DNS_NAME_MAGIC; \
	(n)->ndata = NULL; \
	(n)->length = 0; \
	(n)->labels = 0; \
	(n)->attributes = 0; \
	(n)->offsets = (o); \
	(n)->buffer = NULL; \
	ISC_LINK_INIT((n), link); \
	ISC_LIST_INIT((n)->list); \
} while (0)

#define DNS_NAME_RESET(n) \
do { \
	(n)->ndata = NULL; \
	(n)->length = 0; \
	(n)->labels = 0; \
	(n)->attributes &= ~DNS_NAMEATTR_ABSOLUTE; \
	if ((n)->buffer != NULL) \
		isc_buffer_clear((n)->buffer); \
} while (0)

#define DNS_NAME_SETBUFFER(n, b) \
	(n)->buffer = (b)

#define DNS_NAME_ISABSOLUTE(n) \
	(((n)->attributes & DNS_NAMEATTR_ABSOLUTE) != 0 ? ISC_TRUE : ISC_FALSE)

#define DNS_NAME_COUNTLABELS(n) \
	((n)->labels)

#define DNS_NAME_TOREGION(n, r) \
do { \
	(r)->base = (n)->ndata; \
	(r)->length = (n)->length; \
} while (0)

#define DNS_NAME_SPLIT(n, l, p, s) \
do { \
	dns_name_t *_n = (n); \
	dns_name_t *_p = (p); \
	dns_name_t *_s = (s); \
	unsigned int _l = (l); \
	if (_p != NULL) \
		dns_name_getlabelsequence(_n, 0, _n->labels - _l, _p); \
	if (_s != NULL) \
		dns_name_getlabelsequence(_n, _n->labels - _l, _l, _s); \
} while (0)

#ifdef DNS_NAME_USEINLINE

#define dns_name_init(n, o)		DNS_NAME_INIT(n, o)
#define dns_name_reset(n)		DNS_NAME_RESET(n)
#define dns_name_setbuffer(n, b)	DNS_NAME_SETBUFFER(n, b)
#define dns_name_countlabels(n)		DNS_NAME_COUNTLABELS(n)
#define dns_name_isabsolute(n)		DNS_NAME_ISABSOLUTE(n)
#define dns_name_toregion(n, r)		DNS_NAME_TOREGION(n, r)
#define dns_name_split(n, l, p, s)	DNS_NAME_SPLIT(n, l, p, s)

#endif /* DNS_NAME_USEINLINE */

#endif /* DNS_NAME_H */
