/*
 * Copyright (C) 2004, 2006  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: acl.c,v 1.23.52.6 2006/03/02 00:37:20 marka Exp $ */

#include <config.h>

#include <isc/mem.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/acl.h>

isc_result_t
dns_acl_create(isc_mem_t *mctx, int n, dns_acl_t **target) {
	isc_result_t result;
	dns_acl_t *acl;

	/*
	 * Work around silly limitation of isc_mem_get().
	 */
	if (n == 0)
		n = 1;

	acl = isc_mem_get(mctx, sizeof(*acl));
	if (acl == NULL)
		return (ISC_R_NOMEMORY);
	acl->mctx = mctx;
	acl->name = NULL;
	isc_refcount_init(&acl->refcount, 1);
	acl->elements = NULL;
	acl->alloc = 0;
	acl->length = 0;

	ISC_LINK_INIT(acl, nextincache);
	/*
	 * Must set magic early because we use dns_acl_detach() to clean up.
	 */
	acl->magic = DNS_ACL_MAGIC;

	acl->elements = isc_mem_get(mctx, n * sizeof(dns_aclelement_t));
	if (acl->elements == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}
	acl->alloc = n;
	memset(acl->elements, 0, n * sizeof(dns_aclelement_t));
	*target = acl;
	return (ISC_R_SUCCESS);

 cleanup:
	dns_acl_detach(&acl);
	return (result);
}

isc_result_t
dns_acl_appendelement(dns_acl_t *acl, const dns_aclelement_t *elt) {
	if (acl->length + 1 > acl->alloc) {
		/*
		 * Resize the ACL.
		 */
		unsigned int newalloc;
		void *newmem;

		newalloc = acl->alloc * 2;
		if (newalloc < 4)
			newalloc = 4;
		newmem = isc_mem_get(acl->mctx,
				     newalloc * sizeof(dns_aclelement_t));
		if (newmem == NULL)
			return (ISC_R_NOMEMORY);
		memcpy(newmem, acl->elements,
		       acl->length * sizeof(dns_aclelement_t));
		isc_mem_put(acl->mctx, acl->elements,
			    acl->alloc * sizeof(dns_aclelement_t));
		acl->elements = newmem;
		acl->alloc = newalloc;
	}
	/*
	 * Append the new element.
	 */
	acl->elements[acl->length++] = *elt;

	return (ISC_R_SUCCESS);
}

static isc_result_t
dns_acl_anyornone(isc_mem_t *mctx, isc_boolean_t neg, dns_acl_t **target) {
	isc_result_t result;
	dns_acl_t *acl = NULL;
	result = dns_acl_create(mctx, 1, &acl);
	if (result != ISC_R_SUCCESS)
		return (result);
	acl->elements[0].negative = neg;
	acl->elements[0].type = dns_aclelementtype_any;
	acl->length = 1;
	*target = acl;
	return (result);
}

isc_result_t
dns_acl_any(isc_mem_t *mctx, dns_acl_t **target) {
	return (dns_acl_anyornone(mctx, ISC_FALSE, target));
}

isc_result_t
dns_acl_none(isc_mem_t *mctx, dns_acl_t **target) {
	return (dns_acl_anyornone(mctx, ISC_TRUE, target));
}

isc_result_t
dns_acl_match(const isc_netaddr_t *reqaddr,
	      const dns_name_t *reqsigner,
	      const dns_acl_t *acl,
	      const dns_aclenv_t *env,
	      int *match,
	      dns_aclelement_t const**matchelt)
{
	unsigned int i;

	REQUIRE(reqaddr != NULL);
	REQUIRE(matchelt == NULL || *matchelt == NULL);
	
	for (i = 0; i < acl->length; i++) {
		dns_aclelement_t *e = &acl->elements[i];

		if (dns_aclelement_match(reqaddr, reqsigner,
					 e, env, matchelt)) {
			*match = e->negative ? -((int)i+1) : ((int)i+1);
			return (ISC_R_SUCCESS);
		}
	}
	/* No match. */
	*match = 0;
	return (ISC_R_SUCCESS);
}

