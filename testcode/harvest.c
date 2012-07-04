/*
 * testcode/harvest.c - debug program to get relevant data to a set of queries.
 *
 * Copyright (c) 2008, NLnet Labs. All rights reserved.
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
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *
 * This program downloads relevant DNS data to a set of queries.
 * This means that the queries are asked to root, TLD, SLD servers and
 * the results stored per zone.
 * The following data is pertinent:
 *
 * At each label:
 *	SOA
 *	NS
 *	DNSKEY
 *	DS
 * For the whole query:
 *	the result.
 * For NS-records:
 *	their label data
 *	and the A and AAAA records for it.
 *	(as if the name, with A and AAAA query type is in the list,
 *	 referred to as recursion depth+1)
 * Any NSEC, NSEC3, SOA records or additional data found in answers.
 *
 * All of this is data that would be encountered during an iterative lookup
 * for the queries in the list. It is saved to enable a replay of iterative
 * lookups for performance testing.
 *
 * A number of assumptions are made.
 * 1) configuration is correct.
 *    The parent has the same NS records as the child.
 *    All nameservers carry the same data.
 * 2) EDNS/nonEDNS responses and other behaviour is ignored.
 *    Only the data is saved.
 * This creates a snapshot that represents the data as this resolver saw it.
 */

#include "config.h"
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <ldns/ldns.h>
#include <signal.h>
#include "libunbound/unbound.h"
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef UNBOUND_ALLOC_LITE
#undef malloc
#undef calloc
#undef realloc
#undef free
#undef strdup
#define unbound_lite_wrapstr(s) s
#endif
struct todo_item;
struct labdata;

/** this represents the data that has been collected 
 * as well as a todo list and some settings */
struct harvest_data {
	/** the unbound context */
	struct ub_ctx* ctx;

	/** a tree per label; thus this first one is one root entry,
	 * that has a tree of TLD labels. Those have trees of SLD labels. */
	struct labdata* root;
	/** the original query list */
	struct todo_item* orig_list;
	/** the query list todo */
	struct todo_item* todo_list;
	/** last item in todo list */
	struct todo_item* todo_last;
	/** number of todo items */
	int numtodo;

	/** where to store the results */
	char* resultdir;
	/** maximum recursion depth */
	int maxdepth;
	/** current recursion depth */
	int curdepth;

	/** max depth of labels */
	int maxlabels;
	/** number of RRs stored */
	int num_rrs;
	/** number of zones written */
	int num_zones;
};

/**
 * Todo item
 */
struct todo_item {
	/** the next item */
	struct todo_item* next;

	/** query as rdf */
	ldns_rdf* qname;
	/** the query type */
	int qtype;
	/** query class */
	int qclass;

	/** recursion depth of todo item (orig list is 0) */
	int depth;
	/** the label associated with the query */
	struct labdata* lab;
};

/** 
 * Every label has a sest of sublabels, that have sets of sublabels ...
 * Per label is stored also a set of data items, and todo information
 */
struct labdata {
	/** node in ldns rbtree */
	ldns_rbnode_t node;
	/** the name of this label */
	ldns_rdf* label;
	/** full name of point in domain tree */
	ldns_rdf* name;

	/** parent in label tree (NULL for root) */
	struct labdata* parent;
	/** tree of sublabels (if any) */
	ldns_rbtree_t* sublabels;

	/** list of RRs for this label */
	ldns_rr_list* rrlist;
	/** have queries for this label been queued */
	int done;
};

/** usage information for harvest */
static void usage(char* nm) 
{
	printf("usage: %s [options]\n", nm);
	printf("-f fnm	query list to read from file\n");
	printf("	every line has format: qname qclass qtype\n");
	printf("-v 	verbose (-v -v even more)\n");
	printf("-C cfg	config file with resolver options\n");
	exit(1);
}

/** verbosity for harvest */
static int hverb = 0;

/** exit with error */
static void error_exit(const char* str)
{
	printf("error: %s\n", str);
	exit(1);
}

