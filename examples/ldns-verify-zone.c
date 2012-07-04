/*
 * read a zone file from disk and prints it, one RR per line
 *
 * (c) NLnetLabs 2008
 *
 * See the file LICENSE for the license
 *
 * Missing from the checks: empty non-terminals
 */

#include "config.h"
#include <unistd.h>
#include <stdlib.h>

#include <ldns/ldns.h>

#include <errno.h>

#ifdef HAVE_SSL
#include <openssl/err.h>

static int verbosity = 3;
static time_t check_time = 0;
static int32_t inception_offset = 0;
static int32_t expiration_offset = 0;
static bool do_sigchase = false;
static bool no_nomatch_msg = false;

static FILE* myout;
static FILE* myerr;

static void
update_error(ldns_status* result, ldns_status status)
{
	if (status != LDNS_STATUS_OK) {
		if (*result == LDNS_STATUS_OK || *result == LDNS_STATUS_ERR || 
		    (  *result == LDNS_STATUS_CRYPTO_NO_MATCHING_KEYTAG_DNSKEY 
		     && status != LDNS_STATUS_ERR 
		    )) {
			*result = status;
		}
	}
}

static void
print_type(FILE* stream, ldns_rr_type type)
{
	const ldns_rr_descriptor *descriptor  = ldns_rr_descript(type);

	if (descriptor && descriptor->_name) {
		fprintf(stream, "%s", descriptor->_name);
	} else {
		fprintf(stream, "TYPE%u", type);
	}
}

ldns_status
read_key_file(const char *filename, ldns_rr_list *keys)
{
	ldns_status status = LDNS_STATUS_ERR;
	ldns_rr *rr;
	FILE *fp;
	uint32_t my_ttl = 0;
	ldns_rdf *my_origin = NULL;
	ldns_rdf *my_prev = NULL;
	int line_nr;

	if (!(fp = fopen(filename, "r"))) {
		if (verbosity > 0) {
			fprintf(myerr, "Error opening %s: %s\n", filename,
					strerror(errno));
		}
		return LDNS_STATUS_FILE_ERR;
	}
	while (!feof(fp)) {
		status = ldns_rr_new_frm_fp_l(&rr, fp, &my_ttl, &my_origin,
				&my_prev, &line_nr);

		if (status == LDNS_STATUS_OK) {

			if (   ldns_rr_get_type(rr) == LDNS_RR_TYPE_DS
			    || ldns_rr_get_type(rr) == LDNS_RR_TYPE_DNSKEY)

				ldns_rr_list_push_rr(keys, rr);

		} else if (   status == LDNS_STATUS_SYNTAX_EMPTY
		           || status == LDNS_STATUS_SYNTAX_TTL
		           || status == LDNS_STATUS_SYNTAX_ORIGIN
		           || status == LDNS_STATUS_SYNTAX_INCLUDE)

			status = LDNS_STATUS_OK;
		else
			break;
	}
	return status;
}



static void
print_rr_error(FILE* stream, ldns_rr* rr, const char* msg)
{
	if (verbosity > 0) {
		fprintf(stream, "Error: %s for ", msg);
		ldns_rdf_print(stream, ldns_rr_owner(rr));
		fprintf(stream, "\t");
		print_type(stream, ldns_rr_get_type(rr));
		fprintf(stream, "\n");
	}
}

static void
print_rr_status_error(FILE* stream, ldns_rr* rr, ldns_status status)
{
	if (status != LDNS_STATUS_OK) {
		print_rr_error(stream, rr, ldns_get_errorstr_by_id(status));
		if (verbosity > 0 && status == LDNS_STATUS_SSL_ERR) {
			ERR_load_crypto_strings();
			ERR_print_errors_fp(stream);
		}
	}
}

