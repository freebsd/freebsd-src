/* dns.c

   Domain Name Service subroutines. */

/*
 * Copyright (c) 2001-2002 Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon in cooperation with Nominum, Inc.
 * To learn more about the Internet Software Consortium, see
 * ``http://www.isc.org/''.  To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: dns.c,v 1.35.2.13 2002/11/17 02:26:57 dhankins Exp $ Copyright (c) 2001-2002 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"
#include "arpa/nameser.h"
#include "dst/md5.h"

/* This file is kind of a crutch for the BIND 8 nsupdate code, which has
 * itself been cruelly hacked from its original state.   What this code
 * does is twofold: first, it maintains a database of zone cuts that can
 * be used to figure out which server should be contacted to update any
 * given domain name.   Secondly, it maintains a set of named TSIG keys,
 * and associates those keys with zones.   When an update is requested for
 * a particular zone, the key associated with that zone is used for the
 * update.
 *
 * The way this works is that you define the domain name to which an
 * SOA corresponds, and the addresses of some primaries for that domain name:
 *
 *	zone FOO.COM {
 *	  primary 10.0.17.1;
 *	  secondary 10.0.22.1, 10.0.23.1;
 *	  key "FOO.COM Key";
 * 	}
 *
 * If an update is requested for GAZANGA.TOPANGA.FOO.COM, then the name
 * server looks in its database for a zone record for "GAZANGA.TOPANGA.FOO.COM",
 * doesn't find it, looks for one for "TOPANGA.FOO.COM", doesn't find *that*,
 * looks for "FOO.COM", finds it. So it
 * attempts the update to the primary for FOO.COM.   If that times out, it
 * tries the secondaries.   You can list multiple primaries if you have some
 * kind of magic name server that supports that.   You shouldn't list
 * secondaries that don't know how to forward updates (e.g., BIND 8 doesn't
 * support update forwarding, AFAIK).   If no TSIG key is listed, the update
 * is attempted without TSIG.
 *
 * The DHCP server tries to find an existing zone for any given name by
 * trying to look up a local zone structure for each domain containing
 * that name, all the way up to '.'.   If it finds one cached, it tries
 * to use that one to do the update.   That's why it tries to update
 * "FOO.COM" above, even though theoretically it should try GAZANGA...
 * and TOPANGA... first.
 *
 * If the update fails with a predefined or cached zone (we'll get to
 * those in a second), then it tries to find a more specific zone.   This
 * is done by looking first for an SOA for GAZANGA.TOPANGA.FOO.COM.   Then
 * an SOA for TOPANGA.FOO.COM is sought.   If during this search a predefined
 * or cached zone is found, the update fails - there's something wrong
 * somewhere.
 *
 * If a more specific zone _is_ found, that zone is cached for the length of
 * its TTL in the same database as that described above.   TSIG updates are
 * never done for cached zones - if you want TSIG updates you _must_
 * write a zone definition linking the key to the zone.   In cases where you
 * know for sure what the key is but do not want to hardcode the IP addresses
 * of the primary or secondaries, a zone declaration can be made that doesn't
 * include any primary or secondary declarations.   When the DHCP server
 * encounters this while hunting up a matching zone for a name, it looks up
 * the SOA, fills in the IP addresses, and uses that record for the update.
 * If the SOA lookup returns NXRRSET, a warning is printed and the zone is
 * discarded, TSIG key and all.   The search for the zone then continues as if
 * the zone record hadn't been found.   Zones without IP addresses don't
 * match when initially hunting for a predefined or cached zone to update.
 *
 * When an update is attempted and no predefined or cached zone is found
 * that matches any enclosing domain of the domain being updated, the DHCP
 * server goes through the same process that is done when the update to a
 * predefined or cached zone fails - starting with the most specific domain
 * name (GAZANGA.TOPANGA.FOO.COM) and moving to the least specific (the root),
 * it tries to look up an SOA record.   When it finds one, it creates a cached
 * zone and attempts an update, and gives up if the update fails.
 *
 * TSIG keys are defined like this:
 *
 *	key "FOO.COM Key" {
 *		algorithm HMAC-MD5.SIG-ALG.REG.INT;
 *		secret <Base64>;
 *	}
 *
 * <Base64> is a number expressed in base64 that represents the key.
 * It's also permissible to use a quoted string here - this will be
 * translated as the ASCII bytes making up the string, and will not
 * include any NUL termination.  The key name can be any text string,
 * and the key type must be one of the key types defined in the draft
 * or by the IANA.  Currently only the HMAC-MD5... key type is
 * supported.
 */

