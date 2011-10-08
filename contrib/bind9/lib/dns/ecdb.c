/*
 * Copyright (C) 2009-2011  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: ecdb.c,v 1.8 2011-01-14 00:51:43 tbox Exp $ */

#include "config.h"

#include <isc/result.h>
#include <isc/util.h>
#include <isc/mutex.h>
#include <isc/mem.h>

#include <dns/db.h>
#include <dns/ecdb.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/rdataslab.h>

#define ECDB_MAGIC		ISC_MAGIC('E', 'C', 'D', 'B')
#define VALID_ECDB(db)		((db) != NULL && \
				 (db)->common.impmagic == ECDB_MAGIC)

#define ECDBNODE_MAGIC		ISC_MAGIC('E', 'C', 'D', 'N')
#define VALID_ECDBNODE(ecdbn)	ISC_MAGIC_VALID(ecdbn, ECDBNODE_MAGIC)

#if DNS_RDATASET_FIXED
#error "Fixed rdataset isn't supported in this implementation"
#endif

/*%
 * The 'ephemeral' cache DB (ecdb) implementation.  An ecdb just provides
 * temporary storage for ongoing name resolution with the common DB interfaces.
 * It actually doesn't cache anything.  The implementation expects any stored
 * data is released within a short period, and does not care about the
 * scalability in terms of the number of nodes.
 */

typedef struct dns_ecdb {
	/* Unlocked */
	dns_db_t			common;
	isc_mutex_t			lock;

	/* Locked */
	unsigned int			references;
	ISC_LIST(struct dns_ecdbnode)	nodes;
} dns_ecdb_t;

typedef struct dns_ecdbnode {
	/* Unlocked */
	unsigned int			magic;
	isc_mutex_t			lock;
	dns_ecdb_t			*ecdb;
	dns_name_t			name;
	ISC_LINK(struct dns_ecdbnode)	link;

	/* Locked */
	ISC_LIST(struct rdatasetheader)	rdatasets;
	unsigned int			references;
} dns_ecdbnode_t;

typedef struct rdatasetheader {
	dns_rdatatype_t			type;
	dns_ttl_t			ttl;
	dns_trust_t			trust;
	dns_rdatatype_t			covers;
	unsigned int			attributes;

	ISC_LINK(struct rdatasetheader)	link;
} rdatasetheader_t;

/* Copied from rbtdb.c */
#define RDATASET_ATTR_NXDOMAIN		0x0010
#define NXDOMAIN(header) \
	(((header)->attributes & RDATASET_ATTR_NXDOMAIN) != 0)

static isc_result_t dns_ecdb_create(isc_mem_t *mctx, dns_name_t *origin,
				    dns_dbtype_t type,
				    dns_rdataclass_t rdclass,
				    unsigned int argc, char *argv[],
				    void *driverarg, dns_db_t **dbp);

static void rdataset_disassociate(dns_rdataset_t *rdataset);
static isc_result_t rdataset_first(dns_rdataset_t *rdataset);
static isc_result_t rdataset_next(dns_rdataset_t *rdataset);
static void rdataset_current(dns_rdataset_t *rdataset, dns_rdata_t *rdata);
static void rdataset_clone(dns_rdataset_t *source, dns_rdataset_t *target);
static unsigned int rdataset_count(dns_rdataset_t *rdataset);
static void rdataset_settrust(dns_rdataset_t *rdataset, dns_trust_t trust);

static dns_rdatasetmethods_t rdataset_methods = {
	rdataset_disassociate,
	rdataset_first,
	rdataset_next,
	rdataset_current,
	rdataset_clone,
	rdataset_count,
	NULL,			/* addnoqname */
	NULL,			/* getnoqname */
	NULL,			/* addclosest */
	NULL,			/* getclosest */
	NULL,			/* getadditional */
	NULL,			/* setadditional */
	NULL,			/* putadditional */
	rdataset_settrust,	/* settrust */
	NULL			/* expire */
};

typedef struct ecdb_rdatasetiter {
	dns_rdatasetiter_t		common;
	rdatasetheader_t	       *current;
} ecdb_rdatasetiter_t;

static void		rdatasetiter_destroy(dns_rdatasetiter_t **iteratorp);
static isc_result_t	rdatasetiter_first(dns_rdatasetiter_t *iterator);
static isc_result_t	rdatasetiter_next(dns_rdatasetiter_t *iterator);
static void		rdatasetiter_current(dns_rdatasetiter_t *iterator,
					     dns_rdataset_t *rdataset);