static void
print_rrs_status_error(FILE* stream, ldns_rr_list* rrs, ldns_status status,
		ldns_dnssec_rrs* cur_sig)
{
	if (status != LDNS_STATUS_OK) {
		if (ldns_rr_list_rr_count(rrs) > 0) {
			print_rr_status_error(stream, ldns_rr_list_rr(rrs, 0),
					status);
		} else if (verbosity > 0) {
			fprintf(stream, "Error: %s for <unknown>\n",
					ldns_get_errorstr_by_id(status));
		}
		if (verbosity >= 4) {
			fprintf(stream, "RRSet:\n");
			ldns_rr_list_print(stream, rrs);
			fprintf(stream, "Signature:\n");
			ldns_rr_print(stream, cur_sig->rr);
			fprintf(stream, "\n");
		}
	}
}

static ldns_status
rrsig_check_time_margins(ldns_rr* rrsig
#if 0 /* Passing those as arguments becomes sensible when 
       * rrsig_check_time_margins will be added to the library.
       */
	,time_t check_time, int32_t inception_offset, int32_t expiration_offset
#endif
	)
{
	int32_t inception, expiration;

	inception  = ldns_rdf2native_int32(ldns_rr_rrsig_inception (rrsig));
	expiration = ldns_rdf2native_int32(ldns_rr_rrsig_expiration(rrsig));

	if (((int32_t) (check_time - inception_offset))  - inception  < 0) {
		return LDNS_STATUS_CRYPTO_SIG_NOT_INCEPTED_WITHIN_MARGIN;
	}
	if (expiration - ((int32_t) (check_time + expiration_offset)) < 0) {
		return LDNS_STATUS_CRYPTO_SIG_EXPIRED_WITHIN_MARGIN;
	}
	return LDNS_STATUS_OK;
}

static ldns_status
verify_rrs(ldns_rr_list* rrset_rrs, ldns_dnssec_rrs* cur_sig,
		ldns_rr_list* keys)
{
	ldns_rr_list* good_keys;
	ldns_status status, result = LDNS_STATUS_OK;

	while (cur_sig) {
		good_keys = ldns_rr_list_new();
		status = ldns_verify_rrsig_keylist_time(rrset_rrs, cur_sig->rr,
				keys, check_time, good_keys);
		status = status ? status 
				: rrsig_check_time_margins(cur_sig->rr);
		if (status != LDNS_STATUS_CRYPTO_NO_MATCHING_KEYTAG_DNSKEY ||
			       	!no_nomatch_msg) {

			print_rrs_status_error(myerr, rrset_rrs, status,
					cur_sig);
		}
		update_error(&result, status);
		ldns_rr_list_free(good_keys);
		cur_sig = cur_sig->next;
	}
	return result;
}

static ldns_status
verify_dnssec_rrset(ldns_rdf *zone_name, ldns_rdf *name,
		ldns_dnssec_rrsets *rrset, ldns_rr_list *keys) 
{
	ldns_rr_list *rrset_rrs;
	ldns_dnssec_rrs *cur_rr, *cur_sig;
	ldns_status status;

	if (!rrset->rrs) return LDNS_STATUS_OK;

	rrset_rrs = ldns_rr_list_new();
	cur_rr = rrset->rrs;
	while(cur_rr && cur_rr->rr) {
		ldns_rr_list_push_rr(rrset_rrs, cur_rr->rr);
		cur_rr = cur_rr->next;
	}
	cur_sig = rrset->signatures;
	if (cur_sig) {
		status = verify_rrs(rrset_rrs, cur_sig, keys);

	} else /* delegations may be unsigned (on opt out...) */
	       if (rrset->type != LDNS_RR_TYPE_NS || 
			       ldns_dname_compare(name, zone_name) == 0) {
		
		print_rr_error(myerr, rrset->rrs->rr, "no signatures");
		status = LDNS_STATUS_CRYPTO_NO_RRSIG;
	} else {
		status = LDNS_STATUS_OK;
	}
	ldns_rr_list_free(rrset_rrs);

	return status;
}