/** read a query file */
static void
qlist_read_file(struct harvest_data* data, char* fname)
{
	char buf[1024];
	char nm[1024], cl[1024], tp[1024];
	int r;
	int num = 0;
	FILE* in = fopen(fname, "r");
	struct todo_item* t;
	if(!in) {
		perror(fname);
		error_exit("could not open file");
	}
	while(fgets(buf, (int)sizeof(buf), in)) {
		if(buf[0] == 0) continue;
		if(buf[0] == '\n') continue;
		/* allow some comments */
		if(buf[0] == ';') continue;
		if(buf[0] == '#') continue;
		nm[0] = 0; cl[0] = 0; tp[0] = 0;
		r = sscanf(buf, " %1023s %1023s %1023s", nm, cl, tp);
		if(r == 0) continue;
		t = (struct todo_item*)calloc(1, sizeof(*t));
		if(!t) error_exit("out of memory");
		t->qname = ldns_dname_new_frm_str(nm);
		if(!t->qname) {
			printf("parse error: %s\n", nm);
			error_exit("bad qname");
		}
		t->depth = 0;
		t->qtype = LDNS_RR_TYPE_A;
		t->qclass = LDNS_RR_CLASS_IN;
		if(r >= 2) {
			if(strcmp(cl, "IN") == 0 || strcmp(cl, "CH") == 0)
				t->qclass = ldns_get_rr_class_by_name(cl);
			else	t->qtype = ldns_get_rr_type_by_name(cl);
		}
		if(r >= 3) {
			if(strcmp(tp, "IN") == 0 || strcmp(tp, "CH") == 0)
				t->qclass = ldns_get_rr_class_by_name(tp);
			else	t->qtype = ldns_get_rr_type_by_name(tp);
		}
		num++;

		t->next = data->orig_list;
		data->orig_list = t;
	}
	printf("read %s: %d queries\n", fname, num);
	fclose(in);
}

/** compare two labels */
static int
lab_cmp(const void *x, const void *y)
{
	return ldns_dname_compare((const ldns_rdf*)x, (const ldns_rdf*)y);
}

/** create label entry */
static struct labdata*
lab_create(const char* name)
{
	struct labdata* lab = (struct labdata*)calloc(1, sizeof(*lab));
	if(!lab) error_exit("out of memory");
	lab->label = ldns_dname_new_frm_str(name);
	if(!lab->label) error_exit("out of memory");
	lab->name = ldns_dname_new_frm_str(name);
	if(!lab->name) error_exit("out of memory");
	lab->node.key = lab->label;
	lab->node.data = lab;
	lab->sublabels = ldns_rbtree_create(lab_cmp);
	if(!lab->sublabels) error_exit("out of memory");
	lab->rrlist = ldns_rr_list_new();
	if(!lab->rrlist) error_exit("out of memory");

	return lab;
}

/** for this name, lookup the label, create if does not exist */
static struct labdata*
find_create_lab(struct harvest_data* data, ldns_rdf* name)
{
	struct labdata* lab = data->root;
	struct labdata* nextlab;
	ldns_rdf* next;
	uint8_t numlab = ldns_dname_label_count(name);
	if((int)numlab > data->maxlabels)
		data->maxlabels = (int)numlab;
	while(numlab--) {
		next = ldns_dname_label(name, numlab);
		if(!next) error_exit("ldns_dname_label");
		
		nextlab = (struct labdata*)
			ldns_rbtree_search(lab->sublabels, next);
		if(!nextlab) {
			/* create it */
			nextlab = (struct labdata*)calloc(1, sizeof(*lab));
			if(!nextlab) error_exit("out of memory");
			nextlab->label = ldns_rdf_clone(next);
			if(!nextlab->label) error_exit("out of memory");
			nextlab->node.key = nextlab->label;
			nextlab->node.data = nextlab;
			nextlab->sublabels = ldns_rbtree_create(lab_cmp);
			if(!nextlab->sublabels) error_exit("out of memory");
			nextlab->parent = lab;
			nextlab->name = ldns_rdf_clone(next);
			if(!nextlab->name) error_exit("out of memory");
			if(ldns_dname_cat(nextlab->name, lab->name) 
				!= LDNS_STATUS_OK) error_exit("outofmem");
			nextlab->rrlist = ldns_rr_list_new();
			if(!nextlab->rrlist) error_exit("out of memory");
			(void)ldns_rbtree_insert(lab->sublabels, 
				&nextlab->node);
			if(hverb) {
				printf("new label: ");
				ldns_rdf_print(stdout, nextlab->name);
				printf("\n");
			}
		}
		lab = nextlab;
		ldns_rdf_deep_free(next);
	}
	return lab;
}