static dns_rdatasetitermethods_t rdatasetiter_methods = {
	rdatasetiter_destroy,
	rdatasetiter_first,
	rdatasetiter_next,
	rdatasetiter_current
};

isc_result_t
dns_ecdb_register(isc_mem_t *mctx, dns_dbimplementation_t **dbimp) {
	REQUIRE(mctx != NULL);
	REQUIRE(dbimp != NULL && *dbimp == NULL);

	return (dns_db_register("ecdb", dns_ecdb_create, NULL, mctx, dbimp));
}

void
dns_ecdb_unregister(dns_dbimplementation_t **dbimp) {
	REQUIRE(dbimp != NULL && *dbimp != NULL);

	dns_db_unregister(dbimp);
}

/*%
 * DB routines
 */

static void
attach(dns_db_t *source, dns_db_t **targetp) {
	dns_ecdb_t *ecdb = (dns_ecdb_t *)source;

	REQUIRE(VALID_ECDB(ecdb));
	REQUIRE(targetp != NULL && *targetp == NULL);

	LOCK(&ecdb->lock);
	ecdb->references++;
	UNLOCK(&ecdb->lock);

	*targetp = source;
}

static void
destroy_ecdb(dns_ecdb_t **ecdbp) {
	dns_ecdb_t *ecdb = *ecdbp;
	isc_mem_t *mctx = ecdb->common.mctx;

	if (dns_name_dynamic(&ecdb->common.origin))
		dns_name_free(&ecdb->common.origin, mctx);

	DESTROYLOCK(&ecdb->lock);

	ecdb->common.impmagic = 0;
	ecdb->common.magic = 0;

	isc_mem_putanddetach(&mctx, ecdb, sizeof(*ecdb));

	*ecdbp = NULL;
}

static void
detach(dns_db_t **dbp) {
	dns_ecdb_t *ecdb;
	isc_boolean_t need_destroy = ISC_FALSE;

	REQUIRE(dbp != NULL);
	ecdb = (dns_ecdb_t *)*dbp;
	REQUIRE(VALID_ECDB(ecdb));

	LOCK(&ecdb->lock);
	ecdb->references--;
	if (ecdb->references == 0 && ISC_LIST_EMPTY(ecdb->nodes))
		need_destroy = ISC_TRUE;
	UNLOCK(&ecdb->lock);

	if (need_destroy)
		destroy_ecdb(&ecdb);

	*dbp = NULL;
}

static void
attachnode(dns_db_t *db, dns_dbnode_t *source, dns_dbnode_t **targetp) {
	dns_ecdb_t *ecdb = (dns_ecdb_t *)db;
	dns_ecdbnode_t *node = (dns_ecdbnode_t *)source;

	REQUIRE(VALID_ECDB(ecdb));
	REQUIRE(VALID_ECDBNODE(node));
	REQUIRE(targetp != NULL && *targetp == NULL);

	LOCK(&node->lock);
	INSIST(node->references > 0);
	node->references++;
	INSIST(node->references != 0);		/* Catch overflow. */
	UNLOCK(&node->lock);

	*targetp = node;
}

static void
destroynode(dns_ecdbnode_t *node) {
	isc_mem_t *mctx;
	dns_ecdb_t *ecdb = node->ecdb;
	isc_boolean_t need_destroydb = ISC_FALSE;
	rdatasetheader_t *header;

	mctx = ecdb->common.mctx;

	LOCK(&ecdb->lock);
	ISC_LIST_UNLINK(ecdb->nodes, node, link);
	if (ecdb->references == 0 && ISC_LIST_EMPTY(ecdb->nodes))
		need_destroydb = ISC_TRUE;
	UNLOCK(&ecdb->lock);

	dns_name_free(&node->name, mctx);

	while ((header = ISC_LIST_HEAD(node->rdatasets)) != NULL) {
		unsigned int headersize;

		ISC_LIST_UNLINK(node->rdatasets, header, link);
		headersize =
			dns_rdataslab_size((unsigned char *)header,
					   sizeof(*header));
		isc_mem_put(mctx, header, headersize);
	}

	DESTROYLOCK(&node->lock);

	node->magic = 0;
	isc_mem_put(mctx, node, sizeof(*node));

	if (need_destroydb)
		destroy_ecdb(&ecdb);
}