dns_zone_hash_t *dns_zone_hash;

#if defined (NSUPDATE)
isc_result_t find_tsig_key (ns_tsig_key **key, const char *zname,
			    struct dns_zone *zone)
{
	isc_result_t status;
	ns_tsig_key *tkey;

	if (!zone)
		return ISC_R_NOTFOUND;

	if (!zone -> key) {
		return ISC_R_KEY_UNKNOWN;
	}
	
	if ((!zone -> key -> name ||
	     strlen (zone -> key -> name) > NS_MAXDNAME) ||
	    (!zone -> key -> algorithm ||
	     strlen (zone -> key -> algorithm) > NS_MAXDNAME) ||
	    (!zone -> key) ||
	    (!zone -> key -> key) ||
	    (zone -> key -> key -> len == 0)) {
		return ISC_R_INVALIDKEY;
	}
	tkey = dmalloc (sizeof *tkey, MDL);
	if (!tkey) {
	      nomem:
		return ISC_R_NOMEMORY;
	}
	memset (tkey, 0, sizeof *tkey);
	tkey -> data = dmalloc (zone -> key -> key -> len, MDL);
	if (!tkey -> data) {
		dfree (tkey, MDL);
		goto nomem;
	}
	strcpy (tkey -> name, zone -> key -> name);
	strcpy (tkey -> alg, zone -> key -> algorithm);
	memcpy (tkey -> data,
		zone -> key -> key -> value, zone -> key -> key -> len);
	tkey -> len = zone -> key -> key -> len;
	*key = tkey;
	return ISC_R_SUCCESS;
}

void tkey_free (ns_tsig_key **key)
{
	if ((*key) -> data)
		dfree ((*key) -> data, MDL);
	dfree ((*key), MDL);
	*key = (ns_tsig_key *)0;
}
#endif

isc_result_t enter_dns_zone (struct dns_zone *zone)
{
	struct dns_zone *tz = (struct dns_zone *)0;

	if (dns_zone_hash) {
		dns_zone_hash_lookup (&tz,
				      dns_zone_hash, zone -> name, 0, MDL);
		if (tz == zone) {
			dns_zone_dereference (&tz, MDL);
			return ISC_R_SUCCESS;
		}
		if (tz) {
			dns_zone_hash_delete (dns_zone_hash,
					      zone -> name, 0, MDL);
			dns_zone_dereference (&tz, MDL);
		}
	} else {
		if (!dns_zone_new_hash (&dns_zone_hash, 1, MDL))
			return ISC_R_NOMEMORY;
	}
	dns_zone_hash_add (dns_zone_hash, zone -> name, 0, zone, MDL);
	return ISC_R_SUCCESS;
}

isc_result_t dns_zone_lookup (struct dns_zone **zone, const char *name)
{
	struct dns_zone *tz = (struct dns_zone *)0;
	int len;
	char *tname = (char *)0;
	isc_result_t status;

	if (!dns_zone_hash)
		return ISC_R_NOTFOUND;

	len = strlen (name);
	if (name [len - 1] != '.') {
		tname = dmalloc ((unsigned)len + 2, MDL);
		if (!tname)
			return ISC_R_NOMEMORY;;
		strcpy (tname, name);
		tname [len] = '.';
		tname [len + 1] = 0;
		name = tname;
	}
	if (!dns_zone_hash_lookup (zone, dns_zone_hash, name, 0, MDL))
		status = ISC_R_NOTFOUND;
	else
		status = ISC_R_SUCCESS;

	if (tname)
		dfree (tname, MDL);
	return status;
}