static ldns_status
verify_single_rr(ldns_rr *rr, ldns_dnssec_rrs *signature_rrs,
		ldns_rr_list *keys)
{
	ldns_rr_list *rrset_rrs;
	ldns_status status;

	rrset_rrs = ldns_rr_list_new();
	ldns_rr_list_push_rr(rrset_rrs, rr);

	status = verify_rrs(rrset_rrs, signature_rrs, keys);

	ldns_rr_list_free(rrset_rrs);

	return status;
}

static ldns_status
verify_next_hashed_name(ldns_dnssec_zone* zone, ldns_dnssec_name *name)
{
	ldns_rbnode_t *next_node;
	ldns_dnssec_name *next_name;
	ldns_dnssec_name *cur_next_name = NULL;
	ldns_dnssec_name *cur_first_name = NULL;
	int cmp;
	char *next_owner_str;
	ldns_rdf *next_owner_dname;

	if (!name->hashed_name) {
		name->hashed_name = ldns_nsec3_hash_name_frm_nsec3(
				name->nsec, name->name);
	}
	next_node = ldns_rbtree_first(zone->names);
	while (next_node != LDNS_RBTREE_NULL) {
		next_name = (ldns_dnssec_name *)next_node->data;
		/* skip over names that have no NSEC3 records (whether it
		 * actually should or should not should have been checked
		 * already */
		if (!next_name->nsec) {
			next_node = ldns_rbtree_next(next_node);
			continue;
		}
		if (!next_name->hashed_name) {
			next_name->hashed_name =
				ldns_nsec3_hash_name_frm_nsec3(name->nsec,
						next_name->name);
		}
		/* we keep track of what 'so far' is the next hashed name;
		 * it must of course be 'larger' than the current name
		 * if we find one that is larger, but smaller than what we
		 * previously thought was the next one, that one is the next
		 */
		cmp = ldns_dname_compare(name->hashed_name,
				next_name->hashed_name);
		if (cmp < 0) {
			if (!cur_next_name) {
				cur_next_name = next_name;
			} else {
				cmp = ldns_dname_compare(
						next_name->hashed_name,
						cur_next_name->hashed_name);
				if (cmp < 0) {
					cur_next_name = next_name;
				}
			}
		}
		/* in case the hashed name of the nsec we are checking is the
		 * last one, we need the first hashed name of the zone */
		if (!cur_first_name) {
			cur_first_name = next_name;
		} else {
			cmp = ldns_dname_compare(next_name->hashed_name,
					cur_first_name->hashed_name);
		       	if (cmp < 0) {
				cur_first_name = next_name;
			}
		}
		next_node = ldns_rbtree_next(next_node);
	}
	if (!cur_next_name) {
		cur_next_name = cur_first_name;
	}

	next_owner_str = ldns_rdf2str(ldns_nsec3_next_owner(name->nsec));
	next_owner_dname = ldns_dname_new_frm_str(next_owner_str);
	cmp = ldns_dname_compare(next_owner_dname, cur_next_name->hashed_name);
	ldns_rdf_deep_free(next_owner_dname);
	LDNS_FREE(next_owner_str);
	if (cmp != 0) {
		if (verbosity > 0) {
			fprintf(myerr, "Error: The NSEC3 record for ");
			ldns_rdf_print(stdout, name->name);
			fprintf(myerr, " points to the wrong next hashed owner"
					" name\n\tshould point to ");
			ldns_rdf_print(myerr, cur_next_name->name);
			fprintf(myerr, ", whose hashed name is ");
			ldns_rdf_print(myerr, cur_next_name->hashed_name);
			fprintf(myerr, "\n");
		}
		return LDNS_STATUS_ERR;
	} else {
		return LDNS_STATUS_OK;
	}
}

static bool zone_is_nsec3_optout(ldns_dnssec_zone* zone)
{
	static int remember = -1;
	
	if (remember == -1) {
		remember = ldns_dnssec_zone_is_nsec3_optout(zone) ? 1 : 0;
	}
	return remember == 1;
}

