/*
 * validator/autotrust.c - RFC5011 trust anchor management for unbound.
 *
 * Copyright (c) 2009, NLnet Labs. All rights reserved.
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
 * Contains autotrust implementation. The implementation was taken from 
 * the autotrust daemon (BSD licensed), written by Matthijs Mekking.
 * It was modified to fit into unbound. The state table process is the same.
 */
#include "config.h"
#include <ldns/ldns.h>
#include "validator/autotrust.h"
#include "validator/val_anchor.h"
#include "validator/val_utils.h"
#include "validator/val_sigcrypt.h"
#include "util/data/dname.h"
#include "util/data/packed_rrset.h"
#include "util/log.h"
#include "util/module.h"
#include "util/net_help.h"
#include "util/config_file.h"
#include "util/regional.h"
#include "util/random.h"
#include "util/data/msgparse.h"
#include "services/mesh.h"
#include "services/cache/rrset.h"
#include "validator/val_kcache.h"

/** number of times a key must be seen before it can become valid */
#define MIN_PENDINGCOUNT 2

/** Event: Revoked */
static void do_revoked(struct module_env* env, struct autr_ta* anchor, int* c);

struct autr_global_data* autr_global_create(void)
{
	struct autr_global_data* global;
	global = (struct autr_global_data*)malloc(sizeof(*global));
	if(!global) 
		return NULL;
	rbtree_init(&global->probe, &probetree_cmp);
	return global;
}

void autr_global_delete(struct autr_global_data* global)
{
	if(!global)
		return;
	/* elements deleted by parent */
	memset(global, 0, sizeof(*global));
	free(global);
}

int probetree_cmp(const void* x, const void* y)
{
	struct trust_anchor* a = (struct trust_anchor*)x;
	struct trust_anchor* b = (struct trust_anchor*)y;
	log_assert(a->autr && b->autr);
	if(a->autr->next_probe_time < b->autr->next_probe_time)
		return -1;
	if(a->autr->next_probe_time > b->autr->next_probe_time)
		return 1;
	/* time is equal, sort on trust point identity */
	return anchor_cmp(x, y);
}

size_t 
autr_get_num_anchors(struct val_anchors* anchors)
{
	size_t res = 0;
	if(!anchors)
		return 0;
	lock_basic_lock(&anchors->lock);
	if(anchors->autr)
		res = anchors->autr->probe.count;
	lock_basic_unlock(&anchors->lock);
	return res;
}

/** Position in string */
static int
position_in_string(char *str, const char* sub)
{
	char* pos = strstr(str, sub);
	if(pos)
		return (int)(pos-str)+(int)strlen(sub);
	return -1;
}

/** Debug routine to print pretty key information */
static void
verbose_key(struct autr_ta* ta, enum verbosity_value level, 
	const char* format, ...) ATTR_FORMAT(printf, 3, 4);

/** 
 * Implementation of debug pretty key print 
 * @param ta: trust anchor key with DNSKEY data.
 * @param level: verbosity level to print at.
 * @param format: printf style format string.
 */
static void
verbose_key(struct autr_ta* ta, enum verbosity_value level, 
	const char* format, ...) 
{
	va_list args;
	va_start(args, format);
	if(verbosity >= level) {
		char* str = ldns_rdf2str(ldns_rr_owner(ta->rr));
		int keytag = (int)ldns_calc_keytag(ta->rr);
		char msg[MAXSYSLOGMSGLEN];
		vsnprintf(msg, sizeof(msg), format, args);
		verbose(level, "%s key %d %s", str?str:"??", keytag, msg);
		free(str);
	}
	va_end(args);
}

/** 
 * Parse comments 
 * @param str: to parse
 * @param ta: trust key autotrust metadata
 * @return false on failure.
 */
static int
parse_comments(char* str, struct autr_ta* ta)
{
        int len = (int)strlen(str), pos = 0, timestamp = 0;
        char* comment = (char*) malloc(sizeof(char)*len+1);
        char* comments = comment;
	if(!comment) {
		log_err("malloc failure in parse");
                return 0;
	}
	/* skip over whitespace and data at start of line */
        while (*str != '\0' && *str != ';')
                str++;
        if (*str == ';')
                str++;
        /* copy comments */
        while (*str != '\0')
        {
                *comments = *str;
                comments++;
                str++;
        }
        *comments = '\0';

        comments = comment;

        /* read state */
        pos = position_in_string(comments, "state=");
        if (pos >= (int) strlen(comments))
        {
		log_err("parse error");
                free(comment);
                return 0;
        }
        if (pos <= 0)
                ta->s = AUTR_STATE_VALID;
        else
        {
                int s = (int) comments[pos] - '0';
                switch(s)
                {
                        case AUTR_STATE_START:
                        case AUTR_STATE_ADDPEND:
                        case AUTR_STATE_VALID:
                        case AUTR_STATE_MISSING:
                        case AUTR_STATE_REVOKED:
                        case AUTR_STATE_REMOVED:
                                ta->s = s;
                                break;
                        default:
				verbose_key(ta, VERB_OPS, "has undefined "
					"state, considered NewKey");
                                ta->s = AUTR_STATE_START;
                                break;
                }
        }
        /* read pending count */
        pos = position_in_string(comments, "count=");
        if (pos >= (int) strlen(comments))
        {
		log_err("parse error");
                free(comment);
                return 0;
        }
        if (pos <= 0)
                ta->pending_count = 0;
        else
        {
                comments += pos;
                ta->pending_count = (uint8_t)atoi(comments);
        }

        /* read last change */
        pos = position_in_string(comments, "lastchange=");
        if (pos >= (int) strlen(comments))
        {
		log_err("parse error");
                free(comment);
                return 0;
        }
        if (pos >= 0)
        {
                comments += pos;
                timestamp = atoi(comments);
        }
        if (pos < 0 || !timestamp)
		ta->last_change = 0;
        else
                ta->last_change = (uint32_t)timestamp;

        free(comment);
        return 1;
}

/** Check if a line contains data (besides comments) */
static int
str_contains_data(char* str, char comment)
{
        while (*str != '\0') {
                if (*str == comment || *str == '\n')
                        return 0;
                if (*str != ' ' && *str != '\t')
                        return 1;
                str++;
        }
        return 0;
}

/** Get DNSKEY flags */
static int
dnskey_flags(ldns_rr* rr)
{
	if(ldns_rr_get_type(rr) != LDNS_RR_TYPE_DNSKEY)
		return 0;
	return (int)ldns_read_uint16(ldns_rdf_data(ldns_rr_dnskey_flags(rr)));
}


/** Check if KSK DNSKEY */
static int
rr_is_dnskey_sep(ldns_rr* rr)
{
	return (dnskey_flags(rr)&DNSKEY_BIT_SEP);
}

/** Check if REVOKED DNSKEY */
static int
rr_is_dnskey_revoked(ldns_rr* rr)
{
	return (dnskey_flags(rr)&LDNS_KEY_REVOKE_KEY);
}

/** create ta */
static struct autr_ta*
autr_ta_create(ldns_rr* rr)
{
	struct autr_ta* ta = (struct autr_ta*)calloc(1, sizeof(*ta));
	if(!ta) {
		ldns_rr_free(rr);
		return NULL;
	}
	ta->rr = rr;
	return ta;
}

/** create tp */
static struct trust_anchor*
autr_tp_create(struct val_anchors* anchors, ldns_rdf* own, uint16_t dc)
{
	struct trust_anchor* tp = (struct trust_anchor*)calloc(1, sizeof(*tp));
	if(!tp) return NULL;
	tp->name = memdup(ldns_rdf_data(own), ldns_rdf_size(own));
	if(!tp->name) {
		free(tp);
		return NULL;
	}
	tp->namelen = ldns_rdf_size(own);
	tp->namelabs = dname_count_labels(tp->name);
	tp->node.key = tp;
	tp->dclass = dc;
	tp->autr = (struct autr_point_data*)calloc(1, sizeof(*tp->autr));
	if(!tp->autr) {
		free(tp->name);
		free(tp);
		return NULL;
	}
	tp->autr->pnode.key = tp;

	lock_basic_lock(&anchors->lock);
	if(!rbtree_insert(anchors->tree, &tp->node)) {
		lock_basic_unlock(&anchors->lock);
		log_err("trust anchor presented twice");
		free(tp->name);
		free(tp->autr);
		free(tp);
		return NULL;
	}
	if(!rbtree_insert(&anchors->autr->probe, &tp->autr->pnode)) {
		(void)rbtree_delete(anchors->tree, tp);
		lock_basic_unlock(&anchors->lock);
		log_err("trust anchor in probetree twice");
		free(tp->name);
		free(tp->autr);
		free(tp);
		return NULL;
	}
	lock_basic_unlock(&anchors->lock);
	lock_basic_init(&tp->lock);
	lock_protect(&tp->lock, tp, sizeof(*tp));
	lock_protect(&tp->lock, tp->autr, sizeof(*tp->autr));
	return tp;
}

/** delete assembled rrsets */
static void
autr_rrset_delete(struct ub_packed_rrset_key* r)
{
	if(r) {
		free(r->rk.dname);
		free(r->entry.data);
		free(r);
	}
}

void autr_point_delete(struct trust_anchor* tp)
{
	if(!tp)
		return;
	lock_unprotect(&tp->lock, tp);
	lock_unprotect(&tp->lock, tp->autr);
	lock_basic_destroy(&tp->lock);
	autr_rrset_delete(tp->ds_rrset);
	autr_rrset_delete(tp->dnskey_rrset);
	if(tp->autr) {
		struct autr_ta* p = tp->autr->keys, *np;
		while(p) {
			np = p->next;
			ldns_rr_free(p->rr);
			free(p);
			p = np;
		}
		free(tp->autr->file);
		free(tp->autr);
	}
	free(tp->name);
	free(tp);
}