int dns_zone_dereference (ptr, file, line)
	struct dns_zone **ptr;
	const char *file;
	int line;
{
	int i;
	struct dns_zone *dns_zone;

	if (!ptr || !*ptr) {
		log_error ("%s(%d): null pointer", file, line);
#if defined (POINTER_DEBUG)
		abort ();
#else
		return 0;
#endif
	}

	dns_zone = *ptr;
	*ptr = (struct dns_zone *)0;
	--dns_zone -> refcnt;
	rc_register (file, line, ptr, dns_zone, dns_zone -> refcnt, 1, RC_MISC);
	if (dns_zone -> refcnt > 0)
		return 1;

	if (dns_zone -> refcnt < 0) {
		log_error ("%s(%d): negative refcnt!", file, line);
#if defined (DEBUG_RC_HISTORY)
		dump_rc_history (dns_zone);
#endif
#if defined (POINTER_DEBUG)
		abort ();
#else
		return 0;
#endif
	}

	if (dns_zone -> name)
		dfree (dns_zone -> name, file, line);
	if (dns_zone -> key)
		omapi_auth_key_dereference (&dns_zone -> key, file, line);
	if (dns_zone -> primary)
		option_cache_dereference (&dns_zone -> primary, file, line);
	if (dns_zone -> secondary)
		option_cache_dereference (&dns_zone -> secondary, file, line);
	dfree (dns_zone, file, line);
	return 1;
}

#if defined (NSUPDATE)
isc_result_t find_cached_zone (const char *dname, ns_class class,
			       char *zname, size_t zsize,
			       struct in_addr *addrs,
			       int naddrs, int *naddrout,
			       struct dns_zone **zcookie)
{
	isc_result_t status = ISC_R_NOTFOUND;
	const char *np;
	struct dns_zone *zone = (struct dns_zone *)0;
	struct data_string nsaddrs;
	int ix;

	/* The absence of the zcookie pointer indicates that we
	   succeeded previously, but the update itself failed, meaning
	   that we shouldn't use the cached zone. */
	if (!zcookie)
		return ISC_R_NOTFOUND;

	/* We can't look up a null zone. */
	if (!dname || !*dname)
		return ISC_R_INVALIDARG;

	/* For each subzone, try to find a cached zone. */
	for (np = dname; np; np = strchr (np, '.')) {
		np++;
		status = dns_zone_lookup (&zone, np);
		if (status == ISC_R_SUCCESS)
			break;
	}

	if (status != ISC_R_SUCCESS)
		return status;

	/* Make sure the zone is valid. */
	if (zone -> timeout && zone -> timeout < cur_time) {
		dns_zone_dereference (&zone, MDL);
		return ISC_R_CANCELED;
	}

	/* Make sure the zone name will fit. */
	if (strlen (zone -> name) > zsize) {
		dns_zone_dereference (&zone, MDL);
		return ISC_R_NOSPACE;
	}
	strcpy (zname, zone -> name);

	memset (&nsaddrs, 0, sizeof nsaddrs);
	ix = 0;

	if (zone -> primary) {
		if (evaluate_option_cache (&nsaddrs, (struct packet *)0,
					   (struct lease *)0,
					   (struct client_state *)0,
					   (struct option_state *)0,
					   (struct option_state *)0,
					   &global_scope,
					   zone -> primary, MDL)) {
			int ip = 0;
			while (ix < naddrs) {
				if (ip + 4 > nsaddrs.len)
					break;
				memcpy (&addrs [ix], &nsaddrs.data [ip], 4);
				ip += 4;
				ix++;
			}
			data_string_forget (&nsaddrs, MDL);
		}
	}
	if (zone -> secondary) {
		if (evaluate_option_cache (&nsaddrs, (struct packet *)0,
					   (struct lease *)0,
					   (struct client_state *)0,
					   (struct option_state *)0,
					   (struct option_state *)0,
					   &global_scope,
					   zone -> secondary, MDL)) {
			int ip = 0;
			while (ix < naddrs) {
				if (ip + 4 > nsaddrs.len)
					break;
				memcpy (&addrs [ix], &nsaddrs.data [ip], 4);
				ip += 4;
				ix++;
			}
			data_string_forget (&nsaddrs, MDL);
		}
	}