static void
detachnode(dns_db_t *db, dns_dbnode_t **nodep) {
	dns_ecdb_t *ecdb = (dns_ecdb_t *)db;
	dns_ecdbnode_t *node;
	isc_boolean_t need_destroy = ISC_FALSE;

	REQUIRE(VALID_ECDB(ecdb));
	REQUIRE(nodep != NULL);
	node = (dns_ecdbnode_t *)*nodep;
	REQUIRE(VALID_ECDBNODE(node));

	UNUSED(ecdb);		/* in case REQUIRE() is empty */

	LOCK(&node->lock);
	INSIST(node->references > 0);
	node->references--;
	if (node->references == 0)
		need_destroy = ISC_TRUE;
	UNLOCK(&node->lock);

	if (need_destroy)
		destroynode(node);

	*nodep = NULL;
}

static isc_result_t
find(dns_db_t *db, dns_name_t *name, dns_dbversion_t *version,
    dns_rdatatype_t type, unsigned int options, isc_stdtime_t now,
    dns_dbnode_t **nodep, dns_name_t *foundname, dns_rdataset_t *rdataset,
    dns_rdataset_t *sigrdataset)
{
	dns_ecdb_t *ecdb = (dns_ecdb_t *)db;

	REQUIRE(VALID_ECDB(ecdb));

	UNUSED(name);
	UNUSED(version);
	UNUSED(type);
	UNUSED(options);
	UNUSED(now);
	UNUSED(nodep);
	UNUSED(foundname);
	UNUSED(rdataset);
	UNUSED(sigrdataset);

	return (ISC_R_NOTFOUND);
}

static isc_result_t
findzonecut(dns_db_t *db, dns_name_t *name,
	    unsigned int options, isc_stdtime_t now,
	    dns_dbnode_t **nodep, dns_name_t *foundname,
	    dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset)
{
	dns_ecdb_t *ecdb = (dns_ecdb_t *)db;

	REQUIRE(VALID_ECDB(ecdb));

	UNUSED(name);
	UNUSED(options);
	UNUSED(now);
	UNUSED(nodep);
	UNUSED(foundname);
	UNUSED(rdataset);
	UNUSED(sigrdataset);

	return (ISC_R_NOTFOUND);
}

static isc_result_t
findnode(dns_db_t *db, dns_name_t *name, isc_boolean_t create,
	 dns_dbnode_t **nodep)
{
	dns_ecdb_t *ecdb = (dns_ecdb_t *)db;
	isc_mem_t *mctx;
	dns_ecdbnode_t *node;
	isc_result_t result;

	REQUIRE(VALID_ECDB(ecdb));
	REQUIRE(nodep != NULL && *nodep == NULL);

	UNUSED(name);

	if (create != ISC_TRUE)	{
		/* an 'ephemeral' node is never reused. */
		return (ISC_R_NOTFOUND);
	}

	mctx = ecdb->common.mctx;
	node = isc_mem_get(mctx, sizeof(*node));
	if (node == NULL)
		return (ISC_R_NOMEMORY);

	result = isc_mutex_init(&node->lock);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_mutex_init() failed: %s",
				 isc_result_totext(result));
		isc_mem_put(mctx, node, sizeof(*node));
		return (ISC_R_UNEXPECTED);
	}

	dns_name_init(&node->name, NULL);
	result = dns_name_dup(name, mctx, &node->name);
	if (result != ISC_R_SUCCESS) {
		DESTROYLOCK(&node->lock);
		isc_mem_put(mctx, node, sizeof(*node));
		return (result);
	}
	node->ecdb= ecdb;
	node->references = 1;
	ISC_LIST_INIT(node->rdatasets);

	ISC_LINK_INIT(node, link);

	LOCK(&ecdb->lock);
	ISC_LIST_APPEND(ecdb->nodes, node, link);
	UNLOCK(&ecdb->lock);

	node->magic = ECDBNODE_MAGIC;

	*nodep = node;

	return (ISC_R_SUCCESS);
}

static void
bind_rdataset(dns_ecdb_t *ecdb, dns_ecdbnode_t *node,
	      rdatasetheader_t *header, dns_rdataset_t *rdataset)
{
	unsigned char *raw;

	/*
	 * Caller must be holding the node lock.
	 */

	REQUIRE(!dns_rdataset_isassociated(rdataset));

	rdataset->methods = &rdataset_methods;
	rdataset->rdclass = ecdb->common.rdclass;
	rdataset->type = header->type;
	rdataset->covers = header->covers;
	rdataset->ttl = header->ttl;
	rdataset->trust = header->trust;
	if (NXDOMAIN(header))
		rdataset->attributes |= DNS_RDATASETATTR_NXDOMAIN;

	rdataset->private1 = ecdb;
	rdataset->private2 = node;
	raw = (unsigned char *)header + sizeof(*header);
	rdataset->private3 = raw;
	rdataset->count = 0;

	/*
	 * Reset iterator state.
	 */
	rdataset->privateuint4 = 0;
	rdataset->private5 = NULL;

	INSIST(node->references > 0);
	node->references++;
}