/** find or add a new trust point for autotrust */
static struct trust_anchor*
find_add_tp(struct val_anchors* anchors, ldns_rr* rr)
{
	struct trust_anchor* tp;
	ldns_rdf* own = ldns_rr_owner(rr);
	tp = anchor_find(anchors, ldns_rdf_data(own), 
		dname_count_labels(ldns_rdf_data(own)),
		ldns_rdf_size(own), ldns_rr_get_class(rr));
	if(tp) {
		if(!tp->autr) {
			log_err("anchor cannot be with and without autotrust");
			lock_basic_unlock(&tp->lock);
			return NULL;
		}
		return tp;
	}
	tp = autr_tp_create(anchors, ldns_rr_owner(rr), ldns_rr_get_class(rr));
	lock_basic_lock(&tp->lock);
	return tp;
}

/** Add trust anchor from RR */
static struct autr_ta*
add_trustanchor_frm_rr(struct val_anchors* anchors, ldns_rr* rr, 
	struct trust_anchor** tp)
{
	struct autr_ta* ta = autr_ta_create(rr);
	if(!ta) 
		return NULL;
	*tp = find_add_tp(anchors, rr);
	if(!*tp) {
		ldns_rr_free(ta->rr);
		free(ta);
		return NULL;
	}
	/* add ta to tp */
	ta->next = (*tp)->autr->keys;
	(*tp)->autr->keys = ta;
	lock_basic_unlock(&(*tp)->lock);
	return ta;
}

/**
 * Add new trust anchor from a string in file.
 * @param anchors: all anchors
 * @param str: string with anchor and comments, if any comments.
 * @param tp: trust point returned.
 * @param origin: what to use for @
 * @param prev: previous rr name
 * @param skip: if true, the result is NULL, but not an error, skip it.
 * @return new key in trust point.
 */
static struct autr_ta*
add_trustanchor_frm_str(struct val_anchors* anchors, char* str, 
	struct trust_anchor** tp, ldns_rdf* origin, ldns_rdf** prev, int* skip)
{
        ldns_rr* rr;
	ldns_status lstatus;
        if (!str_contains_data(str, ';')) {
		*skip = 1;
                return NULL; /* empty line */
	}
        if (LDNS_STATUS_OK !=
                (lstatus = ldns_rr_new_frm_str(&rr, str, 0, origin, prev)))
        {
        	log_err("ldns error while converting string to RR: %s",
			ldns_get_errorstr_by_id(lstatus));
                return NULL;
        }
	if(ldns_rr_get_type(rr) != LDNS_RR_TYPE_DNSKEY &&
		ldns_rr_get_type(rr) != LDNS_RR_TYPE_DS) {
		ldns_rr_free(rr);
		*skip = 1;
		return NULL; /* only DS and DNSKEY allowed */
	}
        return add_trustanchor_frm_rr(anchors, rr, tp);
}

/** 
 * Load single anchor 
 * @param anchors: all points.
 * @param str: comments line
 * @param fname: filename
 * @param origin: the $ORIGIN.
 * @param prev: passed to ldns.
 * @param skip: if true, the result is NULL, but not an error, skip it.
 * @return false on failure, otherwise the tp read.
 */
static struct trust_anchor*
load_trustanchor(struct val_anchors* anchors, char* str, const char* fname,
	ldns_rdf* origin, ldns_rdf** prev, int* skip)
{
        struct autr_ta* ta = NULL;
        struct trust_anchor* tp = NULL;

        ta = add_trustanchor_frm_str(anchors, str, &tp, origin, prev, skip);
	if(!ta)
		return NULL;
	lock_basic_lock(&tp->lock);
	if(!parse_comments(str, ta)) {
		lock_basic_unlock(&tp->lock);
		return NULL;
	}
	if(!tp->autr->file) {
		tp->autr->file = strdup(fname);
		if(!tp->autr->file) {
			lock_basic_unlock(&tp->lock);
			log_err("malloc failure");
			return NULL;
		}
	}
	lock_basic_unlock(&tp->lock);
        return tp;
}

/**
 * Assemble the trust anchors into DS and DNSKEY packed rrsets.
 * Uses only VALID and MISSING DNSKEYs.
 * Read the ldns_rrs and builds packed rrsets
 * @param tp: the trust point. Must be locked.
 * @return false on malloc failure.
 */
static int 
autr_assemble(struct trust_anchor* tp)
{
	ldns_rr_list* ds, *dnskey;
	struct autr_ta* ta;
	struct ub_packed_rrset_key* ubds=NULL, *ubdnskey=NULL;

	ds = ldns_rr_list_new();
	dnskey = ldns_rr_list_new();
	if(!ds || !dnskey) {
		ldns_rr_list_free(ds);
		ldns_rr_list_free(dnskey);
		return 0;
	}
	for(ta = tp->autr->keys; ta; ta = ta->next) {
		if(ldns_rr_get_type(ta->rr) == LDNS_RR_TYPE_DS) {
			if(!ldns_rr_list_push_rr(ds, ta->rr)) {
				ldns_rr_list_free(ds);
				ldns_rr_list_free(dnskey);
				return 0;
			}
		} else if(ta->s == AUTR_STATE_VALID || 
			ta->s == AUTR_STATE_MISSING) {
			if(!ldns_rr_list_push_rr(dnskey, ta->rr)) {
				ldns_rr_list_free(ds);
				ldns_rr_list_free(dnskey);
				return 0;
			}
		}
	}

	/* make packed rrset keys - malloced with no ID number, they
	 * are not in the cache */
	/* make packed rrset data (if there is a key) */

	if(ldns_rr_list_rr_count(ds) > 0) {
		ubds = ub_packed_rrset_heap_key(ds);
		if(!ubds) 
			goto error_cleanup;
		ubds->entry.data = packed_rrset_heap_data(ds);
		if(!ubds->entry.data)
			goto error_cleanup;
	}
	if(ldns_rr_list_rr_count(dnskey) > 0) {
		ubdnskey = ub_packed_rrset_heap_key(dnskey);
		if(!ubdnskey)
			goto error_cleanup;
		ubdnskey->entry.data = packed_rrset_heap_data(dnskey);
		if(!ubdnskey->entry.data) {
		error_cleanup:
			autr_rrset_delete(ubds);
			autr_rrset_delete(ubdnskey);
			ldns_rr_list_free(ds);
			ldns_rr_list_free(dnskey);
			return 0;
		}
	}
	/* we have prepared the new keys so nothing can go wrong any more.
	 * And we are sure we cannot be left without trustanchor after
	 * any errors. Put in the new keys and remove old ones. */

	/* free the old data */
	autr_rrset_delete(tp->ds_rrset);
	autr_rrset_delete(tp->dnskey_rrset);

	/* assign the data to replace the old */
	tp->ds_rrset = ubds;
	tp->dnskey_rrset = ubdnskey;
	tp->numDS = ldns_rr_list_rr_count(ds);
	tp->numDNSKEY = ldns_rr_list_rr_count(dnskey);

	ldns_rr_list_free(ds);
	ldns_rr_list_free(dnskey);
	return 1;
}

/** parse integer */
static unsigned int
parse_int(char* line, int* ret)
{
	char *e;
	unsigned int x = (unsigned int)strtol(line, &e, 10);
	if(line == e) {
		*ret = -1; /* parse error */
		return 0; 
	}
	*ret = 1; /* matched */
	return x;
}

/** parse id sequence for anchor */
static struct trust_anchor*
parse_id(struct val_anchors* anchors, char* line)
{
	struct trust_anchor *tp;
	int r;
	ldns_rdf* rdf;
	uint16_t dclass;
	/* read the owner name */
	char* next = strchr(line, ' ');
	if(!next)
		return NULL;
	next[0] = 0;
	rdf = ldns_dname_new_frm_str(line);
	if(!rdf)
		return NULL;

	/* read the class */
	dclass = parse_int(next+1, &r);
	if(r == -1) {
		ldns_rdf_deep_free(rdf);
		return NULL;
	}

	/* find the trust point */
	tp = autr_tp_create(anchors, rdf, dclass);
	ldns_rdf_deep_free(rdf);
	return tp;
}

/** 
 * Parse variable from trustanchor header 
 * @param line: to parse
 * @param anchors: the anchor is added to this, if "id:" is seen.
 * @param anchor: the anchor as result value or previously returned anchor
 * 	value to read the variable lines into.
 * @return: 0 no match, -1 failed syntax error, +1 success line read.
 * 	+2 revoked trust anchor file.
 */