/** for given query, create todo items, and labels if needed */
static void
new_todo_item(struct harvest_data* data, ldns_rdf* qname, int qtype, 
	int qclass, int depth)
{
	struct labdata* lab = find_create_lab(data, qname);
	struct todo_item* it;
	if(!lab) error_exit("out of memory creating new label");
	it = (struct todo_item*)calloc(1, sizeof(*it));
	it->qname = ldns_rdf_clone(qname);
	it->qtype = qtype;
	it->qclass = qclass;
	it->depth = depth;
	it->lab = lab;
	it->next = NULL;
	if(data->todo_last)
		data->todo_last->next = it;
	else data->todo_list = it;
	data->todo_last = it;
	data->numtodo ++;
	if(hverb >= 2) {
		printf("new todo: ");
		ldns_rdf_print(stdout, it->qname);
		if(ldns_rr_descript((uint16_t)it->qtype) && 
			ldns_rr_descript((uint16_t)it->qtype)->_name)
			printf(" %s", ldns_rr_descript((uint16_t)
			it->qtype)->_name);
		if(ldns_lookup_by_id(ldns_rr_classes, it->qclass) && 
			ldns_lookup_by_id(ldns_rr_classes, it->qclass)->name) 
			printf(" %s", ldns_lookup_by_id(ldns_rr_classes, 
				it->qclass)->name);
		printf("\n");
	}
}

/** add infra todo items for this query */
static void
new_todo_infra(struct harvest_data* data, struct labdata* startlab, int depth)
{
	struct labdata* lab;
	for(lab = startlab; lab; lab = lab->parent) {
		if(lab->done)
			return;
		new_todo_item(data, lab->name, LDNS_RR_TYPE_NS, 
			LDNS_RR_CLASS_IN, depth);
		new_todo_item(data, lab->name, LDNS_RR_TYPE_SOA, 
			LDNS_RR_CLASS_IN, depth);
		new_todo_item(data, lab->name, LDNS_RR_TYPE_DNSKEY, 
			LDNS_RR_CLASS_IN, depth);
		new_todo_item(data, lab->name, LDNS_RR_TYPE_DS, 
			LDNS_RR_CLASS_IN, depth);
		new_todo_item(data, lab->name, LDNS_RR_TYPE_A, 
			LDNS_RR_CLASS_IN, depth);
		new_todo_item(data, lab->name, LDNS_RR_TYPE_AAAA, 
			LDNS_RR_CLASS_IN, depth);
		lab->done = 1;
	}
}

/** make todo items for initial data */
static void
make_todo(struct harvest_data* data)
{
	struct todo_item* it;
	for(it=data->orig_list; it; it = it->next) {
		/* create todo item for this query itself */
		new_todo_item(data, it->qname, it->qtype, it->qclass, 0);
		/* create todo items for infra queries to support it */
		new_todo_infra(data, data->todo_list->lab, 
			data->todo_list->depth);
	}
}