isc_result_t
dns_acl_elementmatch(const dns_acl_t *acl,
		     const dns_aclelement_t *elt,
		     const dns_aclelement_t **matchelt)
{
	unsigned int i;

	REQUIRE(elt != NULL);
	REQUIRE(matchelt == NULL || *matchelt == NULL);
	
	for (i = 0; i < acl->length; i++) {
		dns_aclelement_t *e = &acl->elements[i];

		if (dns_aclelement_equal(e, elt) == ISC_TRUE) {
			if (matchelt != NULL)
				*matchelt = e;
			return (ISC_R_SUCCESS);
		}
	}

	return (ISC_R_NOTFOUND);
}

isc_boolean_t
dns_aclelement_match(const isc_netaddr_t *reqaddr,
		     const dns_name_t *reqsigner,
		     const dns_aclelement_t *e,
		     const dns_aclenv_t *env,
		     const dns_aclelement_t **matchelt)
{
	dns_acl_t *inner = NULL;
	const isc_netaddr_t *addr;
	isc_netaddr_t v4addr;
	int indirectmatch;
	isc_result_t result;

	switch (e->type) {
	case dns_aclelementtype_ipprefix:
		if (env == NULL ||
		    env->match_mapped == ISC_FALSE ||
		    reqaddr->family != AF_INET6 ||
		    !IN6_IS_ADDR_V4MAPPED(&reqaddr->type.in6))
			addr = reqaddr;
		else {
			isc_netaddr_fromv4mapped(&v4addr, reqaddr);
			addr = &v4addr;
		}

		if (isc_netaddr_eqprefix(addr,
					 &e->u.ip_prefix.address, 
					 e->u.ip_prefix.prefixlen))
			goto matched;
		break;
		
	case dns_aclelementtype_keyname:
		if (reqsigner != NULL &&
		    dns_name_equal(reqsigner, &e->u.keyname))
			goto matched;
		break;
		
	case dns_aclelementtype_nestedacl:
		inner = e->u.nestedacl;
	nested:
		result = dns_acl_match(reqaddr, reqsigner,
				       inner,
				       env,
				       &indirectmatch, matchelt);
		INSIST(result == ISC_R_SUCCESS);

		/*
		 * Treat negative matches in indirect ACLs as
		 * "no match".
		 * That way, a negated indirect ACL will never become 
		 * a surprise positive match through double negation.
		 * XXXDCL this should be documented.
		 */
		if (indirectmatch > 0)
			goto matchelt_set;
		
		/*
		 * A negative indirect match may have set *matchelt,
		 * but we don't want it set when we return.
		 */
		if (matchelt != NULL)
			*matchelt = NULL;
		break;
		
	case dns_aclelementtype_any:
	matched:
		if (matchelt != NULL)
			*matchelt = e;
	matchelt_set:
		return (ISC_TRUE);
			
	case dns_aclelementtype_localhost:
		if (env != NULL && env->localhost != NULL) {
			inner = env->localhost;
			goto nested;
		} else {
			break;
		}
		
	case dns_aclelementtype_localnets:
		if (env != NULL && env->localnets != NULL) {
			inner = env->localnets;
			goto nested;
		} else {
			break;
		}
		
	default:
		INSIST(0);
		break;
	}

	return (ISC_FALSE);
}	

void
dns_acl_attach(dns_acl_t *source, dns_acl_t **target) {
	REQUIRE(DNS_ACL_VALID(source));
	isc_refcount_increment(&source->refcount, NULL);
	*target = source;
}

static void
destroy(dns_acl_t *dacl) {
	unsigned int i;
	for (i = 0; i < dacl->length; i++) {
		dns_aclelement_t *de = &dacl->elements[i];
		switch (de->type) {
		case dns_aclelementtype_keyname:
			dns_name_free(&de->u.keyname, dacl->mctx);
			break;
		case dns_aclelementtype_nestedacl:
			dns_acl_detach(&de->u.nestedacl);
			break;
		default:
			break;
		}
	}
	if (dacl->elements != NULL)
		isc_mem_put(dacl->mctx, dacl->elements,
			    dacl->alloc * sizeof(dns_aclelement_t));
	if (dacl->name != NULL)
		isc_mem_free(dacl->mctx, dacl->name);
	isc_refcount_destroy(&dacl->refcount);
	dacl->magic = 0;
	isc_mem_put(dacl->mctx, dacl, sizeof(*dacl));
}

void
dns_acl_detach(dns_acl_t **aclp) {
	dns_acl_t *acl = *aclp;
	unsigned int refs;
	REQUIRE(DNS_ACL_VALID(acl));
	isc_refcount_decrement(&acl->refcount, &refs);
	if (refs == 0)
		destroy(acl);
	*aclp = NULL;
}