static int
parse_var_line(char* line, struct val_anchors* anchors, 
	struct trust_anchor** anchor)
{
	struct trust_anchor* tp = *anchor;
	int r = 0;
	if(strncmp(line, ";;id: ", 6) == 0) {
		*anchor = parse_id(anchors, line+6);
		if(!*anchor) return -1;
		else return 1;
	} else if(strncmp(line, ";;REVOKED", 9) == 0) {
		if(tp) {
			log_err("REVOKED statement must be at start of file");
			return -1;
		}
		return 2;
	} else if(strncmp(line, ";;last_queried: ", 16) == 0) {
		if(!tp) return -1;
		lock_basic_lock(&tp->lock);
		tp->autr->last_queried = (time_t)parse_int(line+16, &r);
		lock_basic_unlock(&tp->lock);
	} else if(strncmp(line, ";;last_success: ", 16) == 0) {
		if(!tp) return -1;
		lock_basic_lock(&tp->lock);
		tp->autr->last_success = (time_t)parse_int(line+16, &r);
		lock_basic_unlock(&tp->lock);
	} else if(strncmp(line, ";;next_probe_time: ", 19) == 0) {
		if(!tp) return -1;
		lock_basic_lock(&anchors->lock);
		lock_basic_lock(&tp->lock);
		(void)rbtree_delete(&anchors->autr->probe, tp);
		tp->autr->next_probe_time = (time_t)parse_int(line+19, &r);
		(void)rbtree_insert(&anchors->autr->probe, &tp->autr->pnode);
		lock_basic_unlock(&tp->lock);
		lock_basic_unlock(&anchors->lock);
	} else if(strncmp(line, ";;query_failed: ", 16) == 0) {
		if(!tp) return -1;
		lock_basic_lock(&tp->lock);
		tp->autr->query_failed = (uint8_t)parse_int(line+16, &r);
		lock_basic_unlock(&tp->lock);
	} else if(strncmp(line, ";;query_interval: ", 18) == 0) {
		if(!tp) return -1;
		lock_basic_lock(&tp->lock);
		tp->autr->query_interval = (uint32_t)parse_int(line+18, &r);
		lock_basic_unlock(&tp->lock);
	} else if(strncmp(line, ";;retry_time: ", 14) == 0) {
		if(!tp) return -1;
		lock_basic_lock(&tp->lock);
		tp->autr->retry_time = (uint32_t)parse_int(line+14, &r);
		lock_basic_unlock(&tp->lock);
	}
	return r;
}

/** handle origin lines */
static int
handle_origin(char* line, ldns_rdf** origin)
{
	while(isspace((int)*line))
		line++;
	if(strncmp(line, "$ORIGIN", 7) != 0)
		return 0;
	ldns_rdf_deep_free(*origin);
	line += 7;
	while(isspace((int)*line))
		line++;
	*origin = ldns_dname_new_frm_str(line);
	if(!*origin)
		log_warn("malloc failure or parse error in $ORIGIN");
	return 1;
}

/** Read one line and put multiline RRs onto one line string */
static int
read_multiline(char* buf, size_t len, FILE* in, int* linenr)
{
	char* pos = buf;
	size_t left = len;
	int depth = 0;
	buf[len-1] = 0;
	while(left > 0 && fgets(pos, (int)left, in) != NULL) {
		size_t i, poslen = strlen(pos);
		(*linenr)++;

		/* check what the new depth is after the line */
		/* this routine cannot handle braces inside quotes,
		   say for TXT records, but this routine only has to read keys */
		for(i=0; i<poslen; i++) {
			if(pos[i] == '(') {
				depth++;
			} else if(pos[i] == ')') {
				if(depth == 0) {
					log_err("mismatch: too many ')'");
					return -1;
				}
				depth--;
			} else if(pos[i] == ';') {
				break;
			}
		}

		/* normal oneline or last line: keeps newline and comments */
		if(depth == 0) {
			return 1;
		}

		/* more lines expected, snip off comments and newline */
		if(poslen>0) 
			pos[poslen-1] = 0; /* strip newline */
		if(strchr(pos, ';')) 
			strchr(pos, ';')[0] = 0; /* strip comments */

		/* move to paste other lines behind this one */
		poslen = strlen(pos);
		pos += poslen;
		left -= poslen;
		/* the newline is changed into a space */
		if(left <= 2 /* space and eos */) {
			log_err("line too long");
			return -1;
		}
		pos[0] = ' ';
		pos[1] = 0;
		pos += 1;
		left -= 1;
	}
	if(depth != 0) {
		log_err("mismatch: too many '('");
		return -1;
	}
	if(pos != buf)
		return 1;
	return 0;
}

int autr_read_file(struct val_anchors* anchors, const char* nm)
{
        /* the file descriptor */
        FILE* fd;
        /* keep track of line numbers */
        int line_nr = 0;
        /* single line */
        char line[10240];
	/* trust point being read */
	struct trust_anchor *tp = NULL, *tp2;
	int r;
	/* for $ORIGIN parsing */
	ldns_rdf *origin=NULL, *prev=NULL;

        if (!(fd = fopen(nm, "r"))) {
                log_err("unable to open %s for reading: %s", 
			nm, strerror(errno));
                return 0;
        }
        verbose(VERB_ALGO, "reading autotrust anchor file %s", nm);
        while ( (r=read_multiline(line, sizeof(line), fd, &line_nr)) != 0) {
		if(r == -1 || (r = parse_var_line(line, anchors, &tp)) == -1) {
			log_err("could not parse auto-trust-anchor-file "
				"%s line %d", nm, line_nr);
			fclose(fd);
			ldns_rdf_deep_free(origin);
			ldns_rdf_deep_free(prev);
			return 0;
		} else if(r == 1) {
			continue;
		} else if(r == 2) {
			log_warn("trust anchor %s has been revoked", nm);
			fclose(fd);
			ldns_rdf_deep_free(origin);
			ldns_rdf_deep_free(prev);
			return 1;
		}
        	if (!str_contains_data(line, ';'))
                	continue; /* empty lines allowed */
 		if(handle_origin(line, &origin))
			continue;
		r = 0;
                if(!(tp2=load_trustanchor(anchors, line, nm, origin, &prev, 
			&r))) {
			if(!r) log_err("failed to load trust anchor from %s "
				"at line %i, skipping", nm, line_nr);
                        /* try to do the rest */
			continue;
                }
		if(tp && tp != tp2) {
			log_err("file %s has mismatching data inside: "
				"the file may only contain keys for one name, "
				"remove keys for other domain names", nm);
        		fclose(fd);
			ldns_rdf_deep_free(origin);
			ldns_rdf_deep_free(prev);
			return 0;
		}
		tp = tp2;
        }
        fclose(fd);
	ldns_rdf_deep_free(origin);
	ldns_rdf_deep_free(prev);
	if(!tp) {
		log_err("failed to read %s", nm);
		return 0;
	}

	/* now assemble the data into DNSKEY and DS packed rrsets */
	lock_basic_lock(&tp->lock);
	if(!autr_assemble(tp)) {
		lock_basic_unlock(&tp->lock);
		log_err("malloc failure assembling %s", nm);
		return 0;
	}
	lock_basic_unlock(&tp->lock);
	return 1;
}

/** string for a trustanchor state */
static const char*
trustanchor_state2str(autr_state_t s)
{
        switch (s) {
                case AUTR_STATE_START:       return "  START  ";
                case AUTR_STATE_ADDPEND:     return " ADDPEND ";
                case AUTR_STATE_VALID:       return "  VALID  ";
                case AUTR_STATE_MISSING:     return " MISSING ";
                case AUTR_STATE_REVOKED:     return " REVOKED ";
                case AUTR_STATE_REMOVED:     return " REMOVED ";
        }
        return " UNKNOWN ";
}

/** print ID to file */
static int
print_id(FILE* out, char* fname, struct module_env* env, 
	uint8_t* nm, size_t nmlen, uint16_t dclass)
{
	ldns_rdf rdf;
#ifdef UNBOUND_DEBUG
	ldns_status s;
#endif

	memset(&rdf, 0, sizeof(rdf));
	ldns_rdf_set_data(&rdf, nm);
	ldns_rdf_set_size(&rdf, nmlen);
	ldns_rdf_set_type(&rdf, LDNS_RDF_TYPE_DNAME);

	ldns_buffer_clear(env->scratch_buffer);
#ifdef UNBOUND_DEBUG
	s =
#endif
	ldns_rdf2buffer_str_dname(env->scratch_buffer, &rdf);
	log_assert(s == LDNS_STATUS_OK);
	ldns_buffer_write_u8(env->scratch_buffer, 0);
	ldns_buffer_flip(env->scratch_buffer);
	if(fprintf(out, ";;id: %s %d\n", 
		(char*)ldns_buffer_begin(env->scratch_buffer),
		(int)dclass) < 0) {
		log_err("could not write to %s: %s", fname, strerror(errno));
		return 0;
	}
	return 1;
}

static int
autr_write_contents(FILE* out, char* fn, struct module_env* env,
	struct trust_anchor* tp)
{
	char tmi[32];
	struct autr_ta* ta;
	char* str;

	/* write pretty header */
	if(fprintf(out, "; autotrust trust anchor file\n") < 0) {
		log_err("could not write to %s: %s", fn, strerror(errno));
		return 0;
	}
	if(tp->autr->revoked) {
		if(fprintf(out, ";;REVOKED\n") < 0 ||
		   fprintf(out, "; The zone has all keys revoked, and is\n"
			"; considered as if it has no trust anchors.\n"
			"; the remainder of the file is the last probe.\n"
			"; to restart the trust anchor, overwrite this file.\n"
			"; with one containing valid DNSKEYs or DSes.\n") < 0) {
		   log_err("could not write to %s: %s", fn, strerror(errno));
		   return 0;
		}
	}
	if(!print_id(out, fn, env, tp->name, tp->namelen, tp->dclass)) {
		return 0;
	}
	if(fprintf(out, ";;last_queried: %u ;;%s", 
		(unsigned int)tp->autr->last_queried, 
		ctime_r(&(tp->autr->last_queried), tmi)) < 0 ||
	   fprintf(out, ";;last_success: %u ;;%s", 
		(unsigned int)tp->autr->last_success,
		ctime_r(&(tp->autr->last_success), tmi)) < 0 ||
	   fprintf(out, ";;next_probe_time: %u ;;%s", 
		(unsigned int)tp->autr->next_probe_time,
		ctime_r(&(tp->autr->next_probe_time), tmi)) < 0 ||
	   fprintf(out, ";;query_failed: %d\n", (int)tp->autr->query_failed)<0
	   || fprintf(out, ";;query_interval: %d\n", 
	   (int)tp->autr->query_interval) < 0 ||
	   fprintf(out, ";;retry_time: %d\n", (int)tp->autr->retry_time) < 0) {
		log_err("could not write to %s: %s", fn, strerror(errno));
		return 0;
	}