static ldns_status
verify_nsec(ldns_dnssec_zone* zone, ldns_rbnode_t *cur_node,
		ldns_rr_list *keys)
{
	ldns_rbnode_t *next_node;
	ldns_dnssec_name *name, *next_name;
	ldns_status status, result;
	result = LDNS_STATUS_OK;

	name = (ldns_dnssec_name *) cur_node->data;
	if (name->nsec) {
		if (name->nsec_signatures) {
			status = verify_single_rr(name->nsec,
					name->nsec_signatures, keys);

			update_error(&result, status);
		} else {
			if (verbosity > 0) {
				fprintf(myerr,
					"Error: the NSEC(3) record of ");
				ldns_rdf_print(myerr, name->name);
				fprintf(myerr, " has no signatures\n");
			}
			update_error(&result, LDNS_STATUS_ERR);
		}
		/* check whether the NSEC record points to the right name */
		switch (ldns_rr_get_type(name->nsec)) {
			case LDNS_RR_TYPE_NSEC:
				/* simply try next name */
				next_node = ldns_rbtree_next(cur_node);
				if (next_node == LDNS_RBTREE_NULL) {
					next_node = ldns_rbtree_first(
							zone->names);
				}
				next_node = ldns_dnssec_name_node_next_nonglue(
						next_node);
				if (!next_node) {
					next_node =
					    ldns_dnssec_name_node_next_nonglue(
						ldns_rbtree_first(zone->names));
				}
				next_name = (ldns_dnssec_name*)next_node->data;
				if (ldns_dname_compare(next_name->name,
							ldns_rr_rdf(name->nsec,
								0)) != 0) {
					if (verbosity > 0) {
						fprintf(myerr, "Error: the "
							"NSEC record for ");
						ldns_rdf_print(myerr,
								name->name);
						fprintf(myerr, " points to "
							"the wrong "
							"next owner name\n");
					}
					if (verbosity >= 4) {
						fprintf(myerr, "\t: ");
						ldns_rdf_print(myerr,
							ldns_rr_rdf(
								name->nsec,
								0));
						fprintf(myerr, " i.s.o. ");
						ldns_rdf_print(myerr,
							next_name->name);
						fprintf(myerr, ".\n");
					}
					update_error(&result,
							LDNS_STATUS_ERR);
				}
				break;
			case LDNS_RR_TYPE_NSEC3:
				/* find the hashed next name in the tree */
				/* this is expensive, do we need to add 
				 * support for this in the structs?
				 * (ie. pointer to next hashed name?)
				 */
				status = verify_next_hashed_name(zone, name);
				update_error(&result, status);
				break;
			default:
				break;
		}
	} else {
		if (zone_is_nsec3_optout(zone) &&
		    (ldns_dnssec_name_is_glue(name) ||
		     (    ldns_dnssec_rrsets_contains_type(name->rrsets,
							   LDNS_RR_TYPE_NS)
		      && !ldns_dnssec_rrsets_contains_type(name->rrsets,
							   LDNS_RR_TYPE_DS)))) {
			/* ok, no problem, but we need to remember to check
			 * whether the chain does not actually point to this
			 * name later */
		} else {
			if (verbosity > 0) {
				fprintf(myerr,
					"Error: there is no NSEC(3) for ");
				ldns_rdf_print(myerr, name->name);
				fprintf(myerr, "\n");
			}
			update_error(&result, LDNS_STATUS_ERR);
		}
	}
	return result;
}