	/* It's not an error for zcookie to have a value here - actually,
	   it's quite likely, because res_nupdate cycles through all the
	   names in the update looking for their zones. */
	if (!*zcookie)
		dns_zone_reference (zcookie, zone, MDL);
	dns_zone_dereference (&zone, MDL);
	if (naddrout)
		*naddrout = ix;
	return ISC_R_SUCCESS;
}

void forget_zone (struct dns_zone **zone)
{
	dns_zone_dereference (zone, MDL);
}

void repudiate_zone (struct dns_zone **zone)
{
	/* XXX Currently we're not differentiating between a cached
	   XXX zone and a zone that's been repudiated, which means
	   XXX that if we reap cached zones, we blow away repudiated
	   XXX zones.   This isn't a big problem since we're not yet
	   XXX caching zones... :'} */

	(*zone) -> timeout = cur_time - 1;
	dns_zone_dereference (zone, MDL);
}

void cache_found_zone (ns_class class,
		       char *zname, struct in_addr *addrs, int naddrs)
{
	isc_result_t status = ISC_R_NOTFOUND;
	struct dns_zone *zone = (struct dns_zone *)0;
	struct data_string nsaddrs;
	int ix = strlen (zname);

	if (zname [ix - 1] == '.')
		ix = 0;

	/* See if there's already such a zone. */
	if (dns_zone_lookup (&zone, zname) == ISC_R_SUCCESS) {
		/* If it's not a dynamic zone, leave it alone. */
		if (!zone -> timeout)
			return;
		/* Address may have changed, so just blow it away. */
		if (zone -> primary)
			option_cache_dereference (&zone -> primary, MDL);
		if (zone -> secondary)
			option_cache_dereference (&zone -> secondary, MDL);
	} else if (!dns_zone_allocate (&zone, MDL))
		return;

	if (!zone -> name) {
		zone -> name =
			dmalloc (strlen (zname) + 1 + (ix != 0), MDL);
		if (!zone -> name) {
			dns_zone_dereference (&zone, MDL);
			return;
		}
		strcpy (zone -> name, zname);
		/* Add a trailing '.' if it was missing. */
		if (ix) {
			zone -> name [ix] = '.';
			zone -> name [ix + 1] = 0;
		}
	}

	/* XXX Need to get the lower-level code to push the actual zone
	   XXX TTL up to us. */
	zone -> timeout = cur_time + 1800;
	
	if (!option_cache_allocate (&zone -> primary, MDL)) {
		dns_zone_dereference (&zone, MDL);
		return;
	}
	if (!buffer_allocate (&zone -> primary -> data.buffer,
			      naddrs * sizeof (struct in_addr), MDL)) {
		dns_zone_dereference (&zone, MDL);
		return;
	}
	memcpy (zone -> primary -> data.buffer -> data,
		addrs, naddrs * sizeof *addrs);
	zone -> primary -> data.data =
		&zone -> primary -> data.buffer -> data [0];
	zone -> primary -> data.len = naddrs * sizeof *addrs;

	enter_dns_zone (zone);
}

/* Have to use TXT records for now. */
#define T_DHCID T_TXT