/** store RR and make new work items for it if needed */
static void
process_rr(struct harvest_data* data, ldns_rr* rr, int depth)
{
	/* must free or store rr */
	struct labdata* lab = find_create_lab(data, ldns_rr_owner(rr));
	if(!lab) error_exit("cannot find/create label");
	/* generate extra queries */
	if(ldns_rr_get_type(rr) == LDNS_RR_TYPE_NS) {
		new_todo_infra(data, find_create_lab(data, 
			ldns_rr_ns_nsdname(rr)), depth+1);
	} else if(ldns_rr_get_type(rr) == LDNS_RR_TYPE_MX) {
		new_todo_infra(data, find_create_lab(data, 
			ldns_rr_mx_exchange(rr)), depth+1);
	} else if(ldns_rr_get_type(rr) == LDNS_RR_TYPE_SOA) {
		new_todo_infra(data, find_create_lab(data, 
			ldns_rr_rdf(rr, 0)), depth+1);
	} else if(ldns_rr_get_type(rr) == LDNS_RR_TYPE_CNAME) {
		int t = ldns_rr_get_type(rr);
		if(t!=LDNS_RR_TYPE_A && t!=LDNS_RR_TYPE_AAAA &&
			t!=LDNS_RR_TYPE_SOA && t!=LDNS_RR_TYPE_NS &&
			t!=LDNS_RR_TYPE_DS && t!=LDNS_RR_TYPE_DNSKEY)
			new_todo_item(data, ldns_rr_rdf(rr, 0), t,
				ldns_rr_get_class(rr), depth+1);
			/* can get caught in CNAME loop, but depth will
			 * catch that; unbound cache helps too(servfails on
			 * a cname loop) */
		new_todo_infra(data, find_create_lab(data, 
			ldns_rr_rdf(rr, 0)), depth+1);
	}
	/* store it */
	if(ldns_rr_get_type(rr) == LDNS_RR_TYPE_NSEC) {
		/* find correct zone to store NSEC in (for delegation zones) */
		if(ldns_dname_compare(ldns_rr_rdf(rr, 0), ldns_rr_owner(rr))
			== 0) {
			/* store at the single name = apex */
		} else if(!ldns_dname_is_subdomain(ldns_rr_rdf(rr, 0), 
			ldns_rr_owner(rr)) && lab->parent) {
			/* if   owner NSEC subdomain-of-owner then
		 	 * store at owner (owner is apex or empty nonterminal).
		 	 * Otherwise at owner parent. */
			lab = lab->parent;
		}
	} else if(ldns_rr_get_type(rr) == LDNS_RR_TYPE_DS) {
		/* store DSes in parent zone */
		if(lab->parent)
			lab = lab->parent;
	} else if(ldns_rr_get_type(rr) == LDNS_RR_TYPE_NSEC3) {
		/* store NSEC3s one label up at zone apex */
		if(lab->parent)
			lab = lab->parent;
	}
	/* we assume NS set is equal across parent-child border. */

	if(!ldns_rr_list_contains_rr(lab->rrlist, rr)) {
		if(hverb >= 2) {
			printf("store RR ");
			ldns_rr_print(stdout, rr);
			printf("\n");
		}
		if(!ldns_rr_list_push_rr(lab->rrlist, rr))
			error_exit("outofmem ldns_rr_list_push_rr");
		data->num_rrs++;
	} else {
		if(hverb >= 2) {
			printf("duplicate RR ");
			ldns_rr_print(stdout, rr);
			printf("\n");
		}
		ldns_rr_free(rr);
	}
}

/** store RRs and make new work items if needed */
static void
process_pkt(struct harvest_data* data, ldns_pkt* pkt, int depth)
{
	size_t i;
	ldns_rr_list* list;
	list = ldns_pkt_get_section_clone(pkt, LDNS_SECTION_ANY_NOQUESTION);
	if(!list) error_exit("outofmemory");
	for(i=0; i<ldns_rr_list_rr_count(list); i++) {
		process_rr(data, ldns_rr_list_rr(list, i), depth);
	}
	ldns_rr_list_free(list);
}