static ldns_status
verify_dnssec_name(ldns_rdf *zone_name, ldns_dnssec_zone* zone,
		ldns_rbnode_t *cur_node, ldns_rr_list *keys)
{
	ldns_status result = LDNS_STATUS_OK;
	ldns_status status;
	ldns_dnssec_rrsets *cur_rrset;
	ldns_dnssec_name *name;
	int on_delegation_point;
	/* for NSEC chain checks */

	name = (ldns_dnssec_name *) cur_node->data;
	if (verbosity >= 3) {
		fprintf(myout, "Checking: ");
		ldns_rdf_print(myout, name->name);
		fprintf(myout, "\n");
	}

	if (ldns_dnssec_name_is_glue(name)) {
		/* glue */
		cur_rrset = name->rrsets;
		while (cur_rrset) {
			if (cur_rrset->signatures) {
				if (verbosity > 0) {
					fprintf(myerr, "Error: ");
					ldns_rdf_print(myerr, name->name);
					fprintf(myerr, "\t");
					print_type(myerr, cur_rrset->type);
					fprintf(myerr, " has signature(s),"
							" but is glue\n");
				}
				result = LDNS_STATUS_ERR;
			}
			cur_rrset = cur_rrset->next;
		}
		if (name->nsec) {
			if (verbosity > 0) {
				fprintf(myerr, "Error: ");
				ldns_rdf_print(myerr, name->name);
				fprintf(myerr, " has an NSEC(3),"
						" but is glue\n");
			}
			result = LDNS_STATUS_ERR;
		}
	} else {
		/* not glue, do real verify */

		on_delegation_point =
			    ldns_dnssec_rrsets_contains_type(name->rrsets,
					LDNS_RR_TYPE_NS)
			&& !ldns_dnssec_rrsets_contains_type(name->rrsets,
					LDNS_RR_TYPE_SOA);
		cur_rrset = name->rrsets;
		while(cur_rrset) {

			/* Do not check occluded rrsets
			 * on the delegation point
			 */
			if ((on_delegation_point && 
			     (cur_rrset->type == LDNS_RR_TYPE_NS ||
			      cur_rrset->type == LDNS_RR_TYPE_DS)) ||
			    (!on_delegation_point &&
			     cur_rrset->type != LDNS_RR_TYPE_RRSIG &&
			     cur_rrset->type != LDNS_RR_TYPE_NSEC)) {

				status = verify_dnssec_rrset(zone_name,
						name->name, cur_rrset, keys);
				update_error(&result, status);
			}
			cur_rrset = cur_rrset->next;
		}
		status = verify_nsec(zone, cur_node, keys);
		update_error(&result, status);
	}
	return result;
}

static void
add_keys_with_matching_ds(ldns_dnssec_rrsets* from_keys, ldns_rr_list *dss,
		ldns_rr_list *to_keys)
{
	size_t i;
	ldns_rr* ds_rr;
	ldns_dnssec_rrs *cur_key;

	for (i = 0; i < ldns_rr_list_rr_count(dss); i++) {

		if (ldns_rr_get_type(ds_rr = ldns_rr_list_rr(dss, i)) 
				== LDNS_RR_TYPE_DS) {

			for (cur_key = from_keys->rrs; cur_key;
					cur_key = cur_key->next ) {

				if (ldns_rr_compare_ds(cur_key->rr, ds_rr)) {
					ldns_rr_list_push_rr(to_keys,
							cur_key->rr);
					break;
				}
			}
		}
	}
}