static isc_result_t
addrdataset(dns_db_t *db, dns_dbnode_t *node, dns_dbversion_t *version,
	    isc_stdtime_t now, dns_rdataset_t *rdataset, unsigned int options,
	    dns_rdataset_t *addedrdataset)
{
	dns_ecdb_t *ecdb = (dns_ecdb_t *)db;
	isc_region_t r;
	isc_result_t result = ISC_R_SUCCESS;
	isc_mem_t *mctx;
	dns_ecdbnode_t *ecdbnode = (dns_ecdbnode_t *)node;
	rdatasetheader_t *header;

	REQUIRE(VALID_ECDB(ecdb));
	REQUIRE(VALID_ECDBNODE(ecdbnode));

	UNUSED(version);
	UNUSED(now);
	UNUSED(options);

	mctx = ecdb->common.mctx;

	LOCK(&ecdbnode->lock);

	/*
	 * Sanity check: this implementation does not allow overriding an
	 * existing rdataset of the same type.
	 */
	for (header = ISC_LIST_HEAD(ecdbnode->rdatasets); header != NULL;
	     header = ISC_LIST_NEXT(header, link)) {
		INSIST(header->type != rdataset->type ||
		       header->covers != rdataset->covers);
	}

	result = dns_rdataslab_fromrdataset(rdataset, mctx,
					    &r, sizeof(rdatasetheader_t));
	if (result != ISC_R_SUCCESS)
		goto unlock;

	header = (rdatasetheader_t *)r.base;
	header->type = rdataset->type;
	header->ttl = rdataset->ttl;
	header->trust = rdataset->trust;
	header->covers = rdataset->covers;
	header->attributes = 0;
	if ((rdataset->attributes & DNS_RDATASETATTR_NXDOMAIN) != 0)
		header->attributes |= RDATASET_ATTR_NXDOMAIN;
	ISC_LINK_INIT(header, link);
	ISC_LIST_APPEND(ecdbnode->rdatasets, header, link);

	if (addedrdataset == NULL)
		goto unlock;

	bind_rdataset(ecdb, ecdbnode, header, addedrdataset);

 unlock:
	UNLOCK(&ecdbnode->lock);

	return (result);
}

static isc_result_t
deleterdataset(dns_db_t *db, dns_dbnode_t *node, dns_dbversion_t *version,
	       dns_rdatatype_t type, dns_rdatatype_t covers)
{
	UNUSED(db);
	UNUSED(node);
	UNUSED(version);
	UNUSED(type);
	UNUSED(covers);

	return (ISC_R_NOTIMPLEMENTED);
}

static isc_result_t
createiterator(dns_db_t *db, unsigned int options,
	       dns_dbiterator_t **iteratorp)
{
	UNUSED(db);
	UNUSED(options);
	UNUSED(iteratorp);

	return (ISC_R_NOTIMPLEMENTED);
}

static isc_result_t
allrdatasets(dns_db_t *db, dns_dbnode_t *node, dns_dbversion_t *version,
	     isc_stdtime_t now, dns_rdatasetiter_t **iteratorp)
{
	dns_ecdb_t *ecdb = (dns_ecdb_t *)db;
	dns_ecdbnode_t *ecdbnode = (dns_ecdbnode_t *)node;
	isc_mem_t *mctx;
	ecdb_rdatasetiter_t *iterator;

	REQUIRE(VALID_ECDB(ecdb));
	REQUIRE(VALID_ECDBNODE(ecdbnode));

	mctx = ecdb->common.mctx;

	iterator = isc_mem_get(mctx, sizeof(ecdb_rdatasetiter_t));
	if (iterator == NULL)
		return (ISC_R_NOMEMORY);

	iterator->common.magic = DNS_RDATASETITER_MAGIC;
	iterator->common.methods = &rdatasetiter_methods;
	iterator->common.db = db;
	iterator->common.node = NULL;
	attachnode(db, node, &iterator->common.node);
	iterator->common.version = version;
	iterator->common.now = now;

	*iteratorp = (dns_rdatasetiter_t *)iterator;

	return (ISC_R_SUCCESS);
}

