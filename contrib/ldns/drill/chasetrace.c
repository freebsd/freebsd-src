/*
 * chasetrace.c
 * Where all the hard work concerning chasing
 * and tracing is done
 * (c) 2005, 2006 NLnet Labs
 *
 * See the file LICENSE for the license
 *
 */

#include "drill.h"
#include <ldns/ldns.h>

/**
 * trace down from the root to name
 */

/* same naive method as in drill0.9 
 * We resolver _ALL_ the names, which is ofcourse not needed
 * We _do_ use the local resolver to do that, so it still is
 * fast, but it can be made to run much faster
 */
ldns_pkt *
do_trace(ldns_resolver *local_res, ldns_rdf *name, ldns_rr_type t,
		ldns_rr_class c)
{
	ldns_resolver *res;
	ldns_pkt *p;
	ldns_rr_list *new_nss_a;
	ldns_rr_list *new_nss_aaaa;
	ldns_rr_list *final_answer;
	ldns_rr_list *new_nss;
	ldns_rr_list *ns_addr;
	uint16_t loop_count;
	ldns_rdf *pop; 
	ldns_status status;
	size_t i;
	
	loop_count = 0;
	new_nss_a = NULL;
	new_nss_aaaa = NULL;
	new_nss = NULL;
	ns_addr = NULL;
	final_answer = NULL;
	p = ldns_pkt_new();
	res = ldns_resolver_new();

	if (!p) {
		if (res) {
			ldns_resolver_free(res);
		}
                error("Memory allocation failed");
                return NULL;
	}
	if (!res) {
		ldns_pkt_free(p);
                error("Memory allocation failed");
                return NULL;
        }

	/* transfer some properties of local_res to res,
	 * because they were given on the commandline */
	ldns_resolver_set_ip6(res, 
			ldns_resolver_ip6(local_res));
	ldns_resolver_set_port(res, 
			ldns_resolver_port(local_res));
	ldns_resolver_set_debug(res, 
			ldns_resolver_debug(local_res));
	ldns_resolver_set_dnssec(res, 
			ldns_resolver_dnssec(local_res));
	ldns_resolver_set_fail(res, 
			ldns_resolver_fail(local_res));
	ldns_resolver_set_usevc(res, 
			ldns_resolver_usevc(local_res));
	ldns_resolver_set_random(res, 
			ldns_resolver_random(local_res));
	ldns_resolver_set_recursive(res, false);

	/* setup the root nameserver in the new resolver */
	status = ldns_resolver_push_nameserver_rr_list(res, global_dns_root);
	if (status != LDNS_STATUS_OK) {
		fprintf(stderr, "Error adding root servers to resolver: %s\n", ldns_get_errorstr_by_id(status));
		ldns_rr_list_print(stdout, global_dns_root);
		ldns_resolver_free(res);
		ldns_pkt_free(p);
		return NULL;
	}

	/* this must be a real query to local_res */
	status = ldns_resolver_send(&p, res, ldns_dname_new_frm_str("."), LDNS_RR_TYPE_NS, c, 0);
	/* p can still be NULL */


	if (ldns_pkt_empty(p)) {
		warning("No root server information received");
	} 
	
	if (status == LDNS_STATUS_OK) {
		if (!ldns_pkt_empty(p)) {
			drill_pkt_print(stdout, local_res, p);
		}
	} else {
		error("cannot use local resolver");
		return NULL;
	}

	status = ldns_resolver_send(&p, res, name, t, c, 0);

	while(status == LDNS_STATUS_OK && 
	      ldns_pkt_reply_type(p) == LDNS_PACKET_REFERRAL) {

		if (!p) {
			/* some error occurred, bail out */
			return NULL;
		}

		new_nss_a = ldns_pkt_rr_list_by_type(p,
				LDNS_RR_TYPE_A, LDNS_SECTION_ADDITIONAL);
		new_nss_aaaa = ldns_pkt_rr_list_by_type(p,
				LDNS_RR_TYPE_AAAA, LDNS_SECTION_ADDITIONAL);
		new_nss = ldns_pkt_rr_list_by_type(p,
				LDNS_RR_TYPE_NS, LDNS_SECTION_AUTHORITY);

		if (verbosity != -1) {
			ldns_rr_list_print(stdout, new_nss);
		}
		/* checks itself for verbosity */
		drill_pkt_print_footer(stdout, local_res, p);
		
		/* remove the old nameserver from the resolver */
		while(ldns_resolver_pop_nameserver(res)) { /* do it */ }

		/* also check for new_nss emptyness */

		if (!new_nss_aaaa && !new_nss_a) {
			/* 
			 * no nameserver found!!! 
			 * try to resolve the names we do got 
			 */
			for(i = 0; i < ldns_rr_list_rr_count(new_nss); i++) {
				/* get the name of the nameserver */
				pop = ldns_rr_rdf(ldns_rr_list_rr(new_nss, i), 0);
				if (!pop) {
					break;
				}

				ldns_rr_list_print(stdout, new_nss);
				ldns_rdf_print(stdout, pop);
				/* retrieve it's addresses */
				ns_addr = ldns_rr_list_cat_clone(ns_addr,
					ldns_get_rr_list_addr_by_name(local_res, pop, c, 0));
			}

			if (ns_addr) {
				if (ldns_resolver_push_nameserver_rr_list(res, ns_addr) != 
						LDNS_STATUS_OK) {
					error("Error adding new nameservers");
					ldns_pkt_free(p); 
					return NULL;
				}
				ldns_rr_list_free(ns_addr);
			} else {
				ldns_rr_list_print(stdout, ns_addr);
				error("Could not find the nameserver ip addr; abort");
				ldns_pkt_free(p);
				return NULL;
			}
		}

		/* add the new ones */
		if (new_nss_aaaa) {
			if (ldns_resolver_push_nameserver_rr_list(res, new_nss_aaaa) != 
					LDNS_STATUS_OK) {
				error("adding new nameservers");
				ldns_pkt_free(p); 
				return NULL;
			}
		}
		if (new_nss_a) {
			if (ldns_resolver_push_nameserver_rr_list(res, new_nss_a) != 
					LDNS_STATUS_OK) {
				error("adding new nameservers");
				ldns_pkt_free(p); 
				return NULL;
			}
		}

		if (loop_count++ > 20) {
			/* unlikely that we are doing something usefull */
			error("Looks like we are looping");
			ldns_pkt_free(p); 
			return NULL;
		}
		
		status = ldns_resolver_send(&p, res, name, t, c, 0);
		new_nss_aaaa = NULL;
		new_nss_a = NULL;
		ns_addr = NULL;
	}

	status = ldns_resolver_send(&p, res, name, t, c, 0);

	if (!p) {
		return NULL;
	}

	new_nss = ldns_pkt_authority(p);
	final_answer = ldns_pkt_answer(p);

	if (verbosity != -1) {
		ldns_rr_list_print(stdout, final_answer);
		ldns_rr_list_print(stdout, new_nss);

	}
	drill_pkt_print_footer(stdout, local_res, p);
	ldns_pkt_free(p); 
	return NULL;
}