	/* write anchors */
	for(ta=tp->autr->keys; ta; ta=ta->next) {
		/* by default do not store START and REMOVED keys */
		if(ta->s == AUTR_STATE_START)
			continue;
		if(ta->s == AUTR_STATE_REMOVED)
			continue;
		/* only store keys */
		if(ldns_rr_get_type(ta->rr) != LDNS_RR_TYPE_DNSKEY)
			continue;
		str = ldns_rr2str(ta->rr);
		if(!str || !str[0]) {
			free(str);
			log_err("malloc failure writing %s", fn);
			return 0;
		}
		str[strlen(str)-1] = 0; /* remove newline */
		if(fprintf(out, "%s ;;state=%d [%s] ;;count=%d "
			";;lastchange=%u ;;%s", str, (int)ta->s, 
			trustanchor_state2str(ta->s), (int)ta->pending_count,
			(unsigned int)ta->last_change, 
			ctime_r(&(ta->last_change), tmi)) < 0) {
		   log_err("could not write to %s: %s", fn, strerror(errno));
		   free(str);
		   return 0;
		}
		free(str);
	}
	return 1;
}

void autr_write_file(struct module_env* env, struct trust_anchor* tp)
{
	FILE* out;
	char* fname = tp->autr->file;
	char tempf[2048];
	log_assert(tp->autr);
	/* unique name with pid number and thread number */
	snprintf(tempf, sizeof(tempf), "%s.%d-%d", fname, (int)getpid(),
		env&&env->worker?*(int*)env->worker:0);
	verbose(VERB_ALGO, "autotrust: write to disk: %s", tempf);
	out = fopen(tempf, "w");
	if(!out) {
		log_err("could not open autotrust file for writing, %s: %s",
			tempf, strerror(errno));
		return;
	}
	if(!autr_write_contents(out, tempf, env, tp)) {
		/* failed to write contents (completely) */
		fclose(out);
		unlink(tempf);
		log_err("could not completely write: %s", fname);
		return;
	}
	/* success; overwrite actual file */
	fclose(out);
	verbose(VERB_ALGO, "autotrust: replaced %s", fname);
#ifdef UB_ON_WINDOWS
	(void)unlink(fname); /* windows does not replace file with rename() */
#endif
	if(rename(tempf, fname) < 0) {
		log_err("rename(%s to %s): %s", tempf, fname, strerror(errno));
	}
}

/** 
 * Verify if dnskey works for trust point 
 * @param env: environment (with time) for verification
 * @param ve: validator environment (with options) for verification.
 * @param tp: trust point to verify with
 * @param rrset: DNSKEY rrset to verify.
 * @return false on failure, true if verification successful.
 */
static int
verify_dnskey(struct module_env* env, struct val_env* ve,
        struct trust_anchor* tp, struct ub_packed_rrset_key* rrset)
{
	char* reason = NULL;
	uint8_t sigalg[ALGO_NEEDS_MAX+1];
	int downprot = 1;
	enum sec_status sec = val_verify_DNSKEY_with_TA(env, ve, rrset,
		tp->ds_rrset, tp->dnskey_rrset, downprot?sigalg:NULL, &reason);
	/* sigalg is ignored, it returns algorithms signalled to exist, but
	 * in 5011 there are no other rrsets to check.  if downprot is
	 * enabled, then it checks that the DNSKEY is signed with all
	 * algorithms available in the trust store. */
	verbose(VERB_ALGO, "autotrust: validate DNSKEY with anchor: %s",
		sec_status_to_string(sec));
	return sec == sec_status_secure;
}

/** Find minimum expiration interval from signatures */
static uint32_t
min_expiry(struct module_env* env, ldns_rr_list* rrset)
{
	size_t i;
	uint32_t t, r = 15 * 24 * 3600; /* 15 days max */
	for(i=0; i<ldns_rr_list_rr_count(rrset); i++) {
		ldns_rr* rr = ldns_rr_list_rr(rrset, i);
		if(ldns_rr_get_type(rr) != LDNS_RR_TYPE_RRSIG)
			continue;
		t = ldns_rdf2native_int32(ldns_rr_rrsig_expiration(rr));
		if(t - *env->now > 0) {
			t -= *env->now;
			if(t < r)
				r = t;
		}
	}
	return r;
}

/** Is rr self-signed revoked key */
static int
rr_is_selfsigned_revoked(struct module_env* env, struct val_env* ve,
	struct ub_packed_rrset_key* dnskey_rrset, size_t i)
{
	enum sec_status sec;
	char* reason = NULL;
	verbose(VERB_ALGO, "seen REVOKE flag, check self-signed, rr %d",
		(int)i);
	/* no algorithm downgrade protection necessary, if it is selfsigned
	 * revoked it can be removed. */
	sec = dnskey_verify_rrset(env, ve, dnskey_rrset, dnskey_rrset, i, 
		&reason);
	return (sec == sec_status_secure);
}

/** Set fetched value */
static void
seen_trustanchor(struct autr_ta* ta, uint8_t seen)
{
	ta->fetched = seen;
	if(ta->pending_count < 250) /* no numerical overflow, please */
		ta->pending_count++;
}

/** set revoked value */
static void
seen_revoked_trustanchor(struct autr_ta* ta, uint8_t revoked)
{
	ta->revoked = revoked;
}

/** revoke a trust anchor */
static void
revoke_dnskey(struct autr_ta* ta, int off)
{
        ldns_rdf* rdf;
        uint16_t flags;
	log_assert(ta && ta->rr);
	if(ldns_rr_get_type(ta->rr) != LDNS_RR_TYPE_DNSKEY)
		return;
	rdf = ldns_rr_dnskey_flags(ta->rr);
	flags = ldns_read_uint16(ldns_rdf_data(rdf));

	if (off && (flags&LDNS_KEY_REVOKE_KEY))
		flags ^= LDNS_KEY_REVOKE_KEY; /* flip */
	else
		flags |= LDNS_KEY_REVOKE_KEY;
	ldns_write_uint16(ldns_rdf_data(rdf), flags);
}

/** Compare two RR buffers skipping the REVOKED bit */
static int
ldns_rr_compare_wire_skip_revbit(ldns_buffer* rr1_buf, ldns_buffer* rr2_buf)
{
	size_t rr1_len, rr2_len, min_len, i, offset;
	rr1_len = ldns_buffer_capacity(rr1_buf);
	rr2_len = ldns_buffer_capacity(rr2_buf);
	/* jump past dname (checked in earlier part) and especially past TTL */
	offset = 0;
	while (offset < rr1_len && *ldns_buffer_at(rr1_buf, offset) != 0)
		offset += *ldns_buffer_at(rr1_buf, offset) + 1;
	/* jump to rdata section (PAST the rdata length field) */
	offset += 11; /* 0-dname-end + type + class + ttl + rdatalen */
	min_len = (rr1_len < rr2_len) ? rr1_len : rr2_len;
	/* compare RRs RDATA byte for byte. */
	for(i = offset; i < min_len; i++)
	{
		uint8_t *rdf1, *rdf2;
		rdf1 = ldns_buffer_at(rr1_buf, i);
		rdf2 = ldns_buffer_at(rr2_buf, i);
		if (i==(offset+1))
		{
			/* this is the second part of the flags field */
			*rdf1 = *rdf1 | LDNS_KEY_REVOKE_KEY;
			*rdf2 = *rdf2 | LDNS_KEY_REVOKE_KEY;
		}
		if (*rdf1 < *rdf2)	return -1;
		else if (*rdf1 > *rdf2)	return 1;
        }
	return 0;
}

/** Compare two RRs skipping the REVOKED bit */
static int
ldns_rr_compare_skip_revbit(const ldns_rr* rr1, const ldns_rr* rr2, int* result)
{
	size_t rr1_len, rr2_len;
	ldns_buffer* rr1_buf;
	ldns_buffer* rr2_buf;

	*result = ldns_rr_compare_no_rdata(rr1, rr2);
	if (*result == 0)
	{
		rr1_len = ldns_rr_uncompressed_size(rr1);
		rr2_len = ldns_rr_uncompressed_size(rr2);
		rr1_buf = ldns_buffer_new(rr1_len);
		rr2_buf = ldns_buffer_new(rr2_len);
		if(!rr1_buf || !rr2_buf) {
			ldns_buffer_free(rr1_buf);
			ldns_buffer_free(rr2_buf);
			return 0;
		}
		if (ldns_rr2buffer_wire_canonical(rr1_buf, rr1,
			LDNS_SECTION_ANY) != LDNS_STATUS_OK)
		{
			ldns_buffer_free(rr1_buf);
			ldns_buffer_free(rr2_buf);
			return 0;
		}
		if (ldns_rr2buffer_wire_canonical(rr2_buf, rr2,
			LDNS_SECTION_ANY) != LDNS_STATUS_OK) {
			ldns_buffer_free(rr1_buf);
			ldns_buffer_free(rr2_buf);
			return 0;
		}
		*result = ldns_rr_compare_wire_skip_revbit(rr1_buf, rr2_buf);
		ldns_buffer_free(rr1_buf);
		ldns_buffer_free(rr2_buf);
	}
	return 1;
}