static ldns_status
sigchase(ldns_resolver* res, ldns_rdf *zone_name, ldns_dnssec_rrsets *zonekeys,
		ldns_rr_list *keys)
{
	ldns_dnssec_rrs* cur_key;
	ldns_status status;
	bool free_resolver = false;
	ldns_rdf* parent_name;
	ldns_rr_list* parent_keys;
	ldns_rr_list* ds_keys;

	add_keys_with_matching_ds(zonekeys, keys, keys);

	/* First try to authenticate the keys offline.
	 * When do_sigchase is given validation may continue lookup up
	 * keys online. Reporting the failure of the offline validation
	 * should then be suppressed.
	 */
	no_nomatch_msg = do_sigchase;
	status = verify_dnssec_rrset(zone_name, zone_name, zonekeys, keys);
	no_nomatch_msg = false;

	/* Continue online on validation failure when the -S option was given.
	 */
	if (do_sigchase && 
	    status == LDNS_STATUS_CRYPTO_NO_MATCHING_KEYTAG_DNSKEY &&
	    ldns_dname_label_count(zone_name) > 0 ) {

		if (!res) {
			if ((status = ldns_resolver_new_frm_file(&res, NULL))){
				ldns_resolver_free(res);
				if (verbosity > 0) {
					fprintf(myerr,
						"Could not create resolver: "
						"%s\n",
						ldns_get_errorstr_by_id(status)
						);
				}
				return status;
			}
			free_resolver = true;
			ldns_resolver_set_dnssec(res,1);
			ldns_resolver_set_dnssec_cd(res, 1);
		}
		if ((parent_name = ldns_dname_left_chop(zone_name))) {
			/*
			 * Use the (authenticated) keys of the parent zone ...
			 */
			parent_keys = ldns_fetch_valid_domain_keys(res,
					parent_name, keys, &status);
			ldns_rdf_deep_free(parent_name);

			/*
			 * ... to validate the DS for the zone ...
			 */
			ds_keys = ldns_validate_domain_ds(res, zone_name,
					parent_keys);
			ldns_rr_list_free(parent_keys);

			/*
			 * ... to use it to add the KSK to the trusted keys ...
			 */
			add_keys_with_matching_ds(zonekeys, ds_keys, keys);
			ldns_rr_list_free(ds_keys);

			/*
			 * ... to validate all zonekeys ...
			 */
			status = verify_dnssec_rrset(zone_name, zone_name,
					zonekeys, keys);
		} else {
			status = LDNS_STATUS_MEM_ERR;
		}
		if (free_resolver) {
			ldns_resolver_deep_free(res);
		}

	}
	/*
	 * ... so they can all be added to our list of trusted keys.
	 */
	if (status == LDNS_STATUS_OK)
		for (cur_key = zonekeys->rrs; cur_key; cur_key = cur_key->next)
			ldns_rr_list_push_rr(keys, cur_key->rr);
	return status;
}

static ldns_status
verify_dnssec_zone(ldns_dnssec_zone *dnssec_zone, ldns_rdf *zone_name,
		ldns_rr_list *keys, bool apexonly, int percentage) 
{
	ldns_rbnode_t *cur_node;
	ldns_dnssec_rrsets *cur_key_rrset;
	ldns_dnssec_rrs *cur_key;
	ldns_status status;
	ldns_status result = LDNS_STATUS_OK;

	cur_key_rrset = ldns_dnssec_zone_find_rrset(dnssec_zone, zone_name,
			LDNS_RR_TYPE_DNSKEY);
	if (!cur_key_rrset || !cur_key_rrset->rrs) {
		if (verbosity > 0) {
			fprintf(myerr,
				"Error: No DNSKEY records at zone apex\n");
		}
		result = LDNS_STATUS_ERR;
	} else {
		/* are keys given with -k to use for validation? */
		if (ldns_rr_list_rr_count(keys) > 0) {
			if ((result = sigchase(NULL, zone_name, cur_key_rrset,
							keys)))
				goto error;
		} else
			for (cur_key = cur_key_rrset->rrs; cur_key;
					cur_key = cur_key->next) 
				ldns_rr_list_push_rr(keys, cur_key->rr);

		cur_node = ldns_rbtree_first(dnssec_zone->names);
		if (cur_node == LDNS_RBTREE_NULL) {
			if (verbosity > 0) {
				fprintf(myerr, "Error: Empty zone?\n");
			}
			result = LDNS_STATUS_ERR;
		}
		if (apexonly) {
			/*
			 * In this case, only the first node in the treewalk
			 * below should be checked.
			 */
			assert( cur_node->data == dnssec_zone->soa );
			/* 
			 * Allthough the percentage option doesn't make sense
			 * here, we set it to 100 to force the first node to 
			 * be checked.
			 */
			percentage = 100;
		}
		while (cur_node != LDNS_RBTREE_NULL) {
			/* should we check this one? saves calls to random. */
			if (percentage == 100 
			    || ((random() % 100) >= 100 - percentage)) {
				status = verify_dnssec_name(zone_name,
						dnssec_zone, cur_node, keys);
				update_error(&result, status);
				if (apexonly)
					break;
			}
			cur_node = ldns_rbtree_next(cur_node);
		}
	}
error:
	ldns_rr_list_free(keys);
	return result;
}