/** process a todo item */
static void
process(struct harvest_data* data, struct todo_item* it)
{
	int r;
	char* nm;
	struct ub_result* result = NULL;
	ldns_pkt* pkt = NULL;
	ldns_status s;
	if(hverb) {
		printf("process: ");
		ldns_rdf_print(stdout, it->qname);
		if(ldns_rr_descript((uint16_t)it->qtype) && 
			ldns_rr_descript((uint16_t)it->qtype)->_name)
			printf(" %s", ldns_rr_descript((uint16_t)
			it->qtype)->_name);
		if(ldns_lookup_by_id(ldns_rr_classes, it->qclass) && 
			ldns_lookup_by_id(ldns_rr_classes, it->qclass)->name) 
			printf(" %s", ldns_lookup_by_id(ldns_rr_classes, 
				it->qclass)->name);
		printf("\n");
	}
	/* do lookup */
	nm = ldns_rdf2str(it->qname);
	if(!nm) error_exit("ldns_rdf2str");
	r = ub_resolve(data->ctx, nm, it->qtype, it->qclass, &result);
	if(r != 0) {
		printf("ub_resolve(%s, %d, %d): %s\n", nm, it->qtype, 
			it->qclass, ub_strerror(r));
		free(nm);
		return;
	}
	if(result->rcode == LDNS_RCODE_SERVFAIL) {
		free(nm);
		return;
	}
	/* even if result is a negative, try to store resulting SOA/NSEC */

	/* create ldns pkt */
	s = ldns_wire2pkt(&pkt, result->answer_packet, 
		(size_t)result->answer_len);
	if(s != LDNS_STATUS_OK) {
		printf("ldns_wire2pkt failed! %s %d %d %s %d\n", nm, 
			it->qtype, it->qclass, ldns_get_errorstr_by_id(s),
			result->answer_len);
		free(nm);
		return;
	}
	if(hverb >= 2) {
		printf("answer: ");
		ldns_pkt_print(stdout, pkt);
		printf("\n");
	}
	/* process results */
	process_pkt(data, pkt, it->depth);

	ldns_pkt_free(pkt);
	free(nm);
	ub_resolve_free(result);
}

/** perform main harvesting */
static void
harvest_main(struct harvest_data* data)
{
	struct todo_item* it;
	int numdone = 0;
	/* register todo queries for all original queries */
	make_todo(data);
	printf("depth 0: done %d todo %d\n", 0, data->numtodo);
	/* pick up a todo item and process it */
	while(data->todo_list) {
		numdone++;
		it = data->todo_list;
		data->todo_list = it->next;
		if(!data->todo_list) data->todo_last = NULL;
		if(numdone%1000==0 || it->depth > data->curdepth) {
			data->curdepth = it->depth;
			printf("depth %d: done %d todo %d, %d rrs\n", 
				it->depth, numdone, data->numtodo, 
				data->num_rrs);
		}
		if(it->depth >= data->maxdepth) {
			printf("obtained %d rrs to a max of %d labels.\n",
				data->num_rrs, data->maxlabels);
			return;
		}
		data->numtodo--;
		process(data, it);
		usleep(1000000/100);
	}
}