/** compare two trust anchors */
static int
ta_compare(ldns_rr* a, ldns_rr* b, int* result)
{
	if (!a && !b)	*result = 0;
	else if (!a)	*result = -1;
	else if (!b)	*result = 1;
	else if (ldns_rr_get_type(a) != ldns_rr_get_type(b))
		*result = (int)ldns_rr_get_type(a) - (int)ldns_rr_get_type(b);
	else if (ldns_rr_get_type(a) == LDNS_RR_TYPE_DNSKEY) {
		if(!ldns_rr_compare_skip_revbit(a, b, result))
			return 0;
	}
	else if (ldns_rr_get_type(a) == LDNS_RR_TYPE_DS)
		*result = ldns_rr_compare(a, b);
	else    *result = -1;
	return 1;
}

/** 
 * Find key
 * @param tp: to search in
 * @param rr: to look for
 * @param result: returns NULL or the ta key looked for.
 * @return false on malloc failure during search. if true examine result.
 */
static int
find_key(struct trust_anchor* tp, ldns_rr* rr, struct autr_ta** result)
{
	struct autr_ta* ta;
	int ret;
	if(!tp || !rr)
		return 0;
	for(ta=tp->autr->keys; ta; ta=ta->next) {
		if(!ta_compare(ta->rr, rr, &ret))
			return 0;
		if(ret == 0) {
			*result = ta;
			return 1;
		}
	}
	*result = NULL;
	return 1;
}

/** add key and clone RR and tp already locked */
static struct autr_ta*
add_key(struct trust_anchor* tp, ldns_rr* rr)
{
	ldns_rr* c;
	struct autr_ta* ta;
	c = ldns_rr_clone(rr);
	if(!c) return NULL;
	ta = autr_ta_create(c);
	if(!ta) {
		ldns_rr_free(c);
		return NULL;
	}
	/* link in, tp already locked */
	ta->next = tp->autr->keys;
	tp->autr->keys = ta;
	return ta;
}

/** get TTL from DNSKEY rrset */
static uint32_t
key_ttl(struct ub_packed_rrset_key* k)
{
	struct packed_rrset_data* d = (struct packed_rrset_data*)k->entry.data;
	return d->ttl;
}

/** update the time values for the trustpoint */
static void
set_tp_times(struct trust_anchor* tp, uint32_t rrsig_exp_interval, 
	uint32_t origttl, int* changed)
{
	uint32_t x, qi = tp->autr->query_interval, rt = tp->autr->retry_time;
	
	/* x = MIN(15days, ttl/2, expire/2) */
	x = 15 * 24 * 3600;
	if(origttl/2 < x)
		x = origttl/2;
	if(rrsig_exp_interval/2 < x)
		x = rrsig_exp_interval/2;
	/* MAX(1hr, x) */
	if(x < 3600)
		tp->autr->query_interval = 3600;
	else	tp->autr->query_interval = x;

	/* x= MIN(1day, ttl/10, expire/10) */
	x = 24 * 3600;
	if(origttl/10 < x)
		x = origttl/10;
	if(rrsig_exp_interval/10 < x)
		x = rrsig_exp_interval/10;
	/* MAX(1hr, x) */
	if(x < 3600)
		tp->autr->retry_time = 3600;
	else	tp->autr->retry_time = x;

	if(qi != tp->autr->query_interval || rt != tp->autr->retry_time) {
		*changed = 1;
		verbose(VERB_ALGO, "orig_ttl is %d", (int)origttl);
		verbose(VERB_ALGO, "rrsig_exp_interval is %d", 
			(int)rrsig_exp_interval);
		verbose(VERB_ALGO, "query_interval: %d, retry_time: %d",
			(int)tp->autr->query_interval, 
			(int)tp->autr->retry_time);
	}
}

/** init events to zero */
static void
init_events(struct trust_anchor* tp)
{
	struct autr_ta* ta;
	for(ta=tp->autr->keys; ta; ta=ta->next) {
		ta->fetched = 0;
	}
}

/** check for revoked keys without trusting any other information */
static void
check_contains_revoked(struct module_env* env, struct val_env* ve,
	struct trust_anchor* tp, struct ub_packed_rrset_key* dnskey_rrset,
	int* changed)
{
	ldns_rr_list* r = packed_rrset_to_rr_list(dnskey_rrset, 
		env->scratch_buffer);
	size_t i;
	if(!r) {
		log_err("malloc failure");
		return;
	}
	for(i=0; i<ldns_rr_list_rr_count(r); i++) {
		ldns_rr* rr = ldns_rr_list_rr(r, i);
		struct autr_ta* ta = NULL;
		if(ldns_rr_get_type(rr) != LDNS_RR_TYPE_DNSKEY)
			continue;
		if(!rr_is_dnskey_sep(rr) || !rr_is_dnskey_revoked(rr))
			continue; /* not a revoked KSK */
		if(!find_key(tp, rr, &ta)) {
			log_err("malloc failure");
			continue; /* malloc fail in compare*/
		}
		if(!ta)
			continue; /* key not found */
		if(rr_is_selfsigned_revoked(env, ve, dnskey_rrset, i)) {
			/* checked if there is an rrsig signed by this key. */
			log_assert(dnskey_calc_keytag(dnskey_rrset, i) ==
				ldns_calc_keytag(rr)); /* checks conversion*/
			verbose_key(ta, VERB_ALGO, "is self-signed revoked");
			if(!ta->revoked) 
				*changed = 1;
			seen_revoked_trustanchor(ta, 1);
			do_revoked(env, ta, changed);
		}
	}
	ldns_rr_list_deep_free(r);
}

/** See if a DNSKEY is verified by one of the DSes */
static int
key_matches_a_ds(struct module_env* env, struct val_env* ve,
	struct ub_packed_rrset_key* dnskey_rrset, size_t key_idx,
	struct ub_packed_rrset_key* ds_rrset)
{
	struct packed_rrset_data* dd = (struct packed_rrset_data*)
	                ds_rrset->entry.data;
	size_t ds_idx, num = dd->count;
	int d = val_favorite_ds_algo(ds_rrset);
	char* reason = "";
	for(ds_idx=0; ds_idx<num; ds_idx++) {
		if(!ds_digest_algo_is_supported(ds_rrset, ds_idx) ||
			!ds_key_algo_is_supported(ds_rrset, ds_idx) ||
			ds_get_digest_algo(ds_rrset, ds_idx) != d)
			continue;
		if(ds_get_key_algo(ds_rrset, ds_idx)
		   != dnskey_get_algo(dnskey_rrset, key_idx)
		   || dnskey_calc_keytag(dnskey_rrset, key_idx)
		   != ds_get_keytag(ds_rrset, ds_idx)) {
			continue;
		}
		if(!ds_digest_match_dnskey(env, dnskey_rrset, key_idx,
			ds_rrset, ds_idx)) {
			verbose(VERB_ALGO, "DS match attempt failed");
			continue;
		}
		if(dnskey_verify_rrset(env, ve, dnskey_rrset, 
			dnskey_rrset, key_idx, &reason) == sec_status_secure) {
			return 1;
		} else {
			verbose(VERB_ALGO, "DS match failed because the key "
				"does not verify the keyset: %s", reason);
		}
	}
	return 0;
}

/** Set update events */
static int
update_events(struct module_env* env, struct val_env* ve, 
	struct trust_anchor* tp, struct ub_packed_rrset_key* dnskey_rrset, 
	int* changed)
{
	ldns_rr_list* r = packed_rrset_to_rr_list(dnskey_rrset, 
		env->scratch_buffer);
	size_t i;
	if(!r) 
		return 0;
	init_events(tp);
	for(i=0; i<ldns_rr_list_rr_count(r); i++) {
		ldns_rr* rr = ldns_rr_list_rr(r, i);
		struct autr_ta* ta = NULL;
		if(ldns_rr_get_type(rr) != LDNS_RR_TYPE_DNSKEY)
			continue;
		if(!rr_is_dnskey_sep(rr))
			continue;
		if(rr_is_dnskey_revoked(rr)) {
			/* self-signed revoked keys already detected before,
			 * other revoked keys are not 'added' again */
			continue;
		}
		/* is a key of this type supported?. Note rr_list and
		 * packed_rrset are in the same order. */
		if(!dnskey_algo_is_supported(dnskey_rrset, i)) {
			/* skip unknown algorithm key, it is useless to us */
			log_nametypeclass(VERB_DETAIL, "trust point has "
				"unsupported algorithm at", 
				tp->name, LDNS_RR_TYPE_DNSKEY, tp->dclass);
			continue;
		}

		/* is it new? if revocation bit set, find the unrevoked key */
		if(!find_key(tp, rr, &ta)) {
			ldns_rr_list_deep_free(r); /* malloc fail in compare*/
			return 0;
		}
		if(!ta) {
			ta = add_key(tp, rr);
			*changed = 1;
			/* first time seen, do we have DSes? if match: VALID */
			if(ta && tp->ds_rrset && key_matches_a_ds(env, ve,
				dnskey_rrset, i, tp->ds_rrset)) {
				verbose_key(ta, VERB_ALGO, "verified by DS");
				ta->s = AUTR_STATE_VALID;
			}
		}
		if(!ta) {
			ldns_rr_list_deep_free(r);
			return 0;
		}
		seen_trustanchor(ta, 1);
		verbose_key(ta, VERB_ALGO, "in DNS response");
	}
	set_tp_times(tp, min_expiry(env, r), key_ttl(dnskey_rrset), changed);
	ldns_rr_list_deep_free(r);
	return 1;
}