int get_dhcid (struct data_string *id,
	       int type, const u_int8_t *data, unsigned len)
{
	unsigned char buf[MD5_DIGEST_LENGTH];
	MD5_CTX md5;
	int i;

	/* Types can only be 0..(2^16)-1. */
	if (type < 0 || type > 65535)
		return 0;

	/* Hexadecimal MD5 digest plus two byte type and NUL. */
	if (!buffer_allocate (&id -> buffer,
			      (MD5_DIGEST_LENGTH * 2) + 3, MDL))
		return 0;
	id -> data = id -> buffer -> data;

	/*
	 * DHCP clients and servers should use the following forms of client
	 * identification, starting with the most preferable, and finishing
	 * with the least preferable.  If the client does not send any of these
	 * forms of identification, the DHCP/DDNS interaction is not defined by
	 * this specification.  The most preferable form of identification is
	 * the Globally Unique Identifier Option [TBD].  Next is the DHCP
	 * Client Identifier option.  Last is the client's link-layer address,
	 * as conveyed in its DHCPREQUEST message.  Implementors should note
	 * that the link-layer address cannot be used if there are no
	 * significant bytes in the chaddr field of the DHCP client's request,
	 * because this does not constitute a unique identifier.
	 *   -- "Interaction between DHCP and DNS"
	 *      <draft-ietf-dhc-dhcp-dns-12.txt>
	 *      M. Stapp, Y. Rekhter
	 */

	/* Put the type in the first two bytes. */
	id -> buffer -> data [0] = "0123456789abcdef" [type >> 4];
	id -> buffer -> data [1] = "0123456789abcdef" [type % 15];

	/* Mash together an MD5 hash of the identifier. */
	MD5_Init (&md5);
	MD5_Update (&md5, data, len);
	MD5_Final (buf, &md5);

	/* Convert into ASCII. */
	for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
		id -> buffer -> data [i * 2 + 2] =
			"0123456789abcdef" [(buf [i] >> 4) & 0xf];
		id -> buffer -> data [i * 2 + 3] =
			"0123456789abcdef" [buf [i] & 0xf];
	}
	id -> len = MD5_DIGEST_LENGTH * 2 + 2;
	id -> buffer -> data [id -> len] = 0;
	id -> terminated = 1;

	return 1;
}

/* Now for the DDNS update code that is shared between client and
   server... */