/**
 * Chase the given rr to a known and trusted key
 *
 * Based on drill 0.9
 *
 * the last argument prev_key_list, if not null, and type == DS, then the ds
 * rr list we have must all be a ds for the keys in this list
 */
#ifdef HAVE_SSL
ldns_status
do_chase(ldns_resolver *res,
	    ldns_rdf *name,
	    ldns_rr_type type,
	    ldns_rr_class c,
	    ldns_rr_list *trusted_keys,
	    ldns_pkt *pkt_o,
	    uint16_t qflags,
	    ldns_rr_list * ATTR_UNUSED(prev_key_list),
	    int verbosity)
{
	ldns_rr_list *rrset = NULL;
	ldns_status result;
	ldns_rr *orig_rr = NULL;
	
/*
	ldns_rr_list *sigs;
	ldns_rr *cur_sig;
	uint16_t sig_i;
	ldns_rr_list *keys;
*/
	ldns_pkt *pkt;
	ldns_status tree_result;
	ldns_dnssec_data_chain *chain;
	ldns_dnssec_trust_tree *tree;
	
	const ldns_rr_descriptor *descriptor;
	descriptor = ldns_rr_descript(type);

	ldns_dname2canonical(name);
	
	pkt = ldns_pkt_clone(pkt_o);
	if (!name) {
		mesg("No name to chase");
		ldns_pkt_free(pkt);
		return LDNS_STATUS_EMPTY_LABEL;
	}
	if (verbosity != -1) {
		printf(";; Chasing: ");
			ldns_rdf_print(stdout, name);
			if (descriptor && descriptor->_name) {
				printf(" %s\n", descriptor->_name);
			} else {
				printf(" type %d\n", type);
			}
	}

	if (!trusted_keys || ldns_rr_list_rr_count(trusted_keys) < 1) {
		warning("No trusted keys specified");
	}
	
	if (pkt) {
		rrset = ldns_pkt_rr_list_by_name_and_type(pkt,
				name,
				type,
				LDNS_SECTION_ANSWER
				);
		if (!rrset) {
			/* nothing in answer, try authority */
			rrset = ldns_pkt_rr_list_by_name_and_type(pkt,
					name,
					type,
					LDNS_SECTION_AUTHORITY
					);
		}
		/* answer might be a cname, chase that first, then chase
		   cname target? (TODO) */
		if (!rrset) {
			rrset = ldns_pkt_rr_list_by_name_and_type(pkt,
					name,
					LDNS_RR_TYPE_CNAME,
					LDNS_SECTION_ANSWER
					);
			if (!rrset) {
				/* nothing in answer, try authority */
				rrset = ldns_pkt_rr_list_by_name_and_type(pkt,
						name,
						LDNS_RR_TYPE_CNAME,
						LDNS_SECTION_AUTHORITY
						);
			}
		}
	} else {
		/* no packet? */
		if (verbosity >= 0) {
			fprintf(stderr, "%s", ldns_get_errorstr_by_id(LDNS_STATUS_MEM_ERR));
			fprintf(stderr, "\n");
		}
		return LDNS_STATUS_MEM_ERR;
	}
	
	if (!rrset) {
		/* not found in original packet, try again */
		ldns_pkt_free(pkt);
		pkt = NULL;
		pkt = ldns_resolver_query(res, name, type, c, qflags);
		
		if (!pkt) {
			if (verbosity >= 0) {
				fprintf(stderr, "%s", ldns_get_errorstr_by_id(LDNS_STATUS_NETWORK_ERR));
				fprintf(stderr, "\n");
			}
			return LDNS_STATUS_NETWORK_ERR;
		}
		if (verbosity >= 5) {
			ldns_pkt_print(stdout, pkt);
		}
		
		rrset =	ldns_pkt_rr_list_by_name_and_type(pkt,
				name,
				type,
				LDNS_SECTION_ANSWER
				);
	}
	
	orig_rr = ldns_rr_new();

/* if the answer had no answer section, we need to construct our own rr (for instance if
 * the rr qe asked for doesn't exist. This rr will be destroyed when the chain is freed */
	if (ldns_pkt_ancount(pkt) < 1) {
		ldns_rr_set_type(orig_rr, type);
		ldns_rr_set_owner(orig_rr, ldns_rdf_clone(name));
	
		chain = ldns_dnssec_build_data_chain(res, qflags, rrset, pkt, ldns_rr_clone(orig_rr));
	} else {
		/* chase the first answer */
		chain = ldns_dnssec_build_data_chain(res, qflags, rrset, pkt, NULL);
	}

	if (verbosity >= 4) {
		printf("\n\nDNSSEC Data Chain:\n");
		ldns_dnssec_data_chain_print(stdout, chain);
	}
	
	result = LDNS_STATUS_OK;

	tree = ldns_dnssec_derive_trust_tree(chain, NULL);

	if (verbosity >= 2) {
		printf("\n\nDNSSEC Trust tree:\n");
		ldns_dnssec_trust_tree_print(stdout, tree, 0, true);
	}

	if (ldns_rr_list_rr_count(trusted_keys) > 0) {
		tree_result = ldns_dnssec_trust_tree_contains_keys(tree, trusted_keys);

		if (tree_result == LDNS_STATUS_DNSSEC_EXISTENCE_DENIED) {
			if (verbosity >= 1) {
				printf("Existence denied or verifiably insecure\n");
			}
			result = LDNS_STATUS_OK;
		} else if (tree_result != LDNS_STATUS_OK) {
			if (verbosity >= 1) {
				printf("No trusted keys found in tree: first error was: %s\n", ldns_get_errorstr_by_id(tree_result));
			}
			result = tree_result;
		}

	} else {
		if (verbosity >= 0) {
			printf("You have not provided any trusted keys.\n");
		}
	}
	
	ldns_rr_free(orig_rr);
	ldns_dnssec_trust_tree_free(tree);
	ldns_dnssec_data_chain_deep_free(chain);
	
	ldns_rr_list_deep_free(rrset);
	ldns_pkt_free(pkt);
	/*	ldns_rr_free(orig_rr);*/

	return result;
}
#endif /* HAVE_SSL */