/**
 * Check if the holddown time has already exceeded
 * setting: add-holddown: add holddown timer
 * setting: del-holddown: del holddown timer
 * @param env: environment with current time
 * @param ta: trust anchor to check for.
 * @param holddown: the timer value
 * @return number of seconds the holddown has passed.
 */
static int
check_holddown(struct module_env* env, struct autr_ta* ta, 
	unsigned int holddown)
{
        unsigned int elapsed;
	if((unsigned)*env->now < (unsigned)ta->last_change) {
		log_warn("time goes backwards. delaying key holddown");
		return 0;
	}
	elapsed = (unsigned)*env->now - (unsigned)ta->last_change;
        if (elapsed > holddown) {
                return (int) (elapsed-holddown);
        }
	verbose_key(ta, VERB_ALGO, "holddown time %d seconds to go",
		(int) (holddown-elapsed));
        return 0;
}


/** Set last_change to now */
static void
reset_holddown(struct module_env* env, struct autr_ta* ta, int* changed)
{
	ta->last_change = *env->now;
	*changed = 1;
}

/** Set the state for this trust anchor */
static void
set_trustanchor_state(struct module_env* env, struct autr_ta* ta, int* changed,
	autr_state_t s)
{
	verbose_key(ta, VERB_ALGO, "update: %s to %s",
		trustanchor_state2str(ta->s), trustanchor_state2str(s));
	ta->s = s;
	reset_holddown(env, ta, changed);
}


/** Event: NewKey */
static void
do_newkey(struct module_env* env, struct autr_ta* anchor, int* c)
{
	if (anchor->s == AUTR_STATE_START)
		set_trustanchor_state(env, anchor, c, AUTR_STATE_ADDPEND);
}

/** Event: AddTime */
static void
do_addtime(struct module_env* env, struct autr_ta* anchor, int* c)
{
	/* This not according to RFC, this is 30 days, but the RFC demands 
	 * MAX(30days, TTL expire time of first DNSKEY set with this key),
	 * The value may be too small if a very large TTL was used. */
	int exceeded = check_holddown(env, anchor, env->cfg->add_holddown);
	if (exceeded && anchor->s == AUTR_STATE_ADDPEND) {
		verbose_key(anchor, VERB_ALGO, "add-holddown time exceeded "
			"%d seconds ago, and pending-count %d", exceeded,
			anchor->pending_count);
		if(anchor->pending_count >= MIN_PENDINGCOUNT) {
			set_trustanchor_state(env, anchor, c, AUTR_STATE_VALID);
			anchor->pending_count = 0;
			return;
		}
		verbose_key(anchor, VERB_ALGO, "add-holddown time sanity check "
			"failed (pending count: %d)", anchor->pending_count);
	}
}

/** Event: RemTime */
static void
do_remtime(struct module_env* env, struct autr_ta* anchor, int* c)
{
	int exceeded = check_holddown(env, anchor, env->cfg->del_holddown);
	if(exceeded && anchor->s == AUTR_STATE_REVOKED) {
		verbose_key(anchor, VERB_ALGO, "del-holddown time exceeded "
			"%d seconds ago", exceeded);
		set_trustanchor_state(env, anchor, c, AUTR_STATE_REMOVED);
	}
}

/** Event: KeyRem */
static void
do_keyrem(struct module_env* env, struct autr_ta* anchor, int* c)
{
	if(anchor->s == AUTR_STATE_ADDPEND) {
		set_trustanchor_state(env, anchor, c, AUTR_STATE_START);
		anchor->pending_count = 0;
	} else if(anchor->s == AUTR_STATE_VALID)
		set_trustanchor_state(env, anchor, c, AUTR_STATE_MISSING);
}

/** Event: KeyPres */
static void
do_keypres(struct module_env* env, struct autr_ta* anchor, int* c)
{
	if(anchor->s == AUTR_STATE_MISSING)
		set_trustanchor_state(env, anchor, c, AUTR_STATE_VALID);
}

/* Event: Revoked */
static void
do_revoked(struct module_env* env, struct autr_ta* anchor, int* c)
{
	if(anchor->s == AUTR_STATE_VALID || anchor->s == AUTR_STATE_MISSING) {
                set_trustanchor_state(env, anchor, c, AUTR_STATE_REVOKED);
		verbose_key(anchor, VERB_ALGO, "old id, prior to revocation");
                revoke_dnskey(anchor, 0);
		verbose_key(anchor, VERB_ALGO, "new id, after revocation");
	}
}

/** Do statestable transition matrix for anchor */
static void
anchor_state_update(struct module_env* env, struct autr_ta* anchor, int* c)
{
	log_assert(anchor);
	switch(anchor->s) {
	/* START */
	case AUTR_STATE_START:
		/* NewKey: ADDPEND */
		if (anchor->fetched)
			do_newkey(env, anchor, c);
		break;
	/* ADDPEND */
	case AUTR_STATE_ADDPEND:
		/* KeyRem: START */
		if (!anchor->fetched)
			do_keyrem(env, anchor, c);
		/* AddTime: VALID */
		else	do_addtime(env, anchor, c);
		break;
	/* VALID */
	case AUTR_STATE_VALID:
		/* RevBit: REVOKED */
		if (anchor->revoked)
			do_revoked(env, anchor, c);
		/* KeyRem: MISSING */
		else if (!anchor->fetched)
			do_keyrem(env, anchor, c);
		else if(!anchor->last_change) {
			verbose_key(anchor, VERB_ALGO, "first seen");
			reset_holddown(env, anchor, c);
		}
		break;
	/* MISSING */
	case AUTR_STATE_MISSING:
		/* RevBit: REVOKED */
		if (anchor->revoked)
			do_revoked(env, anchor, c);
		/* KeyPres */
		else if (anchor->fetched)
			do_keypres(env, anchor, c);
		break;
	/* REVOKED */
	case AUTR_STATE_REVOKED:
		if (anchor->fetched)
			reset_holddown(env, anchor, c);
		/* RemTime: REMOVED */
		else	do_remtime(env, anchor, c);
		break;
	/* REMOVED */
	case AUTR_STATE_REMOVED:
	default:
		break;
	}
}

/** if ZSK init then trust KSKs */
static int
init_zsk_to_ksk(struct module_env* env, struct trust_anchor* tp, int* changed)
{
	/* search for VALID ZSKs */
	struct autr_ta* anchor;
	int validzsk = 0;
	int validksk = 0;
	for(anchor = tp->autr->keys; anchor; anchor = anchor->next) {
		/* last_change test makes sure it was manually configured */
                if (ldns_rr_get_type(anchor->rr) == LDNS_RR_TYPE_DNSKEY &&
			anchor->last_change == 0 && 
			!rr_is_dnskey_sep(anchor->rr) &&
			anchor->s == AUTR_STATE_VALID)
                        validzsk++;
	}
	if(validzsk == 0)
		return 0;
	for(anchor = tp->autr->keys; anchor; anchor = anchor->next) {
                if (rr_is_dnskey_sep(anchor->rr) && 
			anchor->s == AUTR_STATE_ADDPEND) {
			verbose_key(anchor, VERB_ALGO, "trust KSK from "
				"ZSK(config)");
			set_trustanchor_state(env, anchor, changed, 
				AUTR_STATE_VALID);
			validksk++;
		}
	}
	return validksk;
}

/** Remove missing trustanchors so the list does not grow forever */
static void
remove_missing_trustanchors(struct module_env* env, struct trust_anchor* tp,
	int* changed)
{
	struct autr_ta* anchor;
	int exceeded;
	int valid = 0;
	/* see if we have anchors that are valid */
	for(anchor = tp->autr->keys; anchor; anchor = anchor->next) {
		/* Only do KSKs */
                if (!rr_is_dnskey_sep(anchor->rr))
                        continue;
                if (anchor->s == AUTR_STATE_VALID)
                        valid++;
	}
	/* if there are no SEP Valid anchors, see if we started out with
	 * a ZSK (last-change=0) anchor, which is VALID and there are KSKs
	 * now that can be made valid.  Do this immediately because there
	 * is no guarantee that the ZSKs get announced long enough.  Usually
	 * this is immediately after init with a ZSK trusted, unless the domain
	 * was not advertising any KSKs at all.  In which case we perfectly
	 * track the zero number of KSKs. */
	if(valid == 0) {
		valid = init_zsk_to_ksk(env, tp, changed);
		if(valid == 0)
			return;
	}
	
	for(anchor = tp->autr->keys; anchor; anchor = anchor->next) {
		/* ignore ZSKs if newly added */
		if(anchor->s == AUTR_STATE_START)
			continue;
		/* remove ZSKs if a KSK is present */
                if (!rr_is_dnskey_sep(anchor->rr)) {
			if(valid > 0) {
				verbose_key(anchor, VERB_ALGO, "remove ZSK "
					"[%d key(s) VALID]", valid);
				set_trustanchor_state(env, anchor, changed, 
					AUTR_STATE_REMOVED);
			}
                        continue;
		}
                /* Only do MISSING keys */
                if (anchor->s != AUTR_STATE_MISSING)
                        continue;
		if(env->cfg->keep_missing == 0)
			continue; /* keep forever */

		exceeded = check_holddown(env, anchor, env->cfg->keep_missing);
		/* If keep_missing has exceeded and we still have more than 
		 * one valid KSK: remove missing trust anchor */
                if (exceeded && valid > 0) {
			verbose_key(anchor, VERB_ALGO, "keep-missing time "
				"exceeded %d seconds ago, [%d key(s) VALID]",
				exceeded, valid);
			set_trustanchor_state(env, anchor, changed, 
				AUTR_STATE_REMOVED);
		}
	}
}