isc_result_t ddns_update_a (struct data_string *ddns_fwd_name,
			    struct iaddr ddns_addr,
			    struct data_string *ddns_dhcid,
			    unsigned long ttl, int rrsetp)
{
	ns_updque updqueue;
	ns_updrec *updrec;
	isc_result_t result;
	char ddns_address [16];

	if (ddns_addr.len != 4)
		return ISC_R_INVALIDARG;
#ifndef NO_SNPRINTF
	snprintf (ddns_address, 16, "%d.%d.%d.%d",
		  ddns_addr.iabuf[0], ddns_addr.iabuf[1],
		  ddns_addr.iabuf[2], ddns_addr.iabuf[3]);
#else
	sprintf (ddns_address, "%d.%d.%d.%d",
		 ddns_addr.iabuf[0], ddns_addr.iabuf[1],
		 ddns_addr.iabuf[2], ddns_addr.iabuf[3]);
#endif

	/*
	 * When a DHCP client or server intends to update an A RR, it first
	 * prepares a DNS UPDATE query which includes as a prerequisite the
	 * assertion that the name does not exist.  The update section of the
	 * query attempts to add the new name and its IP address mapping (an A
	 * RR), and the DHCID RR with its unique client-identity.
	 *   -- "Interaction between DHCP and DNS"
	 */

	ISC_LIST_INIT (updqueue);

	/*
	 * A RR does not exist.
	 */
	updrec = minires_mkupdrec (S_PREREQ,
				   (const char *)ddns_fwd_name -> data,
				   C_IN, T_A, 0);
	if (!updrec) {
		result = ISC_R_NOMEMORY;
		goto error;
	}

	updrec -> r_data = (unsigned char *)0;
	updrec -> r_size = 0;
	updrec -> r_opcode = rrsetp ? NXRRSET : NXDOMAIN;

	ISC_LIST_APPEND (updqueue, updrec, r_link);


	/*
	 * Add A RR.
	 */
	updrec = minires_mkupdrec (S_UPDATE,
				   (const char *)ddns_fwd_name -> data,
				   C_IN, T_A, ttl);
	if (!updrec) {
		result = ISC_R_NOMEMORY;
		goto error;
	}

	updrec -> r_data = (unsigned char *)ddns_address;
	updrec -> r_size = strlen (ddns_address);
	updrec -> r_opcode = ADD;

	ISC_LIST_APPEND (updqueue, updrec, r_link);


	/*
	 * Add DHCID RR.
	 */
	updrec = minires_mkupdrec (S_UPDATE,
				   (const char *)ddns_fwd_name -> data,
				   C_IN, T_DHCID, ttl);
	if (!updrec) {
		result = ISC_R_NOMEMORY;
		goto error;
	}

	updrec -> r_data = ddns_dhcid -> data;
	updrec -> r_size = ddns_dhcid -> len;
	updrec -> r_opcode = ADD;

	ISC_LIST_APPEND (updqueue, updrec, r_link);


	/*
	 * Attempt to perform the update.
	 */
	result = minires_nupdate (&resolver_state, ISC_LIST_HEAD (updqueue));

#ifdef DEBUG_DNS_UPDATES
	print_dns_status ((int)result, &updqueue);
#endif

	/*
	 * If this update operation succeeds, the updater can conclude that it
	 * has added a new name whose only RRs are the A and DHCID RR records.
	 * The A RR update is now complete (and a client updater is finished,
	 * while a server might proceed to perform a PTR RR update).
	 *   -- "Interaction between DHCP and DNS"
	 */

	if (result == ISC_R_SUCCESS) {
		log_info ("Added new forward map from %.*s to %s",
			  (int)ddns_fwd_name -> len,
			  (const char *)ddns_fwd_name -> data, ddns_address);
		goto error;
	}


	/*
	 * If the first update operation fails with YXDOMAIN, the updater can
	 * conclude that the intended name is in use.  The updater then
	 * attempts to confirm that the DNS name is not being used by some
	 * other host. The updater prepares a second UPDATE query in which the
	 * prerequisite is that the desired name has attached to it a DHCID RR
	 * whose contents match the client identity.  The update section of
	 * this query deletes the existing A records on the name, and adds the
	 * A record that matches the DHCP binding and the DHCID RR with the
	 * client identity.
	 *   -- "Interaction between DHCP and DNS"
	 */

	if (result != (rrsetp ? ISC_R_YXRRSET : ISC_R_YXDOMAIN)) {
		log_error ("Unable to add forward map from %.*s to %s: %s",
			   (int)ddns_fwd_name -> len,
			   (const char *)ddns_fwd_name -> data, ddns_address,
			   isc_result_totext (result));
		goto error;
	}

	while (!ISC_LIST_EMPTY (updqueue)) {
		updrec = ISC_LIST_HEAD (updqueue);
		ISC_LIST_UNLINK (updqueue, updrec, r_link);
		minires_freeupdrec (updrec);
	}

	/*
	 * DHCID RR exists, and matches client identity.
	 */
	updrec = minires_mkupdrec (S_PREREQ,
				   (const char *)ddns_fwd_name -> data,
				   C_IN, T_DHCID, 0);
	if (!updrec) {
		result = ISC_R_NOMEMORY;
		goto error;
	}

	updrec -> r_data = ddns_dhcid -> data;
	updrec -> r_size = ddns_dhcid -> len;
	updrec -> r_opcode = YXRRSET;

	ISC_LIST_APPEND (updqueue, updrec, r_link);


	/*
	 * Delete A RRset.
	 */
	updrec = minires_mkupdrec (S_UPDATE,
				   (const char *)ddns_fwd_name -> data,
				   C_IN, T_A, 0);
	if (!updrec) {
		result = ISC_R_NOMEMORY;
		goto error;
	}

	updrec -> r_data = (unsigned char *)0;
	updrec -> r_size = 0;
	updrec -> r_opcode = DELETE;

	ISC_LIST_APPEND (updqueue, updrec, r_link);


	/*
	 * Add A RR.
	 */
	updrec = minires_mkupdrec (S_UPDATE,
				   (const char *)ddns_fwd_name -> data,
				   C_IN, T_A, ttl);
	if (!updrec) {
		result = ISC_R_NOMEMORY;
		goto error;
	}

	updrec -> r_data = (unsigned char *)ddns_address;
	updrec -> r_size = strlen (ddns_address);
	updrec -> r_opcode = ADD;

	ISC_LIST_APPEND (updqueue, updrec, r_link);


	/*
	 * Attempt to perform the update.
	 */
	result = minires_nupdate (&resolver_state, ISC_LIST_HEAD (updqueue));

	if (result != ISC_R_SUCCESS) {
		if (result == YXRRSET || result == YXDOMAIN ||
		    result == NXRRSET || result == NXDOMAIN)
			log_error ("Forward map from %.*s to %s already in use",
				   (int)ddns_fwd_name -> len,
				   (const char *)ddns_fwd_name -> data,
				   ddns_address);
		else
			log_error ("Can't update forward map %.*s to %s: %s",
				   (int)ddns_fwd_name -> len,
				   (const char *)ddns_fwd_name -> data,
				   ddns_address, isc_result_totext (result));

	} else {
		log_info ("Added new forward map from %.*s to %s",
			  (int)ddns_fwd_name -> len,
			  (const char *)ddns_fwd_name -> data, ddns_address);
	}
#if defined (DEBUG_DNS_UPDATES)
	print_dns_status ((int)result, &updqueue);
#endif

	/*
	 * If this query succeeds, the updater can conclude that the current
	 * client was the last client associated with the domain name, and that
	 * the name now contains the updated A RR. The A RR update is now
	 * complete (and a client updater is finished, while a server would
	 * then proceed to perform a PTR RR update).
	 *   -- "Interaction between DHCP and DNS"
	 */

	/*
	 * If the second query fails with NXRRSET, the updater must conclude
	 * that the client's desired name is in use by another host.  At this
	 * juncture, the updater can decide (based on some administrative
	 * configuration outside of the scope of this document) whether to let
	 * the existing owner of the name keep that name, and to (possibly)
	 * perform some name disambiguation operation on behalf of the current
	 * client, or to replace the RRs on the name with RRs that represent
	 * the current client. If the configured policy allows replacement of
	 * existing records, the updater submits a query that deletes the
	 * existing A RR and the existing DHCID RR, adding A and DHCID RRs that
	 * represent the IP address and client-identity of the new client.
	 *   -- "Interaction between DHCP and DNS"
	 */

  error:
	while (!ISC_LIST_EMPTY (updqueue)) {
		updrec = ISC_LIST_HEAD (updqueue);
		ISC_LIST_UNLINK (updqueue, updrec, r_link);
		minires_freeupdrec (updrec);
	}

	return result;
}