/** create directory if it does not exist */
static void
hv_mkdir(char* dir)
{
#ifdef MKDIR_HAS_ONE_ARG
	if(mkdir(dir) == -1) {
#else
	if(mkdir(dir, 0755) == -1) {
#endif
		if(errno == EEXIST)
			return;
		perror(dir);
		error_exit("mkdir failed");
	}
}


/** see if rrlist contains a SOA record */
static ldns_rr*
has_SOA(ldns_rr_list* list)
{
	size_t i;
	for(i=0; i<ldns_rr_list_rr_count(list); i++) {
		if(ldns_rr_get_type(ldns_rr_list_rr(list, i)) 
			== LDNS_RR_TYPE_SOA)
			return ldns_rr_list_rr(list, i);
	}
	return NULL;
}

/** write moredata for a zone*/
static void
write_moredata(struct harvest_data* data, struct labdata* zone,
	FILE *f, struct labdata* thislab, ldns_rr* nslist)
{
	struct labdata* lab;
	size_t i;
	ldns_rr* ns;
	LDNS_RBTREE_FOR(lab, struct labdata*, thislab->sublabels) {
		if(has_SOA(lab->rrlist)) {
			/* copy only NS glue */
			for(i=0; i<ldns_rr_list_rr_count(lab->rrlist); i++) {
				ns = ldns_rr_list_rr(lab->rrlist, i);
				if(ldns_rr_get_type(ns) == LDNS_RR_TYPE_NS) {
					ldns_rr_print(f, ns);
					if(ldns_dname_is_subdomain(
						ldns_rr_ns_nsdname(ns), 
						lab->name)) {
						ldns_rr_push_rdf(nslist, 
							ldns_rdf_clone(
							ldns_rr_ns_nsdname(ns)));
					}
				}
			}
		} else {
			/* copy all, recurse */
			for(i=0; i<ldns_rr_list_rr_count(lab->rrlist); i++) {
				ldns_rr_print(f, 
					ldns_rr_list_rr(lab->rrlist, i));
			}
			write_moredata(data, zone, f, lab, nslist);
		}
	}
}

/** find and write glue into zone file */
static void
write_glue(struct harvest_data* data, struct labdata* thislab, FILE* f, 
	ldns_rdf* name, int dep)
{
	size_t i;
	struct labdata* lab;
	ldns_rr* rr;
	if(ldns_dname_compare(name, thislab->name) == 0) {
		/* this is it! Did we go outside the zone? */
		if(dep == 0)
			return;
		/* find A and AAAA */
		for(i=0; i<ldns_rr_list_rr_count(thislab->rrlist); i++) {
			rr = ldns_rr_list_rr(thislab->rrlist, i);
			if(ldns_rr_get_type(rr) == LDNS_RR_TYPE_A ||
			   ldns_rr_get_type(rr) == LDNS_RR_TYPE_AAAA) {
				ldns_rr_print(f, rr);
			}
		}
		return;
	}
	/* recurse deeper */
	LDNS_RBTREE_FOR(lab, struct labdata*, thislab->sublabels) {
		if(has_SOA(lab->rrlist)) {
			write_glue(data, lab, f, name, dep+1);
		} else {
			write_glue(data, lab, f, name, dep);
		}
	}
}

/** write zonefile for zone at this apex */
static void
write_zonefile(struct harvest_data* data, int dep, FILE* zlist, 
	struct labdata* apex, ldns_rr* soa)
{
	FILE *f;
	char fname[1024];
	char* zname = ldns_rdf2str(apex->name);
	time_t tm = time(NULL);
	size_t i;
	ldns_rr* nslist;
	if(!zname) error_exit("out of mem ldns_rdf2str");
	if(strcmp(zname, ".") == 0)
		snprintf(fname, sizeof(fname), "l%d/root.zone", dep);
	else	snprintf(fname, sizeof(fname), "l%d/%szone", dep, zname);

	fprintf(zlist, "zone: name: \"%s\" %s%szonefile: \"%s\"\n",
		zname, 
		strlen(zname)/8<1?"\t":"",
		strlen(zname)/8<2?"\t":"",
		fname);

	if(hverb) printf("writing %s\n", fname);
	f = fopen(fname, "w");
	if(!f) {
		perror(fname);
		error_exit("cannot open zone file");
	}
	fprintf(f, "; %s - generated by harvest program.\n", fname);
	fprintf(f, "; zone name %s - this is a partial snapshot of "
		"data relevant to the query list.\n", zname);
	fprintf(f, "; created %u - date %s\n", (unsigned)tm, ctime(&tm));
	ldns_rr_print(f, soa);
	fprintf(f, "\n");
	for(i=0; i<ldns_rr_list_rr_count(apex->rrlist); i++) {
		if(ldns_rr_get_type(ldns_rr_list_rr(apex->rrlist, i))
			== LDNS_RR_TYPE_SOA) continue;
		ldns_rr_print(f, ldns_rr_list_rr(apex->rrlist, i));
	}
	/* search for more data - subdomains inside the zone, NS glue */
	nslist = ldns_rr_new();
	if(!nslist) error_exit("out of memory");
	fprintf(f, "; end of apex, more data follows\n");
	write_moredata(data, apex, f, apex, nslist);

	/* add NS from apex that need glue too */
	for(i=0; i<ldns_rr_list_rr_count(apex->rrlist); i++) {
		if(ldns_rr_get_type(ldns_rr_list_rr(apex->rrlist, i)) !=
			LDNS_RR_TYPE_NS)
			continue;
		/* these are only added again if in a subzone */
		if(ldns_dname_is_subdomain(ldns_rr_ns_nsdname(
			ldns_rr_list_rr(apex->rrlist, i)), apex->name)) {
			ldns_rr_push_rdf(nslist, ldns_rdf_clone(
				ldns_rr_ns_nsdname(ldns_rr_list_rr(
				apex->rrlist, i))));
		}
	}

	fprintf(f, "; glue data follows\n");
	/* lookup and add glue (if not already in zone) */
	for(i=0; i<ldns_rr_rd_count(nslist); i++) {
		write_glue(data, apex, f, ldns_rr_rdf(nslist, i), 0);
	}

	fclose(f);
	ldns_rr_free(nslist);
	free(zname);
}

/** create zones at depth d in label tree */
static void
create_zones(struct harvest_data* data, int dep, FILE* zlist, 
	struct labdata* labnow, int depnow)
{
	struct labdata* s;
	ldns_rr* soa;
	if(depnow == dep) {
		/* see if this is a zone start - a SOA */
		if((soa=has_SOA(labnow->rrlist))) {
			write_zonefile(data, dep, zlist, labnow, soa);
			data->num_zones++;
		}
		return;
	}
	/* recurse */
	LDNS_RBTREE_FOR(s, struct labdata*, labnow->sublabels) {
		create_zones(data, dep, zlist, s, depnow+1);
	}
}

/** sort rrlists */
static void
harvest_sort(struct labdata* lab)
{
	struct labdata* s;
	/* prettier output if sorted here */
	ldns_rr_list_sort(lab->rrlist);
	/* and recurse */
	LDNS_RBTREE_FOR(s, struct labdata*, lab->sublabels) {
		harvest_sort(s);
	}
}

/** output harvested results */
static void
harvest_output(struct harvest_data* data)
{
	int d;
	char buf[20];
	FILE* zlist;
	int lastzones;
	hv_mkdir(data->resultdir);
	if(chdir(data->resultdir) == -1) {
		perror(data->resultdir);
		error_exit("cannot chdir");
	}
	harvest_sort(data->root);
	/* create zones */
	for(d = 0; d<data->maxlabels; d++) {
		lastzones = data->num_zones;
		printf("creating zones %d\n", d);	
		snprintf(buf, sizeof(buf), "l%d", d);
		hv_mkdir(buf);
		snprintf(buf, sizeof(buf), "l%d.zones", d);
		zlist = fopen(buf, "w");
		if(!zlist) {
			perror(buf);
			error_exit("cannot write zonelist file");
		}
		fprintf(zlist, "# partial zones at depth %d\n", d);
		create_zones(data, d, zlist, data->root, 0);
		fclose(zlist);
		printf("creating zones %d - %d zones written\n", d,
			data->num_zones - lastzones);	
	}
}

/** getopt global, in case header files fail to declare it. */
extern int optind;
/** getopt global, in case header files fail to declare it. */
extern char* optarg;

/** main program for harvest */
int main(int argc, char* argv[]) 
{
	struct harvest_data data;
	char* nm = argv[0];
	int c;

	/* defaults */
	memset(&data, 0, sizeof(data));
	data.ctx = ub_ctx_create();
	data.resultdir = strdup("harvested_zones");
	if(!data.resultdir) error_exit("out of memory");
	data.maxdepth = 2;

	/* parse the options */
	while( (c=getopt(argc, argv, "hf:vC:")) != -1) {
		switch(c) {
		case 'C':
			if(ub_ctx_config(data.ctx, optarg) != 0)
				error_exit("config read failed");
			break;
		case 'f':
			qlist_read_file(&data, optarg);
			break;
		case 'v':
			hverb++;
			break;
		case '?':
		case 'h':
		default:
			usage(nm);
		}
	}
	argc -= optind;
	argv += optind;
	if(argc != 0)
		usage(nm);
	if(data.orig_list == NULL)
		error_exit("No queries to make, use -f (help with -h).");
	data.root = lab_create(".");
	if(!data.root) error_exit("out of memory");
	
	/* harvest the data */
	harvest_main(&data);
	harvest_output(&data);

	/* no cleanup except the context (to close open sockets) */
	ub_ctx_delete(data.ctx);
	return 0;
}