/** Do the statetable from RFC5011 transition matrix */
static int
do_statetable(struct module_env* env, struct trust_anchor* tp, int* changed)
{
	struct autr_ta* anchor;
	for(anchor = tp->autr->keys; anchor; anchor = anchor->next) {
		/* Only do KSKs */
		if(!rr_is_dnskey_sep(anchor->rr))
			continue;
		anchor_state_update(env, anchor, changed);
	}
	remove_missing_trustanchors(env, tp, changed);
	return 1;
}

/** See if time alone makes ADDPEND to VALID transition */
static void
autr_holddown_exceed(struct module_env* env, struct trust_anchor* tp, int* c)
{
	struct autr_ta* anchor;
	for(anchor = tp->autr->keys; anchor; anchor = anchor->next) {
		if(rr_is_dnskey_sep(anchor->rr) && 
			anchor->s == AUTR_STATE_ADDPEND)
			do_addtime(env, anchor, c);
	}
}

/** cleanup key list */
static void
autr_cleanup_keys(struct trust_anchor* tp)
{
	struct autr_ta* p, **prevp;
	prevp = &tp->autr->keys;
	p = tp->autr->keys;
	while(p) {
		/* do we want to remove this key? */
		if(p->s == AUTR_STATE_START || p->s == AUTR_STATE_REMOVED ||
			ldns_rr_get_type(p->rr) != LDNS_RR_TYPE_DNSKEY) {
			struct autr_ta* np = p->next;
			/* remove */
			ldns_rr_free(p->rr);
			free(p);
			/* snip and go to next item */
			*prevp = np;
			p = np;
			continue;
		}
		/* remove pending counts if no longer pending */
		if(p->s != AUTR_STATE_ADDPEND)
			p->pending_count = 0;
		prevp = &p->next;
		p = p->next;
	}
}

/** calculate next probe time */
static time_t
calc_next_probe(struct module_env* env, uint32_t wait)
{
	/* make it random, 90-100% */
	uint32_t rnd, rest;
	if(wait < 3600)
		wait = 3600;
	rnd = wait/10;
	rest = wait-rnd;
	rnd = (uint32_t)ub_random_max(env->rnd, (long int)rnd);
	return (time_t)(*env->now + rest + rnd);
}

/** what is first probe time (anchors must be locked) */
static time_t
wait_probe_time(struct val_anchors* anchors)
{
	rbnode_t* t = rbtree_first(&anchors->autr->probe);
	if(t != RBTREE_NULL) 
		return ((struct trust_anchor*)t->key)->autr->next_probe_time;
	return 0;
}

/** reset worker timer */
static void
reset_worker_timer(struct module_env* env)
{
	struct timeval tv;
#ifndef S_SPLINT_S
	uint32_t next = (uint32_t)wait_probe_time(env->anchors);
	/* in case this is libunbound, no timer */
	if(!env->probe_timer)
		return;
	if(next > *env->now)
		tv.tv_sec = (time_t)(next - *env->now);
	else	tv.tv_sec = 0;
#endif
	tv.tv_usec = 0;
	comm_timer_set(env->probe_timer, &tv);
	verbose(VERB_ALGO, "scheduled next probe in %d sec", (int)tv.tv_sec);
}

/** set next probe for trust anchor */
static int
set_next_probe(struct module_env* env, struct trust_anchor* tp,
	struct ub_packed_rrset_key* dnskey_rrset)
{
	struct trust_anchor key, *tp2;
	time_t mold, mnew;
	/* use memory allocated in rrset for temporary name storage */
	key.node.key = &key;
	key.name = dnskey_rrset->rk.dname;
	key.namelen = dnskey_rrset->rk.dname_len;
	key.namelabs = dname_count_labels(key.name);
	key.dclass = tp->dclass;
	lock_basic_unlock(&tp->lock);

	/* fetch tp again and lock anchors, so that we can modify the trees */
	lock_basic_lock(&env->anchors->lock);
	tp2 = (struct trust_anchor*)rbtree_search(env->anchors->tree, &key);
	if(!tp2) {
		verbose(VERB_ALGO, "trustpoint was deleted in set_next_probe");
		lock_basic_unlock(&env->anchors->lock);
		return 0;
	}
	log_assert(tp == tp2);
	lock_basic_lock(&tp->lock);

	/* schedule */
	mold = wait_probe_time(env->anchors);
	(void)rbtree_delete(&env->anchors->autr->probe, tp);
	tp->autr->next_probe_time = calc_next_probe(env, 
		tp->autr->query_interval);
	(void)rbtree_insert(&env->anchors->autr->probe, &tp->autr->pnode);
	mnew = wait_probe_time(env->anchors);

	lock_basic_unlock(&env->anchors->lock);
	verbose(VERB_ALGO, "next probe set in %d seconds", 
		(int)tp->autr->next_probe_time - (int)*env->now);
	if(mold != mnew) {
		reset_worker_timer(env);
	}
	return 1;
}

/** Revoke and Delete a trust point */
static void
autr_tp_remove(struct module_env* env, struct trust_anchor* tp,
	struct ub_packed_rrset_key* dnskey_rrset)
{
	struct trust_anchor* del_tp;
	struct trust_anchor key;
	struct autr_point_data pd;
	time_t mold, mnew;

	log_nametypeclass(VERB_OPS, "trust point was revoked",
		tp->name, LDNS_RR_TYPE_DNSKEY, tp->dclass);
	tp->autr->revoked = 1;

	/* use space allocated for dnskey_rrset to save name of anchor */
	memset(&key, 0, sizeof(key));
	memset(&pd, 0, sizeof(pd));
	key.autr = &pd;
	key.node.key = &key;
	pd.pnode.key = &key;
	pd.next_probe_time = tp->autr->next_probe_time;
	key.name = dnskey_rrset->rk.dname;
	key.namelen = tp->namelen;
	key.namelabs = tp->namelabs;
	key.dclass = tp->dclass;

	/* unlock */
	lock_basic_unlock(&tp->lock);

	/* take from tree. It could be deleted by someone else,hence (void). */
	lock_basic_lock(&env->anchors->lock);
	del_tp = (struct trust_anchor*)rbtree_delete(env->anchors->tree, &key);
	mold = wait_probe_time(env->anchors);
	(void)rbtree_delete(&env->anchors->autr->probe, &key);
	mnew = wait_probe_time(env->anchors);
	anchors_init_parents_locked(env->anchors);
	lock_basic_unlock(&env->anchors->lock);

	/* if !del_tp then the trust point is no longer present in the tree,
	 * it was deleted by someone else, who will write the zonefile and
	 * clean up the structure */
	if(del_tp) {
		/* save on disk */
		del_tp->autr->next_probe_time = 0; /* no more probing for it */
		autr_write_file(env, del_tp);

		/* delete */
		autr_point_delete(del_tp);
	}
	if(mold != mnew) {
		reset_worker_timer(env);
	}
}

int autr_process_prime(struct module_env* env, struct val_env* ve,
	struct trust_anchor* tp, struct ub_packed_rrset_key* dnskey_rrset)
{
	int changed = 0;
	log_assert(tp && tp->autr);
	/* autotrust update trust anchors */
	/* the tp is locked, and stays locked unless it is deleted */

	/* we could just catch the anchor here while another thread
	 * is busy deleting it. Just unlock and let the other do its job */
	if(tp->autr->revoked) {
		log_nametypeclass(VERB_ALGO, "autotrust not processed, "
			"trust point revoked", tp->name, 
			LDNS_RR_TYPE_DNSKEY, tp->dclass);
		lock_basic_unlock(&tp->lock);
		return 0; /* it is revoked */
	}

	/* query_dnskeys(): */
	tp->autr->last_queried = *env->now;

	log_nametypeclass(VERB_ALGO, "autotrust process for",
		tp->name, LDNS_RR_TYPE_DNSKEY, tp->dclass);
	/* see if time alone makes some keys valid */
	autr_holddown_exceed(env, tp, &changed);
	if(changed) {
		verbose(VERB_ALGO, "autotrust: morekeys, reassemble");
		if(!autr_assemble(tp)) {
			log_err("malloc failure assembling autotrust keys");
			return 1; /* unchanged */
		}
	}
	/* did we get any data? */
	if(!dnskey_rrset) {
		verbose(VERB_ALGO, "autotrust: no dnskey rrset");
		/* no update of query_failed, because then we would have
		 * to write to disk. But we cannot because we maybe are
		 * still 'initialising' with DS records, that we cannot write
		 * in the full format (which only contains KSKs). */
		return 1; /* trust point exists */
	}
	/* check for revoked keys to remove immediately */
	check_contains_revoked(env, ve, tp, dnskey_rrset, &changed);
	if(changed) {
		verbose(VERB_ALGO, "autotrust: revokedkeys, reassemble");
		if(!autr_assemble(tp)) {
			log_err("malloc failure assembling autotrust keys");
			return 1; /* unchanged */
		}
		if(!tp->ds_rrset && !tp->dnskey_rrset) {
			/* no more keys, all are revoked */
			/* this is a success for this probe attempt */
			tp->autr->last_success = *env->now;
			autr_tp_remove(env, tp, dnskey_rrset);
			return 0; /* trust point removed */
		}
	}
	/* verify the dnskey rrset and see if it is valid. */
	if(!verify_dnskey(env, ve, tp, dnskey_rrset)) {
		verbose(VERB_ALGO, "autotrust: dnskey did not verify.");
		/* only increase failure count if this is not the first prime,
		 * this means there was a previous succesful probe */
		if(tp->autr->last_success) {
			tp->autr->query_failed += 1;
			autr_write_file(env, tp);
		}
		return 1; /* trust point exists */
	}