isc_result_t ddns_remove_a (struct data_string *ddns_fwd_name,
			    struct iaddr ddns_addr,
			    struct data_string *ddns_dhcid)
{
	ns_updque updqueue;
	ns_updrec *updrec;
	isc_result_t result = SERVFAIL;
	char ddns_address [16];

	if (ddns_addr.len != 4)
		return ISC_R_INVALIDARG;

#ifndef NO_SNPRINTF
	snprintf (ddns_address, 16, "%d.%d.%d.%d",
		  ddns_addr.iabuf[0], ddns_addr.iabuf[1],
		  ddns_addr.iabuf[2], ddns_addr.iabuf[3]);
#else
	sprintf (ddns_address, "%d.%d.%d.%d",
		 ddns_addr.iabuf[0], ddns_addr.iabuf[1],
		 ddns_addr.iabuf[2], ddns_addr.iabuf[3]);
#endif


	/*
	 * The entity chosen to handle the A record for this client (either the
	 * client or the server) SHOULD delete the A record that was added when
	 * the lease was made to the client.
	 *
	 * In order to perform this delete, the updater prepares an UPDATE
	 * query which contains two prerequisites.  The first prerequisite
	 * asserts that the DHCID RR exists whose data is the client identity
	 * described in Section 4.3. The second prerequisite asserts that the
	 * data in the A RR contains the IP address of the lease that has
	 * expired or been released.
	 *   -- "Interaction between DHCP and DNS"
	 */

	ISC_LIST_INIT (updqueue);

	/*
	 * DHCID RR exists, and matches client identity.
	 */
	updrec = minires_mkupdrec (S_PREREQ,
				   (const char *)ddns_fwd_name -> data,
				   C_IN, T_DHCID,0);
	if (!updrec) {
		result = ISC_R_NOMEMORY;
		goto error;
	}

	updrec -> r_data = ddns_dhcid -> data;
	updrec -> r_size = ddns_dhcid -> len;
	updrec -> r_opcode = YXRRSET;

	ISC_LIST_APPEND (updqueue, updrec, r_link);


	/*
	 * A RR matches the expiring lease.
	 */
	updrec = minires_mkupdrec (S_PREREQ,
				   (const char *)ddns_fwd_name -> data,
				   C_IN, T_A, 0);
	if (!updrec) {
		result = ISC_R_NOMEMORY;
		goto error;
	}