static dns_dbmethods_t ecdb_methods = {
	attach,
	detach,
	NULL,			/* beginload */
	NULL,			/* endload */
	NULL,			/* dump */
	NULL,			/* currentversion */
	NULL,			/* newversion */
	NULL,			/* attachversion */
	NULL,			/* closeversion */
	findnode,
	find,
	findzonecut,
	attachnode,
	detachnode,
	NULL,			/* expirenode */
	NULL,			/* printnode */
	createiterator,		/* createiterator */
	NULL,			/* findrdataset */
	allrdatasets,
	addrdataset,
	NULL,			/* subtractrdataset */
	deleterdataset,
	NULL,			/* issecure */
	NULL,			/* nodecount */
	NULL,			/* ispersistent */
	NULL,			/* overmem */
	NULL,			/* settask */
	NULL,			/* getoriginnode */
	NULL,			/* transfernode */
	NULL,			/* getnsec3parameters */
	NULL,			/* findnsec3node */
	NULL,			/* setsigningtime */
	NULL,			/* getsigningtime */
	NULL,			/* resigned */
	NULL,			/* isdnssec */
	NULL,			/* getrrsetstats */
	NULL,			/* rpz_enabled */
	NULL			/* rpz_findips */
};

static isc_result_t
dns_ecdb_create(isc_mem_t *mctx, dns_name_t *origin, dns_dbtype_t type,
		dns_rdataclass_t rdclass, unsigned int argc, char *argv[],
		void *driverarg, dns_db_t **dbp)
{
	dns_ecdb_t *ecdb;
	isc_result_t result;

	REQUIRE(mctx != NULL);
	REQUIRE(origin == dns_rootname);
	REQUIRE(type == dns_dbtype_cache);
	REQUIRE(dbp != NULL && *dbp == NULL);

	UNUSED(argc);
	UNUSED(argv);
	UNUSED(driverarg);

	ecdb = isc_mem_get(mctx, sizeof(*ecdb));
	if (ecdb == NULL)
		return (ISC_R_NOMEMORY);

	ecdb->common.attributes = DNS_DBATTR_CACHE;
	ecdb->common.rdclass = rdclass;
	ecdb->common.methods = &ecdb_methods;
	dns_name_init(&ecdb->common.origin, NULL);
	result = dns_name_dupwithoffsets(origin, mctx, &ecdb->common.origin);
	if (result != ISC_R_SUCCESS) {
		isc_mem_put(mctx, ecdb, sizeof(*ecdb));
		return (result);
	}

	result = isc_mutex_init(&ecdb->lock);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_mutex_init() failed: %s",
				 isc_result_totext(result));
		if (dns_name_dynamic(&ecdb->common.origin))
			dns_name_free(&ecdb->common.origin, mctx);
		isc_mem_put(mctx, ecdb, sizeof(*ecdb));
		return (ISC_R_UNEXPECTED);
	}

	ecdb->references = 1;
	ISC_LIST_INIT(ecdb->nodes);

	ecdb->common.mctx = NULL;
	isc_mem_attach(mctx, &ecdb->common.mctx);
	ecdb->common.impmagic = ECDB_MAGIC;
	ecdb->common.magic = DNS_DB_MAGIC;

	*dbp = (dns_db_t *)ecdb;

	return (ISC_R_SUCCESS);
}

/*%
 * Rdataset Methods
 */

static void
rdataset_disassociate(dns_rdataset_t *rdataset) {
	dns_db_t *db = rdataset->private1;
	dns_dbnode_t *node = rdataset->private2;

	dns_db_detachnode(db, &node);
}

static isc_result_t
rdataset_first(dns_rdataset_t *rdataset) {
	unsigned char *raw = rdataset->private3;
	unsigned int count;

	count = raw[0] * 256 + raw[1];
	if (count == 0) {
		rdataset->private5 = NULL;
		return (ISC_R_NOMORE);
	}
	raw += 2;
	/*
	 * The privateuint4 field is the number of rdata beyond the cursor
	 * position, so we decrement the total count by one before storing
	 * it.
	 */
	count--;
	rdataset->privateuint4 = count;
	rdataset->private5 = raw;

	return (ISC_R_SUCCESS);
}

