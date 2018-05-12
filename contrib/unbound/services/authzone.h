/*
 * services/authzone.h - authoritative zone that is locally hosted.
 *
 * Copyright (c) 2017, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *
 * This file contains the functions for an authority zone.  This zone
 * is queried by the iterator, just like a stub or forward zone, but then
 * the data is locally held.
 */

#ifndef SERVICES_AUTHZONE_H
#define SERVICES_AUTHZONE_H
#include "util/rbtree.h"
#include "util/locks.h"
struct ub_packed_rrset_key;
struct regional;
struct config_file;
struct query_info;
struct dns_msg;

/**
 * Authoritative zones, shared.
 */
struct auth_zones {
	/** lock on the authzone tree */
	lock_rw_type lock;
	/** rbtree of struct auth_zone */
	rbtree_type ztree;
};

/**
 * Auth zone.  Authoritative data, that is fetched from instead of sending
 * packets to the internet.
 */
struct auth_zone {
	/** rbtree node, key is name and class */
	rbnode_type node;

	/** zone name, in uncompressed wireformat */
	uint8_t* name;
	/** length of zone name */
	size_t namelen;
	/** number of labels in zone name */
	int namelabs;
	/** the class of this zone, in host byteorder.
	 * uses 'dclass' to not conflict with c++ keyword class. */
	uint16_t dclass;

	/** lock on the data in the structure
	 * For the node, parent, name, namelen, namelabs, dclass, you
	 * need to also hold the zones_tree lock to change them (or to
	 * delete this zone) */
	lock_rw_type lock;

	/** auth data for this zone
	 * rbtree of struct auth_data */
	rbtree_type data;

	/* zonefile name (or NULL for no zonefile) */
	char* zonefile;
	/* fallback to the internet on failure or ttl-expiry of auth zone */
	int fallback_enabled;
};

/**
 * Auth data. One domain name, and the RRs to go with it.
 */
struct auth_data {
	/** rbtree node, key is name only */
	rbnode_type node;
	/** domain name */
	uint8_t* name;
	/** length of name */
	size_t namelen;
	/** number of labels in name */
	int namelabs;
	/** the data rrsets, with different types, linked list.
	 * if the list if NULL the node would be an empty non-terminal,
	 * but in this data structure such nodes that represent an empty
	 * non-terminal are not needed; they just don't exist.
	 */
	struct auth_rrset* rrsets;
};

/**
 * A auth data RRset
 */
struct auth_rrset {
	/** next in list */
	struct auth_rrset* next;
	/** RR type in host byteorder */
	uint16_t type;
	/** RRset data item */
	struct packed_rrset_data* data;
};

/**
 * Create auth zones structure
 */
struct auth_zones* auth_zones_create(void);

/**
 * Apply configuration to auth zones.  Reads zonefiles.
 */
int auth_zones_apply_config(struct auth_zones* az, struct config_file* cfg);

/**
 * Delete auth zones structure
 */
void auth_zones_delete(struct auth_zones* az);

/**
 * Write auth zone data to file, in zonefile format.
 */
int auth_zone_write_file(struct auth_zone* z, const char* fname);

/**
 * Use auth zones to lookup the answer to a query.
 * The query is from the iterator.  And the auth zones attempts to provide
 * the answer instead of going to the internet.
 *
 * @param az: auth zones structure.
 * @param qinfo: query info to lookup.
 * @param region: region to use to allocate the reply in.
 * @param msg: reply is stored here (if one).
 * @param fallback: if true, fallback to making a query to the internet.
 * @param dp_nm: name of delegation point to look for.  This zone is used
 *	to answer the query.
 *	If the dp_nm is not found, fallback is set to true and false returned.
 * @param dp_nmlen: length of dp_nm.
 * @return 0: failure (an error of some sort, like servfail).
 *         if 0 and fallback is true, fallback to the internet.
 *         if 0 and fallback is false, like getting servfail.
 *         If true, an answer is available.
 */
int auth_zones_lookup(struct auth_zones* az, struct query_info* qinfo,
	struct regional* region, struct dns_msg** msg, int* fallback,
	uint8_t* dp_nm, size_t dp_nmlen);

/** 
 * Find the auth zone that is above the given qname.
 * Return NULL when there is no auth_zone above the give name, otherwise
 * returns the closest auth_zone above the qname that pertains to it.
 * @param az: auth zones structure.
 * @param qinfo: query info to lookup.
 * @return NULL or auth_zone that pertains to the query.
 */
struct auth_zone* auth_zones_find_zone(struct auth_zones* az,
	struct query_info* qinfo);

/** find an auth zone by name (exact match by name or NULL returned) */
struct auth_zone* auth_zone_find(struct auth_zones* az, uint8_t* nm,
	size_t nmlen, uint16_t dclass);

/** create an auth zone. returns wrlocked zone. caller must have wrlock
 * on az. returns NULL on malloc failure */
struct auth_zone* auth_zone_create(struct auth_zones* az, uint8_t* nm,
	size_t nmlen, uint16_t dclass);

/** set auth zone zonefile string. caller must have lock on zone */
int auth_zone_set_zonefile(struct auth_zone* z, char* zonefile);

/** set auth zone fallback. caller must have lock on zone.
 * fallbackstr is "yes" or "no". false on parse failure. */
int auth_zone_set_fallback(struct auth_zone* z, char* fallbackstr);

/** read auth zone from zonefile. caller must lock zone. false on failure */
int auth_zone_read_zonefile(struct auth_zone* z);

/** compare auth_zones for sorted rbtree */
int auth_zone_cmp(const void* z1, const void* z2);

/** compare auth_data for sorted rbtree */
int auth_data_cmp(const void* z1, const void* z2);

#endif /* SERVICES_AUTHZONE_H */