isc_boolean_t
dns_aclelement_equal(const dns_aclelement_t *ea, const dns_aclelement_t *eb) {
	if (ea->type != eb->type)
		return (ISC_FALSE);
	switch (ea->type) {
	case dns_aclelementtype_ipprefix:
		if (ea->u.ip_prefix.prefixlen !=
		    eb->u.ip_prefix.prefixlen)
			return (ISC_FALSE);
		return (isc_netaddr_eqprefix(&ea->u.ip_prefix.address,
					     &eb->u.ip_prefix.address,
					     ea->u.ip_prefix.prefixlen));
	case dns_aclelementtype_keyname:
		return (dns_name_equal(&ea->u.keyname, &eb->u.keyname));
	case dns_aclelementtype_nestedacl:
		return (dns_acl_equal(ea->u.nestedacl, eb->u.nestedacl));
	case dns_aclelementtype_localhost:
	case dns_aclelementtype_localnets:
	case dns_aclelementtype_any:
		return (ISC_TRUE);
	default:
		INSIST(0);
		return (ISC_FALSE);
	}
}

isc_boolean_t
dns_acl_equal(const dns_acl_t *a, const dns_acl_t *b) {
	unsigned int i;
	if (a == b)
		return (ISC_TRUE);
	if (a->length != b->length)
		return (ISC_FALSE);
	for (i = 0; i < a->length; i++) {
		if (! dns_aclelement_equal(&a->elements[i],
					   &b->elements[i]))
			return (ISC_FALSE);
	}
	return (ISC_TRUE);
}

static isc_boolean_t
is_loopback(const dns_aclipprefix_t *p) {
	switch (p->address.family) {
	case AF_INET:
		if (p->prefixlen == 32 &&
		    htonl(p->address.type.in.s_addr) == INADDR_LOOPBACK)
			return (ISC_TRUE);
		break;
	case AF_INET6:
		if (p->prefixlen == 128 &&
		    IN6_IS_ADDR_LOOPBACK(&p->address.type.in6))
			return (ISC_TRUE);
		break;
	default:
		break;
	}
	return (ISC_FALSE);
}

isc_boolean_t
dns_acl_isinsecure(const dns_acl_t *a) {
	unsigned int i;
	for (i = 0; i < a->length; i++) {
		dns_aclelement_t *e = &a->elements[i];

		/* A negated match can never be insecure. */
		if (e->negative)
			continue;

		switch (e->type) {
		case dns_aclelementtype_ipprefix:
			/* The loopback address is considered secure. */
			if (! is_loopback(&e->u.ip_prefix))
				return (ISC_TRUE);
			continue;
			
		case dns_aclelementtype_keyname:
		case dns_aclelementtype_localhost:
			continue;

		case dns_aclelementtype_nestedacl:
			if (dns_acl_isinsecure(e->u.nestedacl))
				return (ISC_TRUE);
			continue;
			
		case dns_aclelementtype_localnets:
		case dns_aclelementtype_any:
			return (ISC_TRUE);

		default:
			INSIST(0);
			return (ISC_TRUE);
		}
	}
	/* No insecure elements were found. */
	return (ISC_FALSE);
}

isc_result_t
dns_aclenv_init(isc_mem_t *mctx, dns_aclenv_t *env) {
	isc_result_t result;
	env->localhost = NULL;
	env->localnets = NULL;
	result = dns_acl_create(mctx, 0, &env->localhost);
	if (result != ISC_R_SUCCESS)
		goto cleanup_nothing;
	result = dns_acl_create(mctx, 0, &env->localnets);
	if (result != ISC_R_SUCCESS)
		goto cleanup_localhost;
	env->match_mapped = ISC_FALSE;
	return (ISC_R_SUCCESS);

 cleanup_localhost:
	dns_acl_detach(&env->localhost);
 cleanup_nothing:
	return (result);
}

void
dns_aclenv_copy(dns_aclenv_t *t, dns_aclenv_t *s) {
	dns_acl_detach(&t->localhost);
	dns_acl_attach(s->localhost, &t->localhost);
	dns_acl_detach(&t->localnets);
	dns_acl_attach(s->localnets, &t->localnets);
	t->match_mapped = s->match_mapped;
}

void
dns_aclenv_destroy(dns_aclenv_t *env) {
	dns_acl_detach(&env->localhost);
	dns_acl_detach(&env->localnets);
}