static isc_result_t
rdataset_next(dns_rdataset_t *rdataset) {
	unsigned int count;
	unsigned int length;
	unsigned char *raw;

	count = rdataset->privateuint4;
	if (count == 0)
		return (ISC_R_NOMORE);
	count--;
	rdataset->privateuint4 = count;
	raw = rdataset->private5;
	length = raw[0] * 256 + raw[1];
	raw += length + 2;
	rdataset->private5 = raw;

	return (ISC_R_SUCCESS);
}

static void
rdataset_current(dns_rdataset_t *rdataset, dns_rdata_t *rdata) {
	unsigned char *raw = rdataset->private5;
	isc_region_t r;
	unsigned int length;
	unsigned int flags = 0;

	REQUIRE(raw != NULL);

	length = raw[0] * 256 + raw[1];
	raw += 2;
	if (rdataset->type == dns_rdatatype_rrsig) {
		if (*raw & DNS_RDATASLAB_OFFLINE)
			flags |= DNS_RDATA_OFFLINE;
		length--;
		raw++;
	}
	r.length = length;
	r.base = raw;
	dns_rdata_fromregion(rdata, rdataset->rdclass, rdataset->type, &r);
	rdata->flags |= flags;
}

static void
rdataset_clone(dns_rdataset_t *source, dns_rdataset_t *target) {
	dns_db_t *db = source->private1;
	dns_dbnode_t *node = source->private2;
	dns_dbnode_t *cloned_node = NULL;

	attachnode(db, node, &cloned_node);
	*target = *source;

	/*
	 * Reset iterator state.
	 */
	target->privateuint4 = 0;
	target->private5 = NULL;
}

static unsigned int
rdataset_count(dns_rdataset_t *rdataset) {
	unsigned char *raw = rdataset->private3;
	unsigned int count;

	count = raw[0] * 256 + raw[1];

	return (count);
}

static void
rdataset_settrust(dns_rdataset_t *rdataset, dns_trust_t trust) {
	rdatasetheader_t *header = rdataset->private3;

	header--;
	header->trust = rdataset->trust = trust;
}

/*
 * Rdataset Iterator Methods
 */

static void
rdatasetiter_destroy(dns_rdatasetiter_t **iteratorp) {
	ecdb_rdatasetiter_t *ecdbiterator;
	isc_mem_t *mctx;

	REQUIRE(iteratorp != NULL);
	ecdbiterator = (ecdb_rdatasetiter_t *)*iteratorp;
	REQUIRE(DNS_RDATASETITER_VALID(&ecdbiterator->common));

	mctx = ecdbiterator->common.db->mctx;

	ecdbiterator->common.magic = 0;

	dns_db_detachnode(ecdbiterator->common.db, &ecdbiterator->common.node);
	isc_mem_put(mctx, ecdbiterator, sizeof(ecdb_rdatasetiter_t));

	*iteratorp = NULL;
}

static isc_result_t
rdatasetiter_first(dns_rdatasetiter_t *iterator) {
	ecdb_rdatasetiter_t *ecdbiterator = (ecdb_rdatasetiter_t *)iterator;
	dns_ecdbnode_t *ecdbnode = (dns_ecdbnode_t *)iterator->node;

	REQUIRE(DNS_RDATASETITER_VALID(iterator));

	if (ISC_LIST_EMPTY(ecdbnode->rdatasets))
		return (ISC_R_NOMORE);
	ecdbiterator->current = ISC_LIST_HEAD(ecdbnode->rdatasets);
	return (ISC_R_SUCCESS);
}

static isc_result_t
rdatasetiter_next(dns_rdatasetiter_t *iterator) {
	ecdb_rdatasetiter_t *ecdbiterator = (ecdb_rdatasetiter_t *)iterator;

	REQUIRE(DNS_RDATASETITER_VALID(iterator));

	ecdbiterator->current = ISC_LIST_NEXT(ecdbiterator->current, link);
	if (ecdbiterator->current == NULL)
		return (ISC_R_NOMORE);
	else
		return (ISC_R_SUCCESS);
}

static void
rdatasetiter_current(dns_rdatasetiter_t *iterator, dns_rdataset_t *rdataset) {
	ecdb_rdatasetiter_t *ecdbiterator = (ecdb_rdatasetiter_t *)iterator;
	dns_ecdb_t *ecdb;

	ecdb = (dns_ecdb_t *)iterator->db;
	REQUIRE(VALID_ECDB(ecdb));

	bind_rdataset(ecdb, iterator->node, ecdbiterator->current, rdataset);
}