int
main(int argc, char **argv)
{
	char *filename;
	FILE *fp;
	int line_nr = 0;
	int c;
	ldns_status s;
	ldns_dnssec_zone *dnssec_zone = NULL;
	ldns_status result = LDNS_STATUS_ERR;
	bool apexonly = false;
	int percentage = 100;
	struct tm tm;
	ldns_duration_type *duration;
	ldns_rr_list *keys = ldns_rr_list_new();
	size_t nkeys = 0;

	check_time = ldns_time(NULL);
	myout = stdout;
	myerr = stderr;

	while ((c = getopt(argc, argv, "ae:hi:k:vV:p:St:")) != -1) {
		switch(c) {
                case 'a':
                        apexonly = true;
                        break;
		case 'h':
			printf("Usage: %s [OPTIONS] <zonefile>\n", argv[0]);
			printf("\tReads the zonefile and checks for DNSSEC "
			       "errors.\n");
			printf("\nIt checks whether NSEC(3)s are present, "
			       "and verifies all signatures\n");
			printf("It also checks the NSEC(3) chain, but it "
			       "will error on opted-out delegations\n");
			printf("\nOPTIONS:\n");
			printf("\t-h\t\tshow this text\n");
			printf("\t-a\t\tapex only, "
			       "check only the zone apex\n");
			printf("\t-e <period>\tsignatures may not expire "
			       "within this period.\n\t\t\t"
			       "(default no period is used)\n");
			printf("\t-i <period>\tsignatures must have been "
			       "valid at least this long.\n\t\t\t"
			       "(default signatures should just be valid "
			       "now)\n");
			printf("\t-k <file>\tspecify a file that contains a "
			       "trusted DNSKEY or DS rr.\n\t\t\t"
			       "This option may be given more than once.\n");
			printf("\t-p [0-100]\tonly checks this percentage of "
			       "the zone.\n\t\t\tDefaults to 100\n");
			printf("\t-S\t\tchase signature(s) to a known key. "
			       "The network may be\n\t\t\taccessed to "
			       "validate the zone's DNSKEYs. (implies -k)\n");
			printf("\t-t YYYYMMDDhhmmss | [+|-]offset\n\t\t\t"
			       "set the validation time either by an "
			       "absolute time\n\t\t\tvalue or as an "
			       "offset in seconds from <now>.\n\t\t\t"
			       "For data that came from the network (while "
			       "chasing),\n\t\t\tsystem time will be used "
			       "for validating it regardless.\n");
			printf("\t-v\t\tshows the version and exits\n");
			printf("\t-V [0-5]\tset verbosity level (default 3)\n"
			      );
			printf("\n<period>s are given "
			       "in ISO 8601 duration format: "
			       "P[n]Y[n]M[n]DT[n]H[n]M[n]S\n");
			printf("\nif no file is given "
			       "standard input is read\n");
			exit(EXIT_SUCCESS);
			break;
		case 'e':
		case 'i':
			duration = ldns_duration_create_from_string(optarg);
			if (!duration) {
				if (verbosity > 0) {
					fprintf(myerr,
						"<period> should be in ISO "
						"8601 duration format: "
						"P[n]Y[n]M[n]DT[n]H[n]M[n]S\n"
						);
				}
                                exit(EXIT_FAILURE);
			}
			if (c == 'e')
				expiration_offset =
					ldns_duration2time(duration);
			else
				inception_offset =
					ldns_duration2time(duration);
			break;
		case 'k':
			s = read_key_file(optarg, keys);
			if (s != LDNS_STATUS_OK) {
				if (verbosity > 0) {
					fprintf(myerr,
						"Could not parse key file "
						"%s: %s\n",optarg,
						ldns_get_errorstr_by_id(s));
				}
                                exit(EXIT_FAILURE);
			}
			if (ldns_rr_list_rr_count(keys) == nkeys) {
				if (verbosity > 0) {
					fprintf(myerr,
						"No keys found in file %s\n",
						optarg);
				}
				exit(EXIT_FAILURE);
			}
			nkeys = ldns_rr_list_rr_count(keys);
			break;
                case 'p':
                        percentage = atoi(optarg);
                        if (percentage < 0 || percentage > 100) {
				if (verbosity > 0) {
	                        	fprintf(myerr,
						"percentage needs to fall "
						"between 0..100\n");
				}
                                exit(EXIT_FAILURE);
                        }
                        srandom(time(NULL) ^ getpid());
                        break;
		case 'S':
			do_sigchase = true;
			/* may chase */
			break;
		case 't':
			if (strlen(optarg) == 14 &&
			    sscanf(optarg, "%4d%2d%2d%2d%2d%2d",
				    &tm.tm_year, &tm.tm_mon,
				    &tm.tm_mday, &tm.tm_hour,
				    &tm.tm_min , &tm.tm_sec ) == 6) {

				tm.tm_year -= 1900;
				tm.tm_mon--;
				check_time = mktime_from_utc(&tm);
			}
			else  {
				check_time += atoi(optarg);
			}
			break;
		case 'v':
			printf("verify-zone version %s (ldns version %s)\n",
					LDNS_VERSION, ldns_version());
			exit(EXIT_SUCCESS);
			break;
		case 'V':
			verbosity = atoi(optarg);
			break;
		}
	}
	if (do_sigchase && nkeys == 0) {
		if (verbosity > 0) {
			fprintf(myerr,
				"Unable to chase signature without keys.\n");
		}
		exit(EXIT_FAILURE);
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		fp = stdin;
	} else {
		filename = argv[0];

		fp = fopen(filename, "r");
		if (!fp) {
			if (verbosity > 0) {
				fprintf(myerr, "Unable to open %s: %s\n",
					filename, strerror(errno));
			}
			exit(EXIT_FAILURE);
		}
	}

	s = ldns_dnssec_zone_new_frm_fp_l(&dnssec_zone, fp, NULL, 0,
			LDNS_RR_CLASS_IN, &line_nr);
	if (s == LDNS_STATUS_OK) {
		if (!dnssec_zone->soa) {
			if (verbosity > 0) {
				fprintf(myerr,
					"; Error: no SOA in the zone\n");
			}
			exit(EXIT_FAILURE);
		}

		result = ldns_dnssec_zone_mark_glue(dnssec_zone);
		if (result != LDNS_STATUS_OK) {
			if (verbosity > 0) {
				fprintf(myerr,
					"There were errors identifying the "
					"glue in the zone\n");
			}
		}

		if (verbosity >= 5) {
			ldns_dnssec_zone_print(myout, dnssec_zone);
		}

		result = verify_dnssec_zone(dnssec_zone,
				dnssec_zone->soa->name, keys, apexonly,
				percentage);

		if (result == LDNS_STATUS_OK) {
			if (verbosity >= 3) {
				fprintf(myout,
					"Zone is verified and complete\n");
			}
		} else {
			if (verbosity > 0) {
				fprintf(myerr,
					"There were errors in the zone\n");
			}
		}

		ldns_dnssec_zone_deep_free(dnssec_zone);
	} else {
		if (verbosity > 0) {
			fprintf(myerr, "%s at %d\n",
				ldns_get_errorstr_by_id(s), line_nr);
		}
                exit(EXIT_FAILURE);
	}
	fclose(fp);

	exit(result);
}

#else

int
main(int argc, char **argv)
{
	fprintf(stderr, "ldns-verify-zone needs OpenSSL support, "
			"which has not been compiled in\n");
	return 1;
}
#endif /* HAVE_SSL */