	tp->autr->last_success = *env->now;
	tp->autr->query_failed = 0;

	/* Add new trust anchors to the data structure
	 * - note which trust anchors are seen this probe.
	 * Set trustpoint query_interval and retry_time.
	 * - find minimum rrsig expiration interval
	 */
	if(!update_events(env, ve, tp, dnskey_rrset, &changed)) {
		log_err("malloc failure in autotrust update_events. "
			"trust point unchanged.");
		return 1; /* trust point unchanged, so exists */
	}

	/* - for every SEP key do the 5011 statetable.
	 * - remove missing trustanchors (if veryold and we have new anchors).
	 */
	if(!do_statetable(env, tp, &changed)) {
		log_err("malloc failure in autotrust do_statetable. "
			"trust point unchanged.");
		return 1; /* trust point unchanged, so exists */
	}

	autr_cleanup_keys(tp);
	if(!set_next_probe(env, tp, dnskey_rrset))
		return 0; /* trust point does not exist */
	autr_write_file(env, tp);
	if(changed) {
		verbose(VERB_ALGO, "autotrust: changed, reassemble");
		if(!autr_assemble(tp)) {
			log_err("malloc failure assembling autotrust keys");
			return 1; /* unchanged */
		}
		if(!tp->ds_rrset && !tp->dnskey_rrset) {
			/* no more keys, all are revoked */
			autr_tp_remove(env, tp, dnskey_rrset);
			return 0; /* trust point removed */
		}
	} else verbose(VERB_ALGO, "autotrust: no changes");
	
	return 1; /* trust point exists */
}

/** debug print a trust anchor key */
static void 
autr_debug_print_ta(struct autr_ta* ta)
{
	char buf[32];
	char* str = ldns_rr2str(ta->rr);
	if(!str) {
		log_info("out of memory in debug_print_ta");
		return;
	}
	if(str && str[0]) str[strlen(str)-1]=0; /* remove newline */
	ctime_r(&ta->last_change, buf);
	if(buf[0]) buf[strlen(buf)-1]=0; /* remove newline */
	log_info("[%s] %s ;;state:%d ;;pending_count:%d%s%s last:%s",
		trustanchor_state2str(ta->s), str, ta->s, ta->pending_count,
		ta->fetched?" fetched":"", ta->revoked?" revoked":"", buf);
	free(str);
}

/** debug print a trust point */
static void 
autr_debug_print_tp(struct trust_anchor* tp)
{
	struct autr_ta* ta;
	char buf[257];
	if(!tp->autr)
		return;
	dname_str(tp->name, buf);
	log_info("trust point %s : %d", buf, (int)tp->dclass);
	log_info("assembled %d DS and %d DNSKEYs", 
		(int)tp->numDS, (int)tp->numDNSKEY);
	if(0) { /* turned off because it prints to stderr */
		ldns_buffer* bf = ldns_buffer_new(70000);
		ldns_rr_list* list;
		if(tp->ds_rrset) {
			list = packed_rrset_to_rr_list(tp->ds_rrset, bf);
			ldns_rr_list_print(stderr, list);
			ldns_rr_list_deep_free(list);
		}
		if(tp->dnskey_rrset) {
			list = packed_rrset_to_rr_list(tp->dnskey_rrset, bf);
			ldns_rr_list_print(stderr, list);
			ldns_rr_list_deep_free(list);
		}
		ldns_buffer_free(bf);
	}
	log_info("file %s", tp->autr->file);
	ctime_r(&tp->autr->last_queried, buf);
	if(buf[0]) buf[strlen(buf)-1]=0; /* remove newline */
	log_info("last_queried: %u %s", (unsigned)tp->autr->last_queried, buf);
	ctime_r(&tp->autr->last_success, buf);
	if(buf[0]) buf[strlen(buf)-1]=0; /* remove newline */
	log_info("last_success: %u %s", (unsigned)tp->autr->last_success, buf);
	ctime_r(&tp->autr->next_probe_time, buf);
	if(buf[0]) buf[strlen(buf)-1]=0; /* remove newline */
	log_info("next_probe_time: %u %s", (unsigned)tp->autr->next_probe_time,
		buf);
	log_info("query_interval: %u", (unsigned)tp->autr->query_interval);
	log_info("retry_time: %u", (unsigned)tp->autr->retry_time);
	log_info("query_failed: %u", (unsigned)tp->autr->query_failed);
		
	for(ta=tp->autr->keys; ta; ta=ta->next) {
		autr_debug_print_ta(ta);
	}
}

void 
autr_debug_print(struct val_anchors* anchors)
{
	struct trust_anchor* tp;
	lock_basic_lock(&anchors->lock);
	RBTREE_FOR(tp, struct trust_anchor*, anchors->tree) {
		lock_basic_lock(&tp->lock);
		autr_debug_print_tp(tp);
		lock_basic_unlock(&tp->lock);
	}
	lock_basic_unlock(&anchors->lock);
}

void probe_answer_cb(void* arg, int ATTR_UNUSED(rcode), 
	ldns_buffer* ATTR_UNUSED(buf), enum sec_status ATTR_UNUSED(sec),
	char* ATTR_UNUSED(why_bogus))
{
	/* retry was set before the query was done,
	 * re-querytime is set when query succeeded, but that may not
	 * have reset this timer because the query could have been
	 * handled by another thread. In that case, this callback would
	 * get called after the original timeout is done. 
	 * By not resetting the timer, it may probe more often, but not
	 * less often.
	 * Unless the new lookup resulted in smaller TTLs and thus smaller
	 * timeout values. In that case one old TTL could be mistakenly done.
	 */
	struct module_env* env = (struct module_env*)arg;
	verbose(VERB_ALGO, "autotrust probe answer cb");
	reset_worker_timer(env);
}

/** probe a trust anchor DNSKEY and unlocks tp */
static void
probe_anchor(struct module_env* env, struct trust_anchor* tp)
{
	struct query_info qinfo;
	uint16_t qflags = BIT_RD;
	struct edns_data edns;
	ldns_buffer* buf = env->scratch_buffer;
	qinfo.qname = regional_alloc_init(env->scratch, tp->name, tp->namelen);
	if(!qinfo.qname) {
		log_err("out of memory making 5011 probe");
		return;
	}
	qinfo.qname_len = tp->namelen;
	qinfo.qtype = LDNS_RR_TYPE_DNSKEY;
	qinfo.qclass = tp->dclass;
	log_query_info(VERB_ALGO, "autotrust probe", &qinfo);
	verbose(VERB_ALGO, "retry probe set in %d seconds", 
		(int)tp->autr->next_probe_time - (int)*env->now);
	edns.edns_present = 1;
	edns.ext_rcode = 0;
	edns.edns_version = 0;
	edns.bits = EDNS_DO;
	if(ldns_buffer_capacity(buf) < 65535)
		edns.udp_size = (uint16_t)ldns_buffer_capacity(buf);
	else	edns.udp_size = 65535;

	/* can't hold the lock while mesh_run is processing */
	lock_basic_unlock(&tp->lock);

	/* delete the DNSKEY from rrset and key cache so an active probe
	 * is done. First the rrset so another thread does not use it
	 * to recreate the key entry in a race condition. */
	rrset_cache_remove(env->rrset_cache, qinfo.qname, qinfo.qname_len,
		qinfo.qtype, qinfo.qclass, 0);
	key_cache_remove(env->key_cache, qinfo.qname, qinfo.qname_len, 
		qinfo.qclass);

	if(!mesh_new_callback(env->mesh, &qinfo, qflags, &edns, buf, 0, 
		&probe_answer_cb, env)) {
		log_err("out of memory making 5011 probe");
	}
}

/** fetch first to-probe trust-anchor and lock it and set retrytime */
static struct trust_anchor*
todo_probe(struct module_env* env, uint32_t* next)
{
	struct trust_anchor* tp;
	rbnode_t* el;
	/* get first one */
	lock_basic_lock(&env->anchors->lock);
	if( (el=rbtree_first(&env->anchors->autr->probe)) == RBTREE_NULL) {
		/* in case of revoked anchors */
		lock_basic_unlock(&env->anchors->lock);
		return NULL;
	}
	tp = (struct trust_anchor*)el->key;
	lock_basic_lock(&tp->lock);

	/* is it eligible? */
	if((uint32_t)tp->autr->next_probe_time > *env->now) {
		/* no more to probe */
		*next = (uint32_t)tp->autr->next_probe_time - *env->now;
		lock_basic_unlock(&tp->lock);
		lock_basic_unlock(&env->anchors->lock);
		return NULL;
	}

	/* reset its next probe time */
	(void)rbtree_delete(&env->anchors->autr->probe, tp);
	tp->autr->next_probe_time = calc_next_probe(env, tp->autr->retry_time);
	(void)rbtree_insert(&env->anchors->autr->probe, &tp->autr->pnode);
	lock_basic_unlock(&env->anchors->lock);

	return tp;
}

uint32_t 
autr_probe_timer(struct module_env* env)
{
	struct trust_anchor* tp;
	uint32_t next_probe = 3600;
	int num = 0;
	verbose(VERB_ALGO, "autotrust probe timer callback");
	/* while there are still anchors to probe */
	while( (tp = todo_probe(env, &next_probe)) ) {
		/* make a probe for this anchor */
		probe_anchor(env, tp);
		num++;
	}
	regional_free_all(env->scratch);
	if(num == 0)
		return 0; /* no trust points to probe */
	verbose(VERB_ALGO, "autotrust probe timer %d callbacks done", num);
	return next_probe;
}