	updrec -> r_data = (unsigned char *)ddns_address;
	updrec -> r_size = strlen (ddns_address);
	updrec -> r_opcode = YXRRSET;

	ISC_LIST_APPEND (updqueue, updrec, r_link);


	/*
	 * Delete appropriate A RR.
	 */
	updrec = minires_mkupdrec (S_UPDATE,
				   (const char *)ddns_fwd_name -> data,
				   C_IN, T_A, 0);
	if (!updrec) {
		result = ISC_R_NOMEMORY;
		goto error;
	}

	updrec -> r_data = (unsigned char *)ddns_address;
	updrec -> r_size = strlen (ddns_address);
	updrec -> r_opcode = DELETE;

	ISC_LIST_APPEND (updqueue, updrec, r_link);

	/*
	 * Attempt to perform the update.
	 */
	result = minires_nupdate (&resolver_state, ISC_LIST_HEAD (updqueue));
	print_dns_status ((int)result, &updqueue);

	/*
	 * If the query fails, the updater MUST NOT delete the DNS name.  It
	 * may be that the host whose lease on the server has expired has moved
	 * to another network and obtained a lease from a different server,
	 * which has caused the client's A RR to be replaced. It may also be
	 * that some other client has been configured with a name that matches
	 * the name of the DHCP client, and the policy was that the last client
	 * to specify the name would get the name.  In this case, the DHCID RR
	 * will no longer match the updater's notion of the client-identity of
	 * the host pointed to by the DNS name.
	 *   -- "Interaction between DHCP and DNS"
	 */

	if (result != ISC_R_SUCCESS) {
		/* If the rrset isn't there, we didn't need to do the
		   delete, which is success. */
		if (result == ISC_R_NXRRSET || result == ISC_R_NXDOMAIN)
			result = ISC_R_SUCCESS;	
		goto error;
	}

	while (!ISC_LIST_EMPTY (updqueue)) {
		updrec = ISC_LIST_HEAD (updqueue);
		ISC_LIST_UNLINK (updqueue, updrec, r_link);
		minires_freeupdrec (updrec);
	}

	/* If the deletion of the A succeeded, and there are no A records
	   left for this domain, then we can blow away the DHCID record
	   as well.   We can't blow away the DHCID record above because
	   it's possible that more than one A has been added to this
	   domain name. */
	ISC_LIST_INIT (updqueue);

	/*
	 * A RR does not exist.
	 */
	updrec = minires_mkupdrec (S_PREREQ,
				   (const char *)ddns_fwd_name -> data,
				   C_IN, T_A, 0);
	if (!updrec) {
		result = ISC_R_NOMEMORY;
		goto error;
	}

	updrec -> r_data = (unsigned char *)0;
	updrec -> r_size = 0;
	updrec -> r_opcode = NXRRSET;

	ISC_LIST_APPEND (updqueue, updrec, r_link);

	/*
	 * Delete appropriate DHCID RR.
	 */
	updrec = minires_mkupdrec (S_UPDATE,
				  (const char *)ddns_fwd_name -> data,
				   C_IN, T_DHCID, 0);
	if (!updrec) {
		result = ISC_R_NOMEMORY;
		goto error;
	}

	updrec -> r_data = ddns_dhcid -> data;
	updrec -> r_size = ddns_dhcid -> len;
	updrec -> r_opcode = DELETE;

	ISC_LIST_APPEND (updqueue, updrec, r_link);

	/*
	 * Attempt to perform the update.
	 */
	result = minires_nupdate (&resolver_state, ISC_LIST_HEAD (updqueue));
	print_dns_status ((int)result, &updqueue);

	/* Fall through. */
  error:

	while (!ISC_LIST_EMPTY (updqueue)) {
		updrec = ISC_LIST_HEAD (updqueue);
		ISC_LIST_UNLINK (updqueue, updrec, r_link);
		minires_freeupdrec (updrec);
	}

	return result;
}


#endif /* NSUPDATE */

HASH_FUNCTIONS (dns_zone, const char *, struct dns_zone, dns_zone_hash_t,
		dns_zone_reference, dns_zone_dereference)
