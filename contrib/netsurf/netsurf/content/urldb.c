/*
 * Copyright 2006 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2009 John Tytgat <joty@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Unified URL information database (implementation)
 *
 * URLs are stored in a tree-based structure as follows:
 *
 * The host component is extracted from each URL and, if a FQDN, split on
 * every '.'.The tree is constructed by inserting each FQDN segment in
 * reverse order. Duplicate nodes are merged.
 *
 * If the host part of an URL is an IP address, then this is added to the
 * tree verbatim (as if it were a TLD).
 *
 * This provides something looking like:
 *
 * 			      root (a sentinel)
 * 				|
 * 	-------------------------------------------------
 * 	|	|	|	|	|	|	|
 *     com     edu     gov  127.0.0.1  net     org     uk	TLDs
 * 	|	|	|		|	|	|
 *    google   ...     ...             ...     ...     co	2LDs
 * 	|						|
 *     www					       bbc  Hosts/Subdomains
 *							|
 *						       www	...
 *
 * Each of the nodes in this tree is a struct host_part. This stores the
 * FQDN segment (or IP address) with which the node is concerned. Each node
 * may contain further information about paths on a host (struct path_data)
 * or SSL certificate processing on a host-wide basis
 * (host_part::permit_invalid_certs).
 *
 * Path data is concerned with storing various metadata about the path in
 * question. This includes global history data, HTTP authentication details
 * and any associated HTTP cookies. This is stored as a tree of path segments
 * hanging off the relevant host_part node.
 *
 * Therefore, to find the last visited time of the URL
 * http://www.example.com/path/to/resource.html, the FQDN tree would be
 * traversed in the order root -> "com" -> "example" -> "www". The "www"
 * node would have attached to it a tree of struct path_data:
 *
 *			    (sentinel)
 *				|
 * 			       path
 * 				|
 * 			       to
 * 				|
 * 			   resource.html
 *
 * This represents the absolute path "/path/to/resource.html". The leaf node
 * "resource.html" contains the last visited time of the resource.
 *
 * The mechanism described above is, however, not particularly conducive to
 * fast searching of the database for a given URL (or URLs beginning with a
 * given prefix). Therefore, an anciliary data structure is used to enable
 * fast searching. This structure simply reflects the contents of the
 * database, with entries being added/removed at the same time as for the
 * core database. In order to ensure that degenerate cases are kept to a
 * minimum, we use an AAtree. This is an approximation of a Red-Black tree
 * with similar performance characteristics, but with a significantly
 * simpler implementation. Entries in this tree comprise pointers to the
 * leaf nodes of the host tree described above.
 *
 * REALLY IMPORTANT NOTE: urldb expects all URLs to be normalised. Use of 
 * non-normalised URLs with urldb will result in undefined behaviour and 
 * potential crashes.
 */

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include <curl/curl.h>

#include "image/bitmap.h"
#include "content/content.h"
#include "content/urldb.h"
#include "desktop/cookie_manager.h"
#include "utils/nsoption.h"
#include "utils/log.h"
#include "utils/corestrings.h"
#include "utils/filename.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "utils/bloom.h"

struct cookie_internal_data {
	char *name;		/**< Cookie name */
	char *value;		/**< Cookie value */
	bool value_was_quoted;	/**< Value was quoted in Set-Cookie: */
	char *comment;		/**< Cookie comment */
	bool domain_from_set;	/**< Domain came from Set-Cookie: header */
	char *domain;		/**< Domain */
	bool path_from_set;	/**< Path came from Set-Cookie: header */
	char *path;		/**< Path */
	time_t expires;		/**< Expiry timestamp, or -1 for session */
	time_t last_used;	/**< Last used time */
	bool secure;		/**< Only send for HTTPS requests */
	bool http_only;		/**< Only expose to HTTP(S) requests */
	cookie_version version;	/**< Specification compliance */
	bool no_destroy;	/**< Never destroy this cookie,
				 * unless it's expired */

	struct cookie_internal_data *prev;	/**< Previous in list */
	struct cookie_internal_data *next;	/**< Next in list */
};

/* A protection space is defined as a tuple canonical_root_url and realm.
 * This structure lives as linked list element in a leaf host_part struct
 * so we need additional scheme and port to have a canonical_root_url.  */
struct prot_space_data {
	lwc_string *scheme;	/**< URL scheme of canonical hostname of this
				 * protection space. */
	unsigned int port;	/**< Port number of canonical hostname of this
				 * protection space. When 0, it means the
				 * default port for given scheme, i.e. 80
				 * (http), 443 (https). */
	char *realm;		/**< Protection realm */

	char *auth;		/**< Authentication details for this
				 * protection space in form
				 * username:password */
	struct prot_space_data *next;	/**< Next sibling */
};

struct cache_internal_data {
	char filename[12];	/**< Cached filename, or first byte 0 for none */
};

struct url_internal_data {
	char *title;		/**< Resource title */
	unsigned int visits;	/**< Visit count */
	time_t last_visit;	/**< Last visit time */
	content_type type;	/**< Type of resource */
};

struct path_data {
	nsurl *url;		/**< Full URL */
	lwc_string *scheme;	/**< URL scheme for data */
	unsigned int port;	/**< Port number for data. When 0, it means
				 * the default port for given scheme, i.e.
				 * 80 (http), 443 (https). */
	char *segment;		/**< Path segment for this node */
	unsigned int frag_cnt;	/**< Number of entries in path_data::fragment */
	char **fragment;	/**< Array of fragments */
	bool persistent;	/**< This entry should persist */

	struct bitmap *thumb;	/**< Thumbnail image of resource */
	struct url_internal_data urld;	/**< URL data for resource */
	struct cache_internal_data cache;	/**< Cache data for resource */
	const struct prot_space_data *prot_space;	/**< Protection space
				 * to which this resource belongs too. Can be
				 * NULL when it does not belong to a protection
				 * space or when it is not known. No
				 * ownership (is with struct host_part::prot_space). */
	struct cookie_internal_data *cookies;	/**< Cookies associated with resource */
	struct cookie_internal_data *cookies_end;	/**< Last cookie in list */

	struct path_data *next;	/**< Next sibling */
	struct path_data *prev;	/**< Previous sibling */
	struct path_data *parent;	/**< Parent path segment */
	struct path_data *children;	/**< Child path segments */
	struct path_data *last;		/**< Last child */
};

struct host_part {
	/**< Known paths on this host. This _must_ be first so that
	 * struct host_part *h = (struct host_part *)mypath; works */
	struct path_data paths;
	bool permit_invalid_certs;	/**< Allow access to SSL protected
					 * resources on this host without
					 * verifying certificate authenticity
					 */

	char *part;		/**< Part of host string */

	struct prot_space_data *prot_space;	/**< Linked list of all known
				 * proctection spaces known for his host and
				 * all its schems and ports. */

	struct host_part *next;	/**< Next sibling */
	struct host_part *prev;	/**< Previous sibling */
	struct host_part *parent;	/**< Parent host part */
	struct host_part *children;	/**< Child host parts */
};

struct search_node {
	const struct host_part *data;	/**< Host tree entry */

	unsigned int level;		/**< Node level */

	struct search_node *left;	/**< Left subtree */
	struct search_node *right;	/**< Right subtree */
};

/* Destruction */
static void urldb_destroy_host_tree(struct host_part *root);
static void urldb_destroy_path_tree(struct path_data *root);
static void urldb_destroy_path_node_content(struct path_data *node);
static void urldb_destroy_cookie(struct cookie_internal_data *c);
static void urldb_destroy_prot_space(struct prot_space_data *space);
static void urldb_destroy_search_tree(struct search_node *root);

/* Saving */
static void urldb_save_search_tree(struct search_node *root, FILE *fp);
static void urldb_count_urls(const struct path_data *root, time_t expiry,
		unsigned int *count);
static void urldb_write_paths(const struct path_data *parent,
		const char *host, FILE *fp, char **path, int *path_alloc,
		int *path_used, time_t expiry);

/* Iteration */
static bool urldb_iterate_partial_host(struct search_node *root,
		const char *prefix, bool (*callback)(nsurl *url,
		const struct url_data *data));
static bool urldb_iterate_partial_path(const struct path_data *parent,
		const char *prefix, bool (*callback)(nsurl *url,
		const struct url_data *data));
static bool urldb_iterate_entries_host(struct search_node *parent,
		bool (*url_callback)(nsurl *url,
		const struct url_data *data),
		bool (*cookie_callback)(const struct cookie_data *data));
static bool urldb_iterate_entries_path(const struct path_data *parent,
		bool (*url_callback)(nsurl *url,
		const struct url_data *data),
		bool (*cookie_callback)(const struct cookie_data *data));

/* Insertion */
static struct host_part *urldb_add_host_node(const char *part,
		struct host_part *parent);
static struct path_data *urldb_add_path_node(lwc_string *scheme,
		unsigned int port, const char *segment, lwc_string *fragment,
		struct path_data *parent);
static int urldb_add_path_fragment_cmp(const void *a, const void *b);
static struct path_data *urldb_add_path_fragment(struct path_data *segment,
		lwc_string *fragment);

/* Lookup */
static struct path_data *urldb_find_url(nsurl *url);
static struct path_data *urldb_match_path(const struct path_data *parent,
		const char *path, lwc_string *scheme, unsigned short port);
static struct search_node **urldb_get_search_tree_direct(const char *host);
static struct search_node *urldb_get_search_tree(const char *host);

/* Dump */
static void urldb_dump_hosts(struct host_part *parent);
static void urldb_dump_paths(struct path_data *parent);
static void urldb_dump_search(struct search_node *parent, int depth);

/* Search tree */
static struct search_node *urldb_search_insert(struct search_node *root,
		const struct host_part *data);
static struct search_node *urldb_search_insert_internal(
		struct search_node *root, struct search_node *n);
/* for urldb_search_remove, see r5531 which removed it */
static const struct host_part *urldb_search_find(struct search_node *root,
		const char *host);
static struct search_node *urldb_search_skew(struct search_node *root);
static struct search_node *urldb_search_split(struct search_node *root);
static int urldb_search_match_host(const struct host_part *a,
		const struct host_part *b);
static int urldb_search_match_string(const struct host_part *a,
		const char *b);
static int urldb_search_match_prefix(const struct host_part *a,
		const char *b);

/* Cookies */
static struct cookie_internal_data *urldb_parse_cookie(nsurl *url,
		const char **cookie);
static bool urldb_parse_avpair(struct cookie_internal_data *c, char *n, 
		char *v, bool was_quoted);
static bool urldb_insert_cookie(struct cookie_internal_data *c, 
		lwc_string *scheme, nsurl *url);
static void urldb_free_cookie(struct cookie_internal_data *c);
static bool urldb_concat_cookie(struct cookie_internal_data *c, int version,
		int *used, int *alloc, char **buf);
static void urldb_delete_cookie_hosts(const char *domain, const char *path, 
		const char *name, struct host_part *parent);
static void urldb_delete_cookie_paths(const char *domain, const char *path, 
		const char *name, struct path_data *parent);
static void urldb_save_cookie_hosts(FILE *fp, struct host_part *parent);
static void urldb_save_cookie_paths(FILE *fp, struct path_data *parent);

/** Root database handle */
static struct host_part db_root;

/** Search trees - one per letter + 1 for IPs + 1 for Everything Else */
#define NUM_SEARCH_TREES 28
#define ST_IP 0
#define ST_EE 1
#define ST_DN 2
static struct search_node empty = { 0, 0, &empty, &empty };
static struct search_node *search_trees[NUM_SEARCH_TREES] = {
	&empty, &empty, &empty, &empty, &empty, &empty, &empty, &empty,
	&empty, &empty, &empty, &empty, &empty, &empty, &empty, &empty,
	&empty, &empty, &empty, &empty, &empty, &empty, &empty, &empty,
	&empty, &empty, &empty, &empty
};

#define MIN_COOKIE_FILE_VERSION 100
#define COOKIE_FILE_VERSION 102
static int loaded_cookie_file_version;
#define MIN_URL_FILE_VERSION 106
#define URL_FILE_VERSION 106

/* Bloom filter used for short-circuting the false case of "is this
 * URL in the database?".  BLOOM_SIZE controls how large the filter is
 * in bytes.  Primitive experimentation shows that for a filter of X
 * bytes filled with X items, searching for X items not in the filter
 * has a 5% false-positive rate.  We set it to 32kB, which should be
 * enough for all but the largest databases, while not being shockingly
 * wasteful on memory.
 */
static struct bloom_filter *url_bloom;
#define BLOOM_SIZE (1024 * 32)

/**
 * Import an URL database from file, replacing any existing database
 *
 * \param filename Name of file containing data
 */
void urldb_load(const char *filename)
{
#define MAXIMUM_URL_LENGTH 4096
	char s[MAXIMUM_URL_LENGTH];
	char host[256];
	struct host_part *h;
	int urls;
	int i;
	int version;
	int length;
	FILE *fp;

	assert(filename);

	LOG(("Loading URL file %s", filename));

        if (url_bloom == NULL)
                url_bloom = bloom_create(BLOOM_SIZE);

	fp = fopen(filename, "r");
	if (!fp) {
		LOG(("Failed to open file '%s' for reading", filename));
		return;
	}

	if (!fgets(s, MAXIMUM_URL_LENGTH, fp)) {
		fclose(fp);
		return;
	}

	version = atoi(s);
	if (version < MIN_URL_FILE_VERSION) {
		LOG(("Unsupported URL file version."));
		fclose(fp);
		return;
	}
	if (version > URL_FILE_VERSION) {
		LOG(("Unknown URL file version."));
		fclose(fp);
		return;
	}

	while (fgets(host, sizeof host, fp)) {
		/* get the hostname */
		length = strlen(host) - 1;
		host[length] = '\0';

		/* skip data that has ended up with a host of '' */
		if (length == 0) {
			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;
			urls = atoi(s);
			/* Eight fields/url */
			for (i = 0; i < (8 * urls); i++) {
				if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
					break;
			}
			continue;
		}

		/* read number of URLs */
		if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
			break;
		urls = atoi(s);

		/* no URLs => try next host */
		if (urls == 0) {
			LOG(("No URLs for '%s'", host));
			continue;
		}

		h = urldb_add_host(host);
		if (!h) {
			LOG(("Failed adding host: '%s'", host));
			die("Memory exhausted whilst loading URL file");
		}

		/* load the non-corrupt data */
		for (i = 0; i < urls; i++) {
			struct path_data *p = NULL;
			char scheme[64], ports[10];
			char url[64 + 3 + 256 + 6 + 4096 + 1];
			unsigned int port;
			bool is_file = false;
			nsurl *nsurl;
			lwc_string *scheme_lwc, *fragment_lwc;
			char *path_query;
			size_t len;

			if (!fgets(scheme, sizeof scheme, fp))
				break;
			length = strlen(scheme) - 1;
			scheme[length] = '\0';

			if (!fgets(ports, sizeof ports, fp))
				break;
			length = strlen(ports) - 1;
			ports[length] = '\0';
			port = atoi(ports);

			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;
			length = strlen(s) - 1;
			s[length] = '\0';

			if (!strcasecmp(host, "localhost") &&
					!strcasecmp(scheme, "file"))
				is_file = true;

			snprintf(url, sizeof url, "%s://%s%s%s%s",
					scheme,
					/* file URLs have no host */
					(is_file ? "" : host),
					(port ? ":" : ""),
					(port ? ports : ""),
					s);

			/* TODO: store URLs in pre-parsed state, and make
			 *       a nsurl_load to generate the nsurl more
			 *       swiftly.
			 *       Need a nsurl_save too.
			 */
			if (nsurl_create(url, &nsurl) != NSERROR_OK) {
				LOG(("Failed inserting '%s'", url));
				die("Memory exhausted whilst loading "
						"URL file");
			}
                        
			if (url_bloom != NULL) {
				uint32_t hash = nsurl_hash(nsurl);
				bloom_insert_hash(url_bloom, hash);
			}

			/* Copy and merge path/query strings */
			if (nsurl_get(nsurl, NSURL_PATH | NSURL_QUERY,
					&path_query, &len) != NSERROR_OK) {
				LOG(("Failed inserting '%s'", url));
				die("Memory exhausted whilst loading "
						"URL file");
			}

			scheme_lwc = nsurl_get_component(nsurl, NSURL_SCHEME);
			fragment_lwc = nsurl_get_component(nsurl,
					NSURL_FRAGMENT);
			p = urldb_add_path(scheme_lwc, port, h, path_query,
					fragment_lwc, nsurl);
			if (!p) {
				LOG(("Failed inserting '%s'", url));
				die("Memory exhausted whilst loading "
						"URL file");
			}
			nsurl_unref(nsurl);
			lwc_string_unref(scheme_lwc);
			if (fragment_lwc != NULL)
				lwc_string_unref(fragment_lwc);

			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;
			if (p)
				p->urld.visits = (unsigned int)atoi(s);

			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;
			if (p)
				p->urld.last_visit = (time_t)atoi(s);

			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;
			if (p)
				p->urld.type = (content_type)atoi(s);

			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;


			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;
			length = strlen(s) - 1;
			if (p && length > 0) {
				s[length] = '\0';
				p->urld.title = malloc(length + 1);
				if (p->urld.title)
					memcpy(p->urld.title, s, length + 1);
			}
		}
	}

	fclose(fp);
	LOG(("Successfully loaded URL file"));
#undef MAXIMUM_URL_LENGTH
}

/**
 * Export the current database to file
 *
 * \param filename Name of file to export to
 */
void urldb_save(const char *filename)
{
	FILE *fp;
	int i;

	assert(filename);

	fp = fopen(filename, "w");
	if (!fp) {
		LOG(("Failed to open file '%s' for writing", filename));
		return;
	}

	/* file format version number */
	fprintf(fp, "%d\n", URL_FILE_VERSION);

	for (i = 0; i != NUM_SEARCH_TREES; i++) {
		urldb_save_search_tree(search_trees[i], fp);
	}

	fclose(fp);
}

/**
 * Save a search (sub)tree
 *
 * \param root Root of (sub)tree to save
 * \param fp File to write to
 */
void urldb_save_search_tree(struct search_node *parent, FILE *fp)
{
	char host[256];
	const struct host_part *h;
	unsigned int path_count = 0;
	char *path, *p, *end;
	int path_alloc = 64, path_used = 1;
	time_t expiry;

	expiry = time(NULL) - ((60 * 60 * 24) * nsoption_int(expire_url));

	if (parent == &empty)
		return;

	urldb_save_search_tree(parent->left, fp);

	path = malloc(path_alloc);
	if (!path)
		return;

	path[0] = '\0';

	for (h = parent->data, p = host, end = host + sizeof host;
			h && h != &db_root && p < end; h = h->parent) {
		int written = snprintf(p, end - p, "%s%s", h->part,
				(h->parent && h->parent->parent) ? "." : "");
		if (written < 0) {
			free(path);
			return;
		}
		p += written;
	}

	urldb_count_urls(&parent->data->paths, expiry, &path_count);

	if (path_count > 0) {
		fprintf(fp, "%s\n%i\n", host, path_count);

		urldb_write_paths(&parent->data->paths, host, fp,
				&path, &path_alloc, &path_used, expiry);
	}

	free(path);

	urldb_save_search_tree(parent->right, fp);
}

/**
 * Count number of URLs associated with a host
 *
 * \param root Root of path data tree
 * \param expiry Expiry time for URLs
 * \param count Pointer to count
 */
void urldb_count_urls(const struct path_data *root, time_t expiry,
		unsigned int *count)
{
	const struct path_data *p = root;

	do {
		if (p->children != NULL) {
			/* Drill down into children */
			p = p->children;
		} else {
			/* No more children, increment count if required */
			if (p->persistent || ((p->urld.last_visit > expiry) &&
					(p->urld.visits > 0)))
				(*count)++;

			/* Now, find next node to process. */
			while (p != root) {
				if (p->next != NULL) {
					/* Have a sibling, process that */
					p = p->next;
					break;
				}

				/* Ascend tree */
				p = p->parent;
			}
		}
	} while (p != root);
}

/**
 * Write paths associated with a host
 *
 * \param parent Root of (sub)tree to write
 * \param host Current host name
 * \param fp File to write to
 * \param path Current path string
 * \param path_alloc Allocated size of path
 * \param path_used Used size of path
 * \param expiry Expiry time of URLs
 */
void urldb_write_paths(const struct path_data *parent, const char *host,
		FILE *fp, char **path, int *path_alloc, int *path_used,
		time_t expiry)
{
	const struct path_data *p = parent;
	int i;

	do {
		int seglen = p->segment != NULL ? strlen(p->segment) : 0;
		int len = *path_used + seglen + 1;

		if (*path_alloc < len) {
			char *temp = realloc(*path,
					(len > 64) ? len : *path_alloc + 64);
			if (!temp)
				return;
			*path = temp;
			*path_alloc = (len > 64) ? len : *path_alloc + 64;
		}

		if (p->segment != NULL)
			memcpy(*path + *path_used - 1, p->segment, seglen);

		if (p->children != NULL) {
			(*path)[*path_used + seglen - 1] = '/';
			(*path)[*path_used + seglen] = '\0';
		} else {
			(*path)[*path_used + seglen - 1] = '\0';
			len -= 1;
		}

		*path_used = len;

		if (p->children != NULL) {
			/* Drill down into children */
			p = p->children;
		} else {
			/* leaf node */
			if (p->persistent ||((p->urld.last_visit > expiry) &&
					(p->urld.visits > 0))) {
				fprintf(fp, "%s\n", lwc_string_data(p->scheme));

				if (p->port)
					fprintf(fp,"%d\n", p->port);
				else
					fprintf(fp, "\n");

				fprintf(fp, "%s\n", *path);

				/** \todo handle fragments? */

				fprintf(fp, "%i\n%i\n%i\n", p->urld.visits,
						(int)p->urld.last_visit,
						(int)p->urld.type);

				fprintf(fp, "\n");

				if (p->urld.title) {
					uint8_t *s = (uint8_t *) p->urld.title;

					for (i = 0; s[i] != '\0'; i++)
						if (s[i] < 32)
							s[i] = ' ';
					for (--i; ((i > 0) && (s[i] == ' ')); 
							i--)
						s[i] = '\0';
					fprintf(fp, "%s\n", p->urld.title);
				} else
					fprintf(fp, "\n");
			}

			/* Now, find next node to process. */
			while (p != parent) {
				seglen = p->segment != NULL 
						? strlen(p->segment) : 0;

				/* Remove our segment from the path */
				*path_used -= seglen;
				(*path)[*path_used - 1] = '\0';

				if (p->next != NULL) {
					/* Have a sibling, process that */
					p = p->next;
					break;
				}

				/* Going up, so remove '/' */
				*path_used -= 1;
				(*path)[*path_used - 1] = '\0';

				/* Ascend tree */
				p = p->parent;
			}
		}
	} while (p != parent);
}

/**
 * Set the cross-session persistence of the entry for an URL
 *
 * \param url Absolute URL to persist
 * \param persist True to persist, false otherwise
 */
void urldb_set_url_persistence(nsurl *url, bool persist)
{
	struct path_data *p;

	assert(url);

	p = urldb_find_url(url);
	if (!p)
		return;

	p->persistent = persist;
}

/**
 * Insert an URL into the database
 *
 * \param url Absolute URL to insert
 * \return true on success, false otherwise
 */
bool urldb_add_url(nsurl *url)
{
	struct host_part *h;
	struct path_data *p;
	lwc_string *scheme;
	lwc_string *port;
	lwc_string *host;
	lwc_string *fragment;
	const char *host_str;
	char *path_query = NULL;
	size_t len;
	bool match;
	unsigned int port_int;

	assert(url);
        
        if (url_bloom == NULL)
                url_bloom = bloom_create(BLOOM_SIZE);
        
        if (url_bloom != NULL) {
                uint32_t hash = nsurl_hash(url);
                bloom_insert_hash(url_bloom, hash);
        }

	/* Copy and merge path/query strings */
	if (nsurl_get(url, NSURL_PATH | NSURL_QUERY, &path_query, &len) !=
			NSERROR_OK) {
		return false;
	}
	assert(path_query != NULL);

	scheme = nsurl_get_component(url, NSURL_SCHEME);
	if (scheme == NULL) {
		free(path_query);
		return false;
	}

	host = nsurl_get_component(url, NSURL_HOST);
	if (host != NULL) {
		host_str = lwc_string_data(host);
		lwc_string_unref(host);

	} else if (lwc_string_isequal(scheme, corestring_lwc_file, &match) ==
			lwc_error_ok && match == true) {
		host_str = "localhost";

	} else {
		lwc_string_unref(scheme);
		free(path_query);
		return false;
	}

	fragment = nsurl_get_component(url, NSURL_FRAGMENT);

	port = nsurl_get_component(url, NSURL_PORT);
	if (port != NULL) {
		port_int = atoi(lwc_string_data(port));
		lwc_string_unref(port);
	} else {
		port_int = 0;
	}

	/* Get host entry */
	h = urldb_add_host(host_str);

	/* Get path entry */
	p = (h != NULL) ? urldb_add_path(scheme, port_int, h, path_query,
			fragment, url) : NULL;

	lwc_string_unref(scheme);
	if (fragment != NULL)
		lwc_string_unref(fragment);

	return (p != NULL);
}

/**
 * Set an URL's title string, replacing any existing one
 *
 * \param url The URL to look for
 * \param title The title string to use (copied)
 */
void urldb_set_url_title(nsurl *url, const char *title)
{
	struct path_data *p;
	char *temp;

	assert(url && title);

	p = urldb_find_url(url);
	if (!p)
		return;

	temp = strdup(title);
	if (!temp)
		return;

	free(p->urld.title);
	p->urld.title = temp;
}

/**
 * Set an URL's content type
 *
 * \param url The URL to look for
 * \param type The type to set
 */
void urldb_set_url_content_type(nsurl *url, content_type type)
{
	struct path_data *p;

	assert(url);

	p = urldb_find_url(url);
	if (!p)
		return;

	p->urld.type = type;
}

/**
 * Update an URL's visit data
 *
 * \param url The URL to update
 */
void urldb_update_url_visit_data(nsurl *url)
{
	struct path_data *p;

	assert(url);

	p = urldb_find_url(url);
	if (!p)
		return;

	p->urld.last_visit = time(NULL);
	p->urld.visits++;
}

/**
 * Reset an URL's visit statistics
 *
 * \param url The URL to reset
 */
void urldb_reset_url_visit_data(nsurl *url)
{
	struct path_data *p;

	assert(url);

	p = urldb_find_url(url);
	if (!p)
		return;

	p->urld.last_visit = (time_t)0;
	p->urld.visits = 0;
}


/**
 * Find data for an URL.
 *
 * \param url Absolute URL to look for
 * \return Pointer to result struct, or NULL
 */
const struct url_data *urldb_get_url_data(nsurl *url)
{
	struct path_data *p;
	struct url_internal_data *u;

	assert(url);

	p = urldb_find_url(url);
	if (!p)
		return NULL;

	u = &p->urld;

	return (const struct url_data *) u;
}

/**
 * Extract an URL from the db
 *
 * \param url URL to extract
 * \return Pointer to database's copy of URL or NULL if not found
 */
nsurl *urldb_get_url(nsurl *url)
{
	struct path_data *p;

	assert(url);

	p = urldb_find_url(url);
	if (!p)
		return NULL;

	return p->url;
}

/**
 * Look up authentication details in database
 *
 * \param url Absolute URL to search for
 * \param realm When non-NULL, it is realm which can be used to determine
 * the protection space when that's not been done before for given URL.
 * \return Pointer to authentication details, or NULL if not found
 */
const char *urldb_get_auth_details(nsurl *url, const char *realm)
{
	struct path_data *p, *p_cur, *p_top;

	assert(url);

	/* add to the db, so our lookup will work */
	urldb_add_url(url);

	p = urldb_find_url(url);
	if (!p)
		return NULL;

	/* Check for any auth details attached to the path_data node or any of
	 * its parents. */
	for (p_cur = p; p_cur != NULL; p_top = p_cur, p_cur = p_cur->parent) {
		if (p_cur->prot_space) {
			return p_cur->prot_space->auth;
		}
	}

	/* Only when we have a realm (and canonical root of given URL), we can
	 * uniquely locate the protection space. */
	if (realm != NULL) {
		const struct host_part *h = (const struct host_part *)p_top;
		const struct prot_space_data *space;
		bool match;

		/* Search for a possible matching protection space. */
		for (space = h->prot_space; space != NULL;
				space = space->next) {
			if (!strcmp(space->realm, realm) &&
					lwc_string_isequal(space->scheme,
							p->scheme, &match) ==
							lwc_error_ok &&
					match == true &&
					space->port == p->port) {
				p->prot_space = space;
				return p->prot_space->auth;
			}
		}
	}

	return NULL;
}

/**
 * Retrieve certificate verification permissions from database
 *
 * \param url Absolute URL to search for
 * \return true to permit connections to hosts with invalid certificates,
 * false otherwise.
 */
bool urldb_get_cert_permissions(nsurl *url)
{
	struct path_data *p;
	const struct host_part *h;

	assert(url);

	p = urldb_find_url(url);
	if (!p)
		return false;

	for (; p && p->parent; p = p->parent)
		/* do nothing */;
	assert(p);

	h = (const struct host_part *)p;

	return h->permit_invalid_certs;
}

/**
 * Set authentication data for an URL
 *
 * \param url The URL to consider
 * \param realm The authentication realm
 * \param auth The authentication details (in form username:password)
 */
void urldb_set_auth_details(nsurl *url, const char *realm,
		const char *auth)
{
	struct path_data *p, *pi;
	struct host_part *h;
	struct prot_space_data *space, *space_alloc;
	char *realm_alloc, *auth_alloc;
	bool match;

	assert(url && realm && auth);

	/* add url, in case it's missing */
	urldb_add_url(url);

	p = urldb_find_url(url);

	if (!p)
		return;

	/* Search for host_part */
	for (pi = p; pi->parent != NULL; pi = pi->parent)
		;
	h = (struct host_part *)pi;

	/* Search if given URL belongs to a protection space we already know of. */
	for (space = h->prot_space; space; space = space->next) {
		if (!strcmp(space->realm, realm) &&
				lwc_string_isequal(space->scheme, p->scheme,
						&match) == lwc_error_ok &&
				match == true &&
				space->port == p->port)
			break;
	}

	if (space != NULL) {
		/* Overrule existing auth. */
		free(space->auth);
		space->auth = strdup(auth);
	} else {
		/* Create a new protection space. */
		space = space_alloc = malloc(sizeof(struct prot_space_data));
		realm_alloc = strdup(realm);
		auth_alloc = strdup(auth);

		if (!space_alloc || !realm_alloc || !auth_alloc) {
			free(space_alloc);
			free(realm_alloc);
			free(auth_alloc);
			return;
		}

		space->scheme = lwc_string_ref(p->scheme);
		space->port = p->port;
		space->realm = realm_alloc;
		space->auth = auth_alloc;
		space->next = h->prot_space;
		h->prot_space = space;
	}

	p->prot_space = space;
}

/**
 * Set certificate verification permissions
 *
 * \param url URL to consider
 * \param permit Set to true to allow invalid certificates
 */
void urldb_set_cert_permissions(nsurl *url, bool permit)
{
	struct path_data *p;
	struct host_part *h;

	assert(url);

	/* add url, in case it's missing */
	urldb_add_url(url);

	p = urldb_find_url(url);
	if (!p)
		return;

	for (; p && p->parent; p = p->parent)
		/* do nothing */;
	assert(p);

	h = (struct host_part *)p;

	h->permit_invalid_certs = permit;
}

/**
 * Set thumbnail for url, replacing any existing thumbnail
 *
 * \param url Absolute URL to consider
 * \param bitmap Opaque pointer to thumbnail data, or NULL to invalidate
 */
void urldb_set_thumbnail(nsurl *url, struct bitmap *bitmap)
{
	struct path_data *p;

	assert(url);

	p = urldb_find_url(url);
	if (!p)
		return;

	if (p->thumb && p->thumb != bitmap)
		bitmap_destroy(p->thumb);

	p->thumb = bitmap;
}

/**
 * Retrieve thumbnail data for given URL
 *
 * \param url Absolute URL to search for
 * \return Pointer to thumbnail data, or NULL if not found.
 */
struct bitmap *urldb_get_thumbnail(nsurl *url)
{
	struct path_data *p;

	assert(url);

	p = urldb_find_url(url);
	if (!p)
		return NULL;

	return p->thumb;
}

/**
 * Iterate over entries in the database which match the given prefix
 *
 * \param prefix Prefix to match
 * \param callback Callback function
 */
void urldb_iterate_partial(const char *prefix,
		bool (*callback)(nsurl *url,
		const struct url_data *data))
{
	char host[256];
	char buf[260]; /* max domain + "www." */
	const char *slash, *scheme_sep;
	struct search_node *tree;
	const struct host_part *h;

	assert(prefix && callback);

	/* strip scheme */
	scheme_sep = strstr(prefix, "://");
	if (scheme_sep)
		prefix = scheme_sep + 3;

	slash = strchr(prefix, '/');
	tree = urldb_get_search_tree(prefix);

	if (slash) {
		/* if there's a slash in the input, then we can
		 * assume that we're looking for a path */
		snprintf(host, sizeof host, "%.*s",
				(int) (slash - prefix), prefix);

		h = urldb_search_find(tree, host);
		if (!h) {
			int len = slash - prefix;

			if (len <= 3 || strncasecmp(host, "www.", 4) != 0) {
				snprintf(buf, sizeof buf, "www.%s", host);
				h = urldb_search_find(
					search_trees[ST_DN + 'w' - 'a'],
					buf);
				if (!h)
					return;
			} else
				return;
		}

		if (h->paths.children) {
			/* Have paths, iterate them */
			urldb_iterate_partial_path(&h->paths, slash + 1,
					callback);
		}

	} else {
		int len = strlen(prefix);

		/* looking for hosts */
		if (!urldb_iterate_partial_host(tree, prefix, callback))
			return;

		if (len <= 3 || strncasecmp(prefix, "www.", 4) != 0) {
			/* now look for www.prefix */
			snprintf(buf, sizeof buf, "www.%s", prefix);
			if(!urldb_iterate_partial_host(
					search_trees[ST_DN + 'w' - 'a'],
					buf, callback))
				return;
		}
	}
}

/**
 * Partial host iterator (internal)
 *
 * \param root Root of (sub)tree to traverse
 * \param prefix Prefix to match
 * \param callback Callback function
 * \return true to continue, false otherwise
 */
bool urldb_iterate_partial_host(struct search_node *root, const char *prefix,
		bool (*callback)(nsurl *url, const struct url_data *data))
{
	int c;

	assert(root && prefix && callback);

	if (root == &empty)
		return true;

	c = urldb_search_match_prefix(root->data, prefix);

	if (c > 0)
		/* No match => look in left subtree */
		return urldb_iterate_partial_host(root->left, prefix,
				callback);
	else if (c < 0)
		/* No match => look in right subtree */
		return urldb_iterate_partial_host(root->right, prefix,
				callback);
	else {
		/* Match => iterate over l/r subtrees & process this node */
		if (!urldb_iterate_partial_host(root->left, prefix,
				callback))
			return false;

		if (root->data->paths.children) {
			/* and extract all paths attached to this host */
			if (!urldb_iterate_entries_path(&root->data->paths,
					callback, NULL)) {
				return false;
			}
		}

		if (!urldb_iterate_partial_host(root->right, prefix,
				callback))
			return false;
	}

	return true;
}

/**
 * Partial path iterator (internal)
 *
 * \param parent Root of (sub)tree to traverse
 * \param prefix Prefix to match
 * \param callback Callback function
 * \return true to continue, false otherwise
 */
bool urldb_iterate_partial_path(const struct path_data *parent,
		const char *prefix, bool (*callback)(nsurl *url,
		const struct url_data *data))
{
	const struct path_data *p = parent->children;
	const char *slash, *end = prefix + strlen(prefix);

	/* 
	 * Given: http://www.example.org/a/b/c/d//e
	 * and assuming a path tree:
	 *     .
	 *    / \
	 *   a1 b1
	 *  / \
	 * a2 b2
	 *    /|\
	 *   a b c
	 *   3 3 |
	 *       d
	 *       |
	 *       e
	 *      / \
	 *      f g
	 *
	 * Prefix will be:	p will be:
	 *
	 * a/b/c/d//e		a1
	 *   b/c/d//e		a2
	 *   b/c/d//e		b3
	 *     c/d//e		a3
	 *     c/d//e		b3
	 *     c/d//e		c
	 *       d//e		d
	 *         /e		e		(skip /)
	 *          e		e
	 *
	 * I.E. we perform a breadth-first search of the tree.
	 */

	do {
		slash = strchr(prefix, '/');
		if (!slash)
			slash = end;

		if (slash == prefix && *prefix == '/') {
			/* Ignore "//" */
			prefix++;
			continue;
		}
	
		if (strncasecmp(p->segment, prefix, slash - prefix) == 0) {
			/* prefix matches so far */
			if (slash == end) {
				/* we've run out of prefix, so all
				 * paths below this one match */
				if (!urldb_iterate_entries_path(p, callback, 
						NULL))
					return false;

				/* Progress to next sibling */
				p = p->next;
			} else {
				/* Skip over this segment */
				prefix = slash + 1;

				p = p->children;
			}
		} else {
			/* Doesn't match this segment, try next sibling */
			p = p->next;
		}
	} while (p != NULL);

	return true;
}

/**
 * Iterate over all entries in database
 *
 * \param callback Function to callback for each entry
 */
void urldb_iterate_entries(bool (*callback)(nsurl *url,
		const struct url_data *data))
{
	int i;

	assert(callback);

	for (i = 0; i < NUM_SEARCH_TREES; i++) {
		if (!urldb_iterate_entries_host(search_trees[i],
				callback, NULL))
			break;
	}
}

/**
 * Iterate over all cookies in database
 *
 * \param callback Function to callback for each entry
 */
void urldb_iterate_cookies(bool (*callback)(const struct cookie_data *data))
{
	int i;

	assert(callback);

	for (i = 0; i < NUM_SEARCH_TREES; i++) {
		if (!urldb_iterate_entries_host(search_trees[i],
				NULL, callback))
			break;
	}
}

/**
 * Host data iterator (internal)
 *
 * \param parent Root of subtree to iterate over
 * \param url_callback Callback function
 * \param cookie_callback Callback function
 * \return true to continue, false otherwise
 */
bool urldb_iterate_entries_host(struct search_node *parent,
		bool (*url_callback)(nsurl *url,
				const struct url_data *data),
		bool (*cookie_callback)(const struct cookie_data *data))
{
	if (parent == &empty)
		return true;

	if (!urldb_iterate_entries_host(parent->left,
			url_callback, cookie_callback))
		return false;

	if ((parent->data->paths.children) || ((cookie_callback) &&
			(parent->data->paths.cookies))) {
		/* We have paths (or domain cookies), so iterate them */
		if (!urldb_iterate_entries_path(&parent->data->paths,
				url_callback, cookie_callback)) {
			return false;
		}
	}

	if (!urldb_iterate_entries_host(parent->right,
			url_callback, cookie_callback))
		return false;

	return true;
}

/**
 * Path data iterator (internal)
 *
 * \param parent Root of subtree to iterate over
 * \param url_callback Callback function
 * \param cookie_callback Callback function
 * \return true to continue, false otherwise
 */
bool urldb_iterate_entries_path(const struct path_data *parent,
		bool (*url_callback)(nsurl *url,
				const struct url_data *data),
		bool (*cookie_callback)(const struct cookie_data *data))
{
	const struct path_data *p = parent;
	const struct cookie_data *c;
	
	do {
		if (p->children != NULL) {
			/* Drill down into children */
			p = p->children;
		} else {
			/* All leaf nodes in the path tree should have an URL or
			 * cookies attached to them. If this is not the case, it
			 * indicates that there's a bug in the file loader/URL
			 * insertion code. Therefore, assert this here. */
			assert(url_callback || cookie_callback);

			/** \todo handle fragments? */
			if (url_callback) {
				const struct url_internal_data *u = &p->urld;

				assert(p->url);

				if (!url_callback(p->url,
						(const struct url_data *) u))
					return false;
			} else {
				c = (const struct cookie_data *)p->cookies;
				for (; c != NULL; c = c->next)
					if (!cookie_callback(c))
						return false;
			}

			/* Now, find next node to process. */
			while (p != parent) {
				if (p->next != NULL) {
					/* Have a sibling, process that */
					p = p->next;
					break;
				}

				/* Ascend tree */
				p = p->parent;
			}
		}
	} while (p != parent);

	return true;
}

/**
 * Add a host node to the tree
 *
 * \param part Host segment to add (or whole IP address) (copied)
 * \param parent Parent node to add to
 * \return Pointer to added node, or NULL on memory exhaustion
 */
struct host_part *urldb_add_host_node(const char *part,
		struct host_part *parent)
{
	struct host_part *d;

	assert(part && parent);

	d = calloc(1, sizeof(struct host_part));
	if (!d)
		return NULL;

	d->part = strdup(part);
	if (!d->part) {
		free(d);
		return NULL;
	}

	d->next = parent->children;
	if (parent->children)
		parent->children->prev = d;
	d->parent = parent;
	parent->children = d;

	return d;
}

/**
 * Add a host to the database, creating any intermediate entries
 *
 * \param host Hostname to add
 * \return Pointer to leaf node, or NULL on memory exhaustion
 */
struct host_part *urldb_add_host(const char *host)
{
	struct host_part *d = (struct host_part *) &db_root, *e;
	struct search_node *s;
	char buf[256]; /* 256 bytes is sufficient - domain names are
			* limited to 255 chars. */
	char *part;

	assert(host);

	if (url_host_is_ip_address(host)) {
		/* Host is an IP, so simply add as TLD */

		/* Check for existing entry */
		for (e = d->children; e; e = e->next)
			if (strcasecmp(host, e->part) == 0)
				/* found => return it */
				return e;

		d = urldb_add_host_node(host, d);

		s = urldb_search_insert(search_trees[ST_IP], d);
		if (!s) {
			/* failed */
			d = NULL;
		} else {
			search_trees[ST_IP] = s;
		}

		return d;
	}

	/* Copy host string, so we can corrupt it */
	strncpy(buf, host, sizeof buf);
	buf[sizeof buf - 1] = '\0';

	/* Process FQDN segments backwards */
	do {
		part = strrchr(buf, '.');
		if (!part) {
			/* last segment */
			/* Check for existing entry */
			for (e = d->children; e; e = e->next)
				if (strcasecmp(buf, e->part) == 0)
					break;

			if (e) {
				d = e;
			} else {
				d = urldb_add_host_node(buf, d);
			}

			/* And insert into search tree */
			if (d) {
				struct search_node **r;

				r = urldb_get_search_tree_direct(buf);
				s = urldb_search_insert(*r, d);
				if (!s) {
					/* failed */
					d = NULL;
				} else {
					*r = s;
				}
			}
			break;
		}

		/* Check for existing entry */
		for (e = d->children; e; e = e->next)
			if (strcasecmp(part + 1, e->part) == 0)
				break;

		d = e ? e : urldb_add_host_node(part + 1, d);
		if (!d)
			break;

		*part = '\0';
	} while (1);

	return d;
}

/**
 * Add a path node to the tree
 *
 * \param scheme URL scheme associated with path (copied)
 * \param port Port number on host associated with path
 * \param segment Path segment to add (copied)
 * \param fragment URL fragment (copied), or NULL
 * \param parent Parent node to add to
 * \return Pointer to added node, or NULL on memory exhaustion
 */
struct path_data *urldb_add_path_node(lwc_string *scheme, unsigned int port,
		const char *segment, lwc_string *fragment,
		struct path_data *parent)
{
	struct path_data *d, *e;

	assert(scheme && segment && parent);

	d = calloc(1, sizeof(struct path_data));
	if (!d)
		return NULL;

	d->scheme = lwc_string_ref(scheme);

	d->port = port;

	d->segment = strdup(segment);
	if (!d->segment) {
		lwc_string_unref(d->scheme);
		free(d);
		return NULL;
	}

	if (fragment) {
		if (!urldb_add_path_fragment(d, fragment)) {
			free(d->segment);
			lwc_string_unref(d->scheme);
			free(d);
			return NULL;
		}
	}

	for (e = parent->children; e; e = e->next)
		if (strcmp(e->segment, d->segment) > 0)
			break;

	if (e) {
		d->prev = e->prev;
		d->next = e;
		if (e->prev)
			e->prev->next = d;
		else
			parent->children = d;
		e->prev = d;
	} else if (!parent->children) {
		d->prev = d->next = NULL;
		parent->children = parent->last = d;
	} else {
		d->next = NULL;
		d->prev = parent->last;
		parent->last->next = d;
		parent->last = d;
	}
	d->parent = parent;

	return d;
}

/**
 * Add a path to the database, creating any intermediate entries
 *
 * \param scheme URL scheme associated with path
 * \param port Port number on host associated with path
 * \param host Host tree node to attach to
 * \param path_query Absolute path plus query to add (freed)
 * \param fragment URL fragment, or NULL
 * \param url URL (fragment ignored)
 * \return Pointer to leaf node, or NULL on memory exhaustion
 */
struct path_data *urldb_add_path(lwc_string *scheme, unsigned int port,
		const struct host_part *host, char *path_query,
		lwc_string *fragment, nsurl *url)
{
	struct path_data *d, *e;
	char *buf = path_query;
	char *segment, *slash;
	bool match;

	assert(scheme && host && url);

	d = (struct path_data *) &host->paths;

	/* skip leading '/' */
	segment = buf;
	if (*segment == '/')
		segment++;

	/* Process path segments */
	do {
		slash = strchr(segment, '/');
		if (!slash) {
			/* last segment */
			/* look for existing entry */
			for (e = d->children; e; e = e->next)
				if (strcmp(segment, e->segment) == 0 &&
						lwc_string_isequal(scheme,
						e->scheme, &match) ==
						lwc_error_ok &&
						match == true &&
						e->port == port)
					break;

			d = e ? urldb_add_path_fragment(e, fragment) :
					urldb_add_path_node(scheme, port,
					segment, fragment, d);
			break;
		}

		*slash = '\0';

		/* look for existing entry */
		for (e = d->children; e; e = e->next)
			if (strcmp(segment, e->segment) == 0 &&
					lwc_string_isequal(scheme, e->scheme,
						&match) == lwc_error_ok &&
						match == true &&
					e->port == port)
				break;

		d = e ? e : urldb_add_path_node(scheme, port, segment, NULL, d);
		if (!d)
			break;

		segment = slash + 1;
	} while (1);

	free(path_query);

	if (d && !d->url) {
		/* Insert defragmented URL */
		if (nsurl_defragment(url, &d->url) != NSERROR_OK)
			return NULL;
	}

	return d;
}

/**
 * Fragment comparator callback for qsort
 */
int urldb_add_path_fragment_cmp(const void *a, const void *b)
{
	return strcasecmp(*((const char **) a), *((const char **) b));
}

/**
 * Add a fragment to a path segment
 *
 * \param segment Path segment to add to
 * \param fragment Fragment to add (copied), or NULL
 * \return segment or NULL on memory exhaustion
 */
struct path_data *urldb_add_path_fragment(struct path_data *segment,
		lwc_string *fragment)
{
	char **temp;

	assert(segment);

	/* If no fragment, this function is a NOP
	 * This may seem strange, but it makes the rest
	 * of the code cleaner */
	if (!fragment)
		return segment;

	temp = realloc(segment->fragment,
			(segment->frag_cnt + 1) * sizeof(char *));
	if (!temp)
		return NULL;

	segment->fragment = temp;
	segment->fragment[segment->frag_cnt] =
			strdup(lwc_string_data(fragment));
	if (!segment->fragment[segment->frag_cnt]) {
		/* Don't free temp - it's now our buffer */
		return NULL;
	}

	segment->frag_cnt++;

	/* We want fragments in alphabetical order, so sort them
	 * It may prove better to insert in alphabetical order instead */
	qsort(segment->fragment, segment->frag_cnt, sizeof (char *),
			urldb_add_path_fragment_cmp);

	return segment;
}

/**
 * Find an URL in the database
 *
 * \param url Absolute URL to find
 * \return Pointer to path data, or NULL if not found
 */
struct path_data *urldb_find_url(nsurl *url)
{
	const struct host_part *h;
	struct path_data *p;
	struct search_node *tree;
	char *plq;
	const char *host_str;
	lwc_string *scheme, *host, *port;
	size_t len = 0;
	unsigned int port_int;
	bool match;

	assert(url);
        
	if (url_bloom != NULL) {
		if (bloom_search_hash(url_bloom,
					nsurl_hash(url)) == false) {
					return NULL;
		}
	}

	scheme = nsurl_get_component(url, NSURL_SCHEME);
	if (scheme == NULL)
		return NULL;

	host = nsurl_get_component(url, NSURL_HOST);
	if (host != NULL) {
		host_str = lwc_string_data(host);
		lwc_string_unref(host);

	} else if (lwc_string_isequal(scheme, corestring_lwc_file, &match) ==
			lwc_error_ok && match == true) {
		host_str = "localhost";

	} else {
		lwc_string_unref(scheme);
		return NULL;
	}

	tree = urldb_get_search_tree(host_str);
	h = urldb_search_find(tree, host_str);
	if (!h) {
		lwc_string_unref(scheme);
		return NULL;
	}

	/* generate plq (path, leaf, query) */
	if (nsurl_get(url, NSURL_PATH | NSURL_QUERY, &plq, &len) !=
			NSERROR_OK) {
		lwc_string_unref(scheme);
		return NULL;
	}

	/* Get port */
	port = nsurl_get_component(url, NSURL_PORT);
	if (port != NULL) {
		port_int = atoi(lwc_string_data(port));
		lwc_string_unref(port);
	} else {
		port_int = 0;
	}

	p = urldb_match_path(&h->paths, plq, scheme, port_int);

	free(plq);
	lwc_string_unref(scheme);

	return p;
}

/**
 * Match a path string
 *
 * \param parent Path (sub)tree to look in
 * \param path The path to search for
 * \param scheme The URL scheme associated with the path
 * \param port The port associated with the path
 * \return Pointer to path data or NULL if not found.
 */
struct path_data *urldb_match_path(const struct path_data *parent,
		const char *path, lwc_string *scheme, unsigned short port)
{
	const struct path_data *p;
	const char *slash;
	bool match;

	assert(parent != NULL);
	assert(parent->segment == NULL);
	assert(path[0] == '/');

	/* Start with children, as parent has no segment */
	p = parent->children;

	while (p != NULL) {
		slash = strchr(path + 1, '/');
		if (!slash)
			slash = path + strlen(path);

		if (strncmp(p->segment, path + 1, slash - path - 1) == 0 &&
				lwc_string_isequal(p->scheme, scheme, &match) ==
						lwc_error_ok &&
				match == true &&
				p->port == port) {
			if (*slash == '\0') {
				/* Complete match */
				return (struct path_data *) p;
			}

			/* Match so far, go down tree */
			p = p->children;

			path = slash;
		} else {
			/* No match, try next sibling */
			p = p->next;
		}
	}

	return NULL;
}

/**
 * Get the search tree for a particular host
 *
 * \param host  the host to lookup
 * \return the corresponding search tree
 */
struct search_node **urldb_get_search_tree_direct(const char *host) {
	assert(host);

	if (url_host_is_ip_address(host))
		return &search_trees[ST_IP];
	else if (isalpha(*host))
		return &search_trees[ST_DN + tolower(*host) - 'a'];
	return &search_trees[ST_EE];
}

/**
 * Get the search tree for a particular host
 *
 * \param host  the host to lookup
 * \return the corresponding search tree
 */
struct search_node *urldb_get_search_tree(const char *host) {
  	return *urldb_get_search_tree_direct(host);
}

/**
 * Dump URL database to stderr
 */
void urldb_dump(void)
{
	int i;

	urldb_dump_hosts(&db_root);

	for (i = 0; i != NUM_SEARCH_TREES; i++)
		urldb_dump_search(search_trees[i], 0);
}

/**
 * Dump URL database hosts to stderr
 *
 * \param parent Parent node of tree to dump
 */
void urldb_dump_hosts(struct host_part *parent)
{
	struct host_part *h;

	if (parent->part) {
		LOG(("%s", parent->part));

		LOG(("\t%s invalid SSL certs",
			parent->permit_invalid_certs ? "Permits" : "Denies"));
	}

	/* Dump path data */
	urldb_dump_paths(&parent->paths);

	/* and recurse */
	for (h = parent->children; h; h = h->next)
		urldb_dump_hosts(h);
}

/**
 * Dump URL database paths to stderr
 *
 * \param parent Parent node of tree to dump
 */
void urldb_dump_paths(struct path_data *parent)
{
	const struct path_data *p = parent;
	unsigned int i;

	do {
		if (p->segment != NULL) {
			LOG(("\t%s : %u", lwc_string_data(p->scheme), p->port));

			LOG(("\t\t'%s'", p->segment));

			for (i = 0; i != p->frag_cnt; i++)
				LOG(("\t\t\t#%s", p->fragment[i]));
		}

		if (p->children != NULL) {
			p = p->children;
		} else {
			while (p != parent) {
				if (p->next != NULL) {
					p = p->next;
					break;
				}

				p = p->parent;
			}
		}
	} while (p != parent);
}

/**
 * Dump search tree
 *
 * \param parent Parent node of tree to dump
 * \param depth Tree depth
 */
void urldb_dump_search(struct search_node *parent, int depth)
{
	const struct host_part *h;
	int i;

	if (parent == &empty)
		return;

	urldb_dump_search(parent->left, depth + 1);

	for (i = 0; i != depth; i++)
			fputc(' ', stderr);

	for (h = parent->data; h; h = h->parent) {
		if (h->part)
			fprintf(stderr, "%s", h->part);

		if (h->parent && h->parent->parent)
			fputc('.', stderr);
	}

	fputc('\n', stderr);

	urldb_dump_search(parent->right, depth + 1);
}

/**
 * Insert a node into the search tree
 *
 * \param root Root of tree to insert into
 * \param data User data to insert
 * \return Pointer to updated root, or NULL if failed
 */
struct search_node *urldb_search_insert(struct search_node *root,
		const struct host_part *data)
{
	struct search_node *n;

	assert(root && data);

	n = malloc(sizeof(struct search_node));
	if (!n)
		return NULL;

	n->level = 1;
	n->data = data;
	n->left = n->right = &empty;

	root = urldb_search_insert_internal(root, n);

	return root;
}

/**
 * Insert node into search tree
 *
 * \param root Root of (sub)tree to insert into
 * \param n Node to insert
 * \return Pointer to updated root
 */
struct search_node *urldb_search_insert_internal(struct search_node *root,
		struct search_node *n)
{
	assert(root && n);

	if (root == &empty) {
		root = n;
	} else {
		int c = urldb_search_match_host(root->data, n->data);

		if (c > 0) {
			root->left = urldb_search_insert_internal(
					root->left, n);
		} else if (c < 0) {
			root->right = urldb_search_insert_internal(
					root->right, n);
		} else {
			/* exact match */
			free(n);
			return root;
		}

		root = urldb_search_skew(root);
		root = urldb_search_split(root);
	}

	return root;
}

/**
 * Find a node in a search tree
 *
 * \param root Tree to look in
 * \param host Host to find
 * \return Pointer to host tree node, or NULL if not found
 */
const struct host_part *urldb_search_find(struct search_node *root,
		const char *host)
{
	int c;

	assert(root && host);

	if (root == &empty) {
		return NULL;
	}

	c = urldb_search_match_string(root->data, host);

	if (c > 0)
		return urldb_search_find(root->left, host);
	else if (c < 0)
		return urldb_search_find(root->right, host);
	else
		return root->data;
}

/**
 * Compare a pair of host_parts
 *
 * \param a
 * \param b
 * \return 0 if match, non-zero, otherwise
 */
int urldb_search_match_host(const struct host_part *a,
		const struct host_part *b)
{
	int ret;

	assert(a && b);

	/* traverse up tree to root, comparing parts as we go. */
	for (; a && a != &db_root && b && b != &db_root;
			a = a->parent, b = b->parent)
		if ((ret = strcasecmp(a->part, b->part)) != 0)
			/* They differ => return the difference here */
			return ret;

	/* If we get here then either:
	 *    a) The path lengths differ
	 * or b) The hosts are identical
	 */
	if (a && a != &db_root && (!b || b == &db_root))
		/* len(a) > len(b) */
		return 1;
	else if ((!a || a == &db_root) && b && b != &db_root)
		/* len(a) < len(b) */
		return -1;

	/* identical */
	return 0;
}

/**
 * Compare host_part with a string
 *
 * \param a
 * \param b
 * \return 0 if match, non-zero, otherwise
 */
int urldb_search_match_string(const struct host_part *a,
		const char *b)
{
	const char *end, *dot;
	int plen, ret;

	assert(a && a != &db_root && b);

	if (url_host_is_ip_address(b)) {
		/* IP address */
		return strcasecmp(a->part, b);
	}

	end = b + strlen(b) + 1;

	while (b < end && a && a != &db_root) {
		dot = strchr(b, '.');
		if (!dot) {
			/* last segment */
			dot = end - 1;
		}

		/* Compare strings (length limited) */
		if ((ret = strncasecmp(a->part, b, dot - b)) != 0)
			/* didn't match => return difference */
			return ret;

		/* The strings matched, now check that the lengths do, too */
		plen = strlen(a->part);

		if (plen > dot - b)
			/* len(a) > len(b) */
			return 1;
		else if (plen < dot - b)
			/* len(a) < len(b) */
			return -1;

		b = dot + 1;
		a = a->parent;
	}

	/* If we get here then either:
	 *    a) The path lengths differ
	 * or b) The hosts are identical
	 */
	if (a && a != &db_root && b >= end)
		/* len(a) > len(b) */
		return 1;
	else if ((!a || a == &db_root) && b < end)
		/* len(a) < len(b) */
		return -1;

	/* Identical */
	return 0;
}

/**
 * Compare host_part with prefix
 *
 * \param a
 * \param b
 * \return 0 if match, non-zero, otherwise
 */
int urldb_search_match_prefix(const struct host_part *a,
		const char *b)
{
	const char *end, *dot;
	int plen, ret;

	assert(a && a != &db_root && b);

	if (url_host_is_ip_address(b)) {
		/* IP address */
		return strncasecmp(a->part, b, strlen(b));
	}

	end = b + strlen(b) + 1;

	while (b < end && a && a != &db_root) {
		dot = strchr(b, '.');
		if (!dot) {
			/* last segment */
			dot = end - 1;
		}

		/* Compare strings (length limited) */
		if ((ret = strncasecmp(a->part, b, dot - b)) != 0)
			/* didn't match => return difference */
			return ret;

		/* The strings matched */
		if (dot < end - 1) {
			/* Consider segment lengths only in the case
			 * where the prefix contains segments */
			plen = strlen(a->part);
			if (plen > dot - b)
				/* len(a) > len(b) */
				return 1;
			else if (plen < dot - b)
				/* len(a) < len(b) */
				return -1;
		}

		b = dot + 1;
		a = a->parent;
	}

	/* If we get here then either:
	 *    a) The path lengths differ
	 * or b) The hosts are identical
	 */
	if (a && a != &db_root && b >= end)
		/* len(a) > len(b) => prefix matches */
		return 0;
	else if ((!a || a == &db_root) && b < end)
		/* len(a) < len(b) => prefix does not match */
		return -1;

	/* Identical */
	return 0;
}

/**
 * Rotate a subtree right
 *
 * \param root Root of subtree to rotate
 * \return new root of subtree
 */
struct search_node *urldb_search_skew(struct search_node *root)
{
	struct search_node *temp;

	assert(root);

	if (root->left->level == root->level) {
		temp = root->left;
		root->left = temp->right;
		temp->right = root;
		root = temp;
	}

	return root;
}

/**
 * Rotate a node left, increasing the parent's level
 *
 * \param root Root of subtree to rotate
 * \return New root of subtree
 */
struct search_node *urldb_search_split(struct search_node *root)
{
	struct search_node *temp;

	assert(root);

	if (root->right->right->level == root->level) {
		temp = root->right;
		root->right = temp->left;
		temp->left = root;
		root = temp;

		root->level++;
	}

	return root;
}

/**
 * Retrieve cookies for an URL
 *
 * \param url URL being fetched
 * \param include_http_only Whether to include HTTP(S) only cookies.
 * \return Cookies string for libcurl (on heap), or NULL on error/no cookies
 */
char *urldb_get_cookie(nsurl *url, bool include_http_only)
{
	const struct path_data *p, *q;
	const struct host_part *h;
	lwc_string *path_lwc;
	struct cookie_internal_data *c;
	int count = 0, version = COOKIE_RFC2965;
	struct cookie_internal_data **matched_cookies;
	int matched_cookies_size = 20;
	int ret_alloc = 4096, ret_used = 1;
	const char *path;
	char *ret;
	lwc_string *scheme;
	time_t now;
	int i;
	bool match;

	assert(url != NULL);

	/* The URL must exist in the db in order to find relevant cookies, since
	 * we search up the tree from the URL node, and cookies from further
	 * up also apply. */
	urldb_add_url(url);

	p = urldb_find_url(url);
	if (!p)
		return NULL;

	scheme = p->scheme;

	matched_cookies = malloc(matched_cookies_size * 
			sizeof(struct cookie_internal_data *));
	if (!matched_cookies)
		return NULL;

#define GROW_MATCHED_COOKIES						\
	do {								\
		if (count == matched_cookies_size) {			\
			struct cookie_internal_data **temp;		\
			temp = realloc(matched_cookies,			\
				(matched_cookies_size + 20) *		\
				sizeof(struct cookie_internal_data *));	\
									\
			if (temp == NULL) {				\
				free(ret);				\
				free(matched_cookies);			\
				return NULL;				\
			}						\
									\
			matched_cookies = temp;				\
			matched_cookies_size += 20;			\
		}							\
	} while(0)

	ret = malloc(ret_alloc);
	if (!ret) {
		free(matched_cookies);
		return NULL;
	}

	ret[0] = '\0';

	path_lwc = nsurl_get_component(url, NSURL_PATH);
	if (path_lwc == NULL) {
		free(ret);
		free(matched_cookies);
		return NULL;
	}
	path = lwc_string_data(path_lwc);
	lwc_string_unref(path_lwc);

	now = time(NULL);

	if (*(p->segment) != '\0') {
		/* Match exact path, unless directory, when prefix matching
		 * will handle this case for us. */
		for (q = p->parent->children; q; q = q->next) {
			if (strcmp(q->segment, p->segment))
				continue;

			/* Consider all cookies associated with
			 * this exact path */
			for (c = q->cookies; c; c = c->next) {
				if (c->expires != -1 && c->expires < now)
					/* cookie has expired => ignore */
					continue;

				if (c->secure && lwc_string_isequal(
							q->scheme,
							corestring_lwc_https,
							&match) &&
						match == false)
					/* secure cookie for insecure host.
					 * ignore */
					continue;

				if (c->http_only && !include_http_only)
					/* Ignore HttpOnly */
					continue;

				matched_cookies[count++] = c;

				GROW_MATCHED_COOKIES;

				if (c->version < (unsigned int)version)
					version = c->version;

				c->last_used = now;

				cookie_manager_add((struct cookie_data *)c);
			}
		}
	}

	/* Now consider cookies whose paths prefix-match ours */
	for (p = p->parent; p; p = p->parent) {
		/* Find directory's path entry(ies) */
		/* There are potentially multiple due to differing schemes */
		for (q = p->children; q; q = q->next) {
			if (*(q->segment) != '\0')
				continue;

			for (c = q->cookies; c; c = c->next) {
				if (c->expires != -1 && c->expires < now)
					/* cookie has expired => ignore */
					continue;

				if (c->secure && lwc_string_isequal(
							q->scheme,
							corestring_lwc_https,
							&match) &&
						match == false)
					/* Secure cookie for insecure server
					 * => ignore */
					continue;

				matched_cookies[count++] = c;

				GROW_MATCHED_COOKIES;

				if (c->version < (unsigned int) version)
					version = c->version;

				c->last_used = now;

				cookie_manager_add((struct cookie_data *)c);
			}
		}

		if (!p->parent) {
			/* No parent, so bail here. This can't go in
			 * the loop exit condition as we also want to
			 * process the top-level node.
                         *
                         * If p->parent is NULL then p->cookies are
                         * the domain cookies and thus we don't even
                         * try matching against them.
                         */
			break;
		}

		/* Consider p itself - may be the result of Path=/foo */
		for (c = p->cookies; c; c = c->next) {
			if (c->expires != -1 && c->expires < now)
				/* cookie has expired => ignore */
				continue;

			/* Ensure cookie path is a prefix of the resource */
			if (strncmp(c->path, path, strlen(c->path)) != 0)
				/* paths don't match => ignore */
				continue;

			if (c->secure && lwc_string_isequal(p->scheme,
						corestring_lwc_https,
						&match) &&
					match == false)
				/* Secure cookie for insecure server
				 * => ignore */
				continue;

			matched_cookies[count++] = c;

			GROW_MATCHED_COOKIES;

			if (c->version < (unsigned int) version)
				version = c->version;

			c->last_used = now;

			cookie_manager_add((struct cookie_data *)c);
		}

	}

	/* Finally consider domain cookies for hosts which domain match ours */
	for (h = (const struct host_part *)p; h && h != &db_root;
			h = h->parent) {
		for (c = h->paths.cookies; c; c = c->next) {
			if (c->expires != -1 && c->expires < now)
				/* cookie has expired => ignore */
				continue;

			/* Ensure cookie path is a prefix of the resource */
			if (strncmp(c->path, path, strlen(c->path)) != 0)
				/* paths don't match => ignore */
				continue;

			if (c->secure && lwc_string_isequal(scheme,
						corestring_lwc_https,
						&match) &&
					match == false)
				/* secure cookie for insecure host. ignore */
				continue;

			matched_cookies[count++] = c;

			GROW_MATCHED_COOKIES;

			if (c->version < (unsigned int)version)
				version = c->version;

			c->last_used = now;

			cookie_manager_add((struct cookie_data *)c);
		}
	}

	if (count == 0) {
		/* No cookies found */
		free(ret);
		free(matched_cookies);
		return NULL;
	}

	/* and build output string */
	if (version > COOKIE_NETSCAPE) {
		sprintf(ret, "$Version=%d", version);
		ret_used = strlen(ret) + 1;
	}

	for (i = 0; i < count; i++) {
		if (!urldb_concat_cookie(matched_cookies[i], version,
				&ret_used, &ret_alloc, &ret)) {
			free(ret);
			free(matched_cookies);
			return NULL;
		}
	}

	if (version == COOKIE_NETSCAPE) {
		/* Old-style cookies => no version & skip "; " */
		memmove(ret, ret + 2, ret_used - 2);
		ret_used -= 2;
	}

	/* Now, shrink the output buffer to the required size */
	{
		char *temp = realloc(ret, ret_used);
		if (!temp) {
			free(ret);
			free(matched_cookies);
			return NULL;
		}

		ret = temp;
	}

	free(matched_cookies);

	return ret;

#undef GROW_MATCHED_COOKIES
}

/**
 * Parse Set-Cookie header and insert cookie(s) into database
 *
 * \param header Header to parse, with Set-Cookie: stripped
 * \param url URL being fetched
 * \param referer Referring resource, or 0 for verifiable transaction
 * \return true on success, false otherwise
 */
bool urldb_set_cookie(const char *header, nsurl *url, nsurl *referer)
{
	const char *cur = header, *end;
	lwc_string *path, *host, *scheme;
	nsurl *urlt;
	bool match;

	assert(url && header);

	/* Get defragmented URL, as 'urlt' */
	if (nsurl_defragment(url, &urlt) != NSERROR_OK)
		return NULL;

	scheme = nsurl_get_component(url, NSURL_SCHEME);
	if (scheme == NULL) {
		nsurl_unref(urlt);
		return false;
	}

	path = nsurl_get_component(url, NSURL_PATH);
	if (path == NULL) {
		lwc_string_unref(scheme);
		nsurl_unref(urlt);
		return false;
	}

	host = nsurl_get_component(url, NSURL_HOST);
	if (host == NULL) {
		lwc_string_unref(path);
		lwc_string_unref(scheme);
		nsurl_unref(urlt);
		return false;
	}

	if (referer) {
		lwc_string *rhost;

		/* Ensure that url's host name domain matches
		 * referer's (4.3.5) */
		rhost = nsurl_get_component(referer, NSURL_HOST);
		if (rhost == NULL) {
			goto error;
		}

		/* Domain match host names */
		if (lwc_string_isequal(host, rhost, &match) == lwc_error_ok &&
				match == false) {
			const char *hptr;
			const char *rptr;
			const char *dot;
			const char *host_data = lwc_string_data(host);
			const char *rhost_data = lwc_string_data(rhost);

			/* Ensure neither host nor rhost are IP addresses */
			if (url_host_is_ip_address(host_data) ||
					url_host_is_ip_address(rhost_data)) {
				/* IP address, so no partial match */
				lwc_string_unref(rhost);
				goto error;
			}

			/* Not exact match, so try the following:
			 * 
			 * 1) Find the longest common suffix of host and rhost
			 *    (may be all of host/rhost)
			 * 2) Discard characters from the start of the suffix
			 *    until the suffix starts with a dot
			 *    (prevents foobar.com matching bar.com)
			 * 3) Ensure the suffix is non-empty and contains 
			 *    embedded dots (to avoid permitting .com as a 
			 *    suffix)
			 *
			 * Note that the above in no way resembles the
			 * domain matching algorithm found in RFC2109.
			 * It does, however, model the real world rather
			 * more accurately.
			 */

			/** \todo In future, we should consult a TLD service
			 * instead of just looking for embedded dots.
			 */

			hptr = host_data + lwc_string_length(host) - 1;
			rptr = rhost_data + lwc_string_length(rhost) - 1;

			/* 1 */
			while (hptr >= host_data && rptr >= rhost_data) {
				if (*hptr != *rptr)
					break;
				hptr--;
				rptr--;
			}
			/* Ensure we end up pointing at the start of the 
			 * common suffix. The above loop will exit pointing
			 * to the byte before the start of the suffix. */
			hptr++;

			/* 2 */
			while (*hptr != '\0' && *hptr != '.')
				hptr++;

			/* 3 */
			if (*hptr == '\0' || 
				(dot = strchr(hptr + 1, '.')) == NULL ||
					*(dot + 1) == '\0') {
				lwc_string_unref(rhost);
				goto error;
			}
		}

		lwc_string_unref(rhost);
	}

	end = cur + strlen(cur) - 2 /* Trailing CRLF */;

	do {
		struct cookie_internal_data *c;
		char *dot;
		size_t len;

		c = urldb_parse_cookie(url, &cur);
		if (!c) {
			/* failed => stop parsing */
			goto error;
		}

		/* validate cookie */

		/* 4.2.2:i Cookie must have NAME and VALUE */
		if (!c->name || !c->value) {
			urldb_free_cookie(c);
			goto error;
		}

		/* 4.3.2:i Cookie path must be a prefix of URL path */
		len = strlen(c->path);
		if (len > lwc_string_length(path) ||
				strncmp(c->path, lwc_string_data(path),
						len) != 0) {
			urldb_free_cookie(c);
			goto error;
		}

		/* 4.3.2:ii Cookie domain must contain embedded dots */
		dot = strchr(c->domain + 1, '.');
		if (!dot || *(dot + 1) == '\0') {
			/* no embedded dots */
			urldb_free_cookie(c);
			goto error;
		}

		/* Domain match fetch host with cookie domain */
		if (strcasecmp(lwc_string_data(host), c->domain) != 0) {
			int hlen, dlen;
			char *domain = c->domain;

			/* c->domain must be a domain cookie here because:
			 * c->domain is either:
			 *   + specified in the header as a domain cookie
			 *     (non-domain cookies in the header are ignored
			 *      by urldb_parse_cookie / urldb_parse_avpair)
			 *   + defaulted to the URL's host part
			 *     (by urldb_parse_cookie if no valid domain was
			 *      specified in the header)
			 *
			 * The latter will pass the strcasecmp above, which 
			 * leaves the former (i.e. a domain cookie)
			 */
			assert(c->domain[0] == '.');

			/* 4.3.2:iii */
			if (url_host_is_ip_address(lwc_string_data(host))) {
				/* IP address, so no partial match */
				urldb_free_cookie(c);
				goto error;
			}

			hlen = lwc_string_length(host);
			dlen = strlen(c->domain);

			if (hlen <= dlen && hlen != dlen - 1) {
				/* Partial match not possible */
				urldb_free_cookie(c);
				goto error;
			}

			if (hlen == dlen - 1) {
				/* Relax matching to allow
				 * host a.com to match .a.com */
				domain++;
				dlen--;
			}

			if (strcasecmp(lwc_string_data(host) + (hlen - dlen),
					domain)) {
				urldb_free_cookie(c);
				goto error;
			}

			/* 4.3.2:iv Ensure H contains no dots
			 *
			 * If you believe the spec, H should contain no
			 * dots in _any_ cookie. Unfortunately, however,
			 * reality differs in that many sites send domain
			 * cookies of the form .foo.com from hosts such
			 * as bar.bat.foo.com and then expect domain
			 * matching to work. Thus we have to do what they
			 * expect, regardless of any potential security
			 * implications.
			 *
			 * This is what code conforming to the spec would
			 * look like:
			 *
			 * for (int i = 0; i < (hlen - dlen); i++) {
			 *	if (host[i] == '.') {
			 *		urldb_free_cookie(c);
			 *		goto error;
			 *	}
			 * }
			 */
		}

		/* Now insert into database */
		if (!urldb_insert_cookie(c, scheme, urlt))
			goto error;
	} while (cur < end);

	lwc_string_unref(host);
	lwc_string_unref(path);
	lwc_string_unref(scheme);
	nsurl_unref(urlt);

	return true;

error:
	lwc_string_unref(host);
	lwc_string_unref(path);
	lwc_string_unref(scheme);
	nsurl_unref(urlt);

	return false;
}

/**
 * Parse a cookie
 *
 * \param url URL being fetched
 * \param cookie Pointer to cookie string (updated on exit)
 * \return Pointer to cookie structure (on heap, caller frees) or NULL
 */
struct cookie_internal_data *urldb_parse_cookie(nsurl *url,
		const char **cookie)
{
	struct cookie_internal_data *c;
	const char *cur;
	char name[1024], value[4096];
	char *n = name, *v = value;
	bool in_value = false;
	bool had_value_data = false;
	bool value_verbatim = false;
	bool quoted = false;
	bool was_quoted = false;

	assert(url && cookie && *cookie);

	c = calloc(1, sizeof(struct cookie_internal_data));
	if (c == NULL)
		return NULL;

	c->expires = -1;

	name[0] = '\0';
	value[0] = '\0';

	for (cur = *cookie; *cur; cur++) {
		if (*cur == '\r' && *(cur + 1) == '\n') {
			/* End of header */
			if (quoted) {
				/* Unmatched quote encountered */

				/* Match Firefox 2.0.0.11 */
				value[0] = '\0';

#if 0
				/* This is what IE6/7 & Safari 3 do */
				/* Opera 9.25 discards the entire cookie */

				/* Shuffle value up by 1 */
				memmove(value + 1, value, 
					min(v - value, sizeof(value) - 2));
				v++;
				/* And insert " character at the start */
				value[0] = '"';

				/* Now, run forwards through the value
				 * looking for a semicolon. If one exists,
				 * terminate the value at this point. */
				for (char *s = value; s < v; s++) {
					if (*s == ';') {
						*s = '\0';
						v = s;
						break;
					}
				}
#endif
			}

			break;
		} else if (*cur == '\r') {
			/* Spurious linefeed */
			continue;	
		} else if (*cur == '\n') {
			/* Spurious newline */
			continue;
		}

		if (in_value && !had_value_data) {
			if (*cur == ' ' || *cur == '\t') {
				/* Strip leading whitespace from value */
				continue;
			} else {
				had_value_data = true;

				/* Value is taken verbatim if first non-space 
				 * character is not a " */
				if (*cur != '"') {
					value_verbatim = true;
				}
			}
		}

		if (in_value && !value_verbatim && (*cur == '"')) {
			/* Only non-verbatim values may be quoted */
			if (cur == *cookie || *(cur - 1) != '\\') {
				/* Only unescaped quotes count */
				was_quoted = quoted;
				quoted = !quoted;

				continue;
			}
		}

		if (!quoted && !in_value && *cur == '=') {
			/* First equals => attr-value separator */
			in_value = true;
			continue;
		}

		if (!quoted && (was_quoted || *cur == ';')) {
			/* Semicolon or after quoted value 
			 * => end of current avpair */

			/* NUL-terminate tokens */
			*n = '\0';
			*v = '\0';

			if (!urldb_parse_avpair(c, name, value, was_quoted)) {
				/* Memory exhausted */
				urldb_free_cookie(c);
				return NULL;
			}

			/* And reset to start */
			n = name;
			v = value;
			in_value = false;
			had_value_data = false;
			value_verbatim = false;
			was_quoted = false;

			/* Now, if the current input is anything other than a
			 * semicolon, we must be sure to reprocess it */
			if (*cur != ';') {
				cur--;
			}

			continue;
		}

		/* And now handle commas. These are a pain as they may mean
		 * any of the following:
		 *
		 * + End of cookie
		 * + Day separator in Expires avpair
		 * + (Invalid) comma in unquoted value
		 *
		 * Therefore, in order to handle all 3 cases (2 and 3 are
		 * identical, the difference being that 2 is in the spec and
		 * 3 isn't), we need to determine where the comma actually
		 * lies. We use the following heuristic:
		 *
		 *   Given a comma at the current input position, find the
		 *   immediately following semicolon (or end of input if none
		 *   found). Then, consider the input characters between
		 *   these two positions. If any of these characters is an
		 *   '=', we must assume that the comma signified the end of
		 *   the current cookie.
		 *
		 * This holds as the first avpair of any cookie must be
		 * NAME=VALUE, so the '=' is guaranteed to appear in the
		 * case where the comma marks the end of a cookie.
		 *
		 * This will fail, however, in the case where '=' appears in
		 * the value of the current avpair after the comma or the
		 * subsequent cookie does not start with NAME=VALUE. Neither
		 * of these is particularly likely and if they do occur, the
		 * website is more broken than we can be bothered to handle.
		 */
		if (!quoted && *cur == ',') {
			/* Find semi-colon, if any */
			const char *p;
			const char *semi = strchr(cur + 1, ';');
			if (!semi)
				semi = cur + strlen(cur) - 2 /* CRLF */;

			/* Look for equals sign between comma and semi */
			for (p = cur + 1; p < semi; p++)
				if (*p == '=')
					break;

			if (p == semi) {
				/* none found => comma internal to value */
				/* do nothing */
			} else {
				/* found one => comma marks end of cookie */
				cur++;
				break;
			}
		}

		/* Accumulate into buffers, always leaving space for a NUL */
		/** \todo is silently truncating overlong names/values wise? */
		if (!in_value) {
			if (n < name + (sizeof(name) - 1))
				*n++ = *cur;
		} else {
			if (v < value + (sizeof(value) - 1))
				*v++ = *cur;
		}
	}

	/* Parse final avpair */
	*n = '\0';
	*v = '\0';

	if (!urldb_parse_avpair(c, name, value, was_quoted)) {
		/* Memory exhausted */
		urldb_free_cookie(c);
		return NULL;
	}

	/* Now fix-up default values */
	if (c->domain == NULL) {
		lwc_string *host = nsurl_get_component(url, NSURL_HOST);
		if (host == NULL) {
			urldb_free_cookie(c);
			return NULL;
		}
		c->domain = strdup(lwc_string_data(host));
		lwc_string_unref(host);
	}

	if (c->path == NULL) {
		const char *path_data;
		char *path, *slash;
		lwc_string *path_lwc;

		path_lwc = nsurl_get_component(url, NSURL_PATH);
		if (path_lwc == NULL) {
			urldb_free_cookie(c);
			return NULL;
		}
		path_data = lwc_string_data(path_lwc);

		/* Strip leafname and trailing slash (4.3.1) */
		slash = strrchr(path_data, '/');
		if (slash != NULL) {
			/* Special case: retain first slash in path */
			if (slash == path_data)
				slash++;

			slash = strndup(path_data, slash - path_data);
			if (slash == NULL) {
				lwc_string_unref(path_lwc);
				urldb_free_cookie(c);
				return NULL;
			}

			path = slash;
			lwc_string_unref(path_lwc);
		} else {
			path = strdup(lwc_string_data(path_lwc));
			lwc_string_unref(path_lwc);
			if (path == NULL) {
				urldb_free_cookie(c);
				return NULL;
			}
		}

		c->path = path;
	}

	/* Write back current position */
	*cookie = cur;

	return c;
}

/**
 * Parse a cookie avpair
 *
 * \param c Cookie struct to populate
 * \param n Name component
 * \param v Value component
 * \param was_quoted Whether ::v was quoted in the input
 * \return true on success, false on memory exhaustion
 */
bool urldb_parse_avpair(struct cookie_internal_data *c, char *n, char *v,
		bool was_quoted)
{
	int vlen;

	assert(c && n && v);

	/* Strip whitespace from start of name */
	for (; *n; n++) {
		if (*n != ' ' && *n != '\t')
			break;
	}

	/* Strip whitespace from end of name */
	for (vlen = strlen(n); vlen; vlen--) {
		if (n[vlen] == ' ' || n[vlen] == '\t')
			n[vlen] = '\0';
		else
			break;
	}

	/* Strip whitespace from start of value */
	for (; *v; v++) {
		if (*v != ' ' && *v != '\t')
			break;
	}

	/* Strip whitespace from end of value */
	for (vlen = strlen(v); vlen; vlen--) {
		if (v[vlen] == ' ' || v[vlen] == '\t')
			v[vlen] = '\0';
		else
			break;
	}

	if (!c->comment && strcasecmp(n, "Comment") == 0) {
		c->comment = strdup(v);
		if (!c->comment)
			return false;
	} else if (!c->domain && strcasecmp(n, "Domain") == 0) {
		if (v[0] == '.') {
			/* Domain must start with a dot */
			c->domain_from_set = true;
			c->domain = strdup(v);
			if (!c->domain)
				return false;
		}
	} else if (strcasecmp(n, "Max-Age") == 0) {
		int temp = atoi(v);
		if (temp == 0)
			/* Special case - 0 means delete */
			c->expires = 0;
		else
			c->expires = time(NULL) + temp;
	} else if (!c->path && strcasecmp(n, "Path") == 0) {
		c->path_from_set = true;
		c->path = strdup(v);
		if (!c->path)
			return false;
	} else if (strcasecmp(n, "Version") == 0) {
		c->version = atoi(v);
	} else if (strcasecmp(n, "Expires") == 0) {
		char *datenoday;
		time_t expires;

		/* Strip dayname from date (these are hugely
		 * variable and liable to break the parser.
		 * They also serve no useful purpose) */
		for (datenoday = v; *datenoday && !isdigit(*datenoday);
				datenoday++)
			; /* do nothing */

		expires = curl_getdate(datenoday, NULL);
		if (expires == -1) {
			/* assume we have an unrepresentable
			 * date => force it to the maximum
			 * possible value of a 32bit time_t
			 * (this may break in 2038. We'll
			 * deal with that once we come to
			 * it) */
			expires = (time_t)0x7fffffff;
		}
		c->expires = expires;
	} else if (strcasecmp(n, "Secure") == 0) {
		c->secure = true;
	} else if (strcasecmp(n, "HttpOnly") == 0) {
		c->http_only = true;
	} else if (!c->name) {
		c->name = strdup(n);
		c->value = strdup(v);
		c->value_was_quoted = was_quoted;
		if (!c->name || !c->value)
			return false;
	}

	return true;
}

/**
 * Insert a cookie into the database
 *
 * \param c The cookie to insert
 * \param scheme URL scheme associated with cookie path
 * \param url URL (sans fragment) associated with cookie
 * \return true on success, false on memory exhaustion (c will be freed)
 */
bool urldb_insert_cookie(struct cookie_internal_data *c, lwc_string *scheme,
		nsurl *url)
{
	struct cookie_internal_data *d;
	const struct host_part *h;
	struct path_data *p;
	time_t now = time(NULL);

	assert(c);

	if (c->domain[0] == '.') {
		h = urldb_search_find(
			urldb_get_search_tree(&(c->domain[1])),
			c->domain + 1);
		if (!h) {
			h = urldb_add_host(c->domain + 1);
			if (!h) {
				urldb_free_cookie(c);
				return false;
			}
		}

		p = (struct path_data *) &h->paths;
	} else {
		/* Need to have a URL and scheme, if it's not a domain cookie */
		assert(url != NULL);
		assert(scheme != NULL);

		h = urldb_search_find(
				urldb_get_search_tree(c->domain),
				c->domain);

		if (!h) {
			h = urldb_add_host(c->domain);
			if (!h) {
				urldb_free_cookie(c);
				return false;
			}
		}

		/* find path */
		p = urldb_add_path(scheme, 0, h,
				strdup(c->path), NULL, url);
		if (!p) {
			urldb_free_cookie(c);
			return false;
		}
	}

	/* add cookie */
	for (d = p->cookies; d; d = d->next) {
		if (!strcmp(d->domain, c->domain) &&
				!strcmp(d->path, c->path) &&
				!strcmp(d->name, c->name))
			break;
	}

	if (d) {
		if (c->expires != -1 && c->expires < now) {
			/* remove cookie */
			if (d->next)
				d->next->prev = d->prev;
			else
				p->cookies_end = d->prev;
			if (d->prev)
				d->prev->next = d->next;
			else
				p->cookies = d->next;

			cookie_manager_remove((struct cookie_data *)d);

			urldb_free_cookie(d);
			urldb_free_cookie(c);
		} else {
			/* replace d with c */
			c->prev = d->prev;
			c->next = d->next;
			if (c->next)
				c->next->prev = c;
			else
				p->cookies_end = c;
			if (c->prev)
				c->prev->next = c;
			else
				p->cookies = c;

			cookie_manager_remove((struct cookie_data *)d);
			urldb_free_cookie(d);

			cookie_manager_add((struct cookie_data *)c);
		}
	} else {
		c->prev = p->cookies_end;
		c->next = NULL;
		if (p->cookies_end)
			p->cookies_end->next = c;
		else
			p->cookies = c;
		p->cookies_end = c;

		cookie_manager_add((struct cookie_data *)c);
	}

	return true;
}

/**
 * Free a cookie
 *
 * \param c The cookie to free
 */
void urldb_free_cookie(struct cookie_internal_data *c)
{
	assert(c);

	free(c->comment);
	free(c->domain);
	free(c->path);
	free(c->name);
	free(c->value);
	free(c);
}

/**
 * Concatenate a cookie into the provided buffer
 *
 * \param c Cookie to concatenate
 * \param version The version of the cookie string to output
 * \param used Pointer to amount of buffer used (updated)
 * \param alloc Pointer to allocated size of buffer (updated)
 * \param buf Pointer to Pointer to buffer (updated)
 * \return true on success, false on memory exhaustion
 */
bool urldb_concat_cookie(struct cookie_internal_data *c, int version,
		int *used, int *alloc, char **buf)
{
	/* Combined (A)BNF for the Cookie: request header:
	 * 
	 * CHAR           = <any US-ASCII character (octets 0 - 127)>
	 * CTL            = <any US-ASCII control character
	 *                  (octets 0 - 31) and DEL (127)>
	 * CR             = <US-ASCII CR, carriage return (13)>
	 * LF             = <US-ASCII LF, linefeed (10)> 
	 * SP             = <US-ASCII SP, space (32)>
	 * HT             = <US-ASCII HT, horizontal-tab (9)>
	 * <">            = <US-ASCII double-quote mark (34)>
	 *
	 * CRLF           = CR LF
	 *
	 * LWS            = [CRLF] 1*( SP | HT )
	 *
	 * TEXT           = <any OCTET except CTLs,
	 *                  but including LWS>
	 *
	 * token          = 1*<any CHAR except CTLs or separators>
	 * separators     = "(" | ")" | "<" | ">" | "@"
	 *                | "," | ";" | ":" | "\" | <">
	 *                | "/" | "[" | "]" | "?" | "="
	 *                | "{" | "}" | SP | HT
	 *
	 * quoted-string  = ( <"> *(qdtext | quoted-pair ) <"> )
	 * qdtext         = <any TEXT except <">>
	 * quoted-pair    = "\" CHAR
	 *
	 * attr            =       token
	 * value           =       word
	 * word            =       token | quoted-string
	 *
	 * cookie          =       "Cookie:" cookie-version
	 *                         1*((";" | ",") cookie-value)
	 * cookie-value    =       NAME "=" VALUE [";" path] [";" domain]
	 * cookie-version  =       "$Version" "=" value
	 * NAME            =       attr
	 * VALUE           =       value
	 * path            =       "$Path" "=" value
	 * domain          =       "$Domain" "=" value
	 *
	 * A note on quoted-string handling:
	 *   The cookie data stored in the db is verbatim (i.e. sans enclosing
	 *   <">, if any, and with all quoted-pairs intact) thus all that we 
	 *   need to do here is ensure that value strings which were quoted
	 *   in Set-Cookie or which include any of the separators are quoted 
	 *   before use.
	 *
	 * A note on cookie-value separation:
	 *   We use semicolons for all separators, including between 
	 *   cookie-values. This simplifies things and is backwards compatible.
	 */		      
	const char * const separators = "()<>@,;:\\\"/[]?={} \t";

	int max_len;

	assert(c && used && alloc && buf && *buf);

	/* "; " cookie-value 
	 * We allow for the possibility that values are quoted
	 */
	max_len = 2 + strlen(c->name) + 1 + strlen(c->value) + 2 +
			(c->path_from_set ?
				8 + strlen(c->path) + 2 : 0) +
			(c->domain_from_set ?
				10 + strlen(c->domain) + 2 : 0);

	if (*used + max_len >= *alloc) {
		char *temp = realloc(*buf, *alloc + 4096);
		if (!temp) {
			return false;
		}
		*buf = temp;
		*alloc += 4096;
	}

	if (version == COOKIE_NETSCAPE) {
		/* Original Netscape cookie */
		sprintf(*buf + *used - 1, "; %s=", c->name);
		*used += 2 + strlen(c->name) + 1;

		/* The Netscape spec doesn't mention quoting of cookie values.
		 * RFC 2109 $10.1.3 indicates that values must not be quoted.
		 *
		 * However, other browsers preserve quoting, so we should, too
		 */
		if (c->value_was_quoted) {
			sprintf(*buf + *used - 1, "\"%s\"", c->value);
			*used += 1 + strlen(c->value) + 1;
		} else {
			/** \todo should we %XX-encode [;HT,SP] ? */
			/** \todo Should we strip escaping backslashes? */
			sprintf(*buf + *used - 1, "%s", c->value);
			*used += strlen(c->value);
		}

		/* We don't send path/domain information -- that's what the 
		 * Netscape spec suggests we should do, anyway. */
	} else {
		/* RFC2109 or RFC2965 cookie */
		sprintf(*buf + *used - 1, "; %s=", c->name);
		*used += 2 + strlen(c->name) + 1;

		/* Value needs quoting if it contains any separator or if
		 * it needs preserving from the Set-Cookie header */
		if (c->value_was_quoted ||
				strpbrk(c->value, separators) != NULL) {
			sprintf(*buf + *used - 1, "\"%s\"", c->value);
			*used += 1 + strlen(c->value) + 1;
		} else {
			sprintf(*buf + *used - 1, "%s", c->value);
			*used += strlen(c->value);
		}

		if (c->path_from_set) {
			/* Path, quoted if necessary */
			sprintf(*buf + *used - 1, "; $Path=");
			*used += 8;

			if (strpbrk(c->path, separators) != NULL) {
				sprintf(*buf + *used - 1, "\"%s\"", c->path);
				*used += 1 + strlen(c->path) + 1;
			} else {
				sprintf(*buf + *used - 1, "%s", c->path);
				*used += strlen(c->path);
			}
		}

		if (c->domain_from_set) {
			/* Domain, quoted if necessary */
			sprintf(*buf + *used - 1, "; $Domain=");
			*used += 10;

			if (strpbrk(c->domain, separators) != NULL) {
				sprintf(*buf + *used - 1, "\"%s\"", c->domain);
				*used += 1 + strlen(c->domain) + 1;
			} else {
				sprintf(*buf + *used - 1, "%s", c->domain);
				*used += strlen(c->domain);
			}
		}
	}

	return true;
}

/**
 * Load a cookie file into the database
 *
 * \param filename File to load
 */
void urldb_load_cookies(const char *filename)
{
	FILE *fp;
	char s[16*1024];

	assert(filename);

	fp = fopen(filename, "r");
	if (!fp)
		return;

#define FIND_T {							\
		for (; *p && *p != '\t'; p++)				\
			; /* do nothing */				\
		if (p >= end) {						\
			LOG(("Overran input"));				\
			continue;					\
		}							\
		*p++ = '\0';						\
}

#define SKIP_T {							\
		for (; *p && *p == '\t'; p++)				\
			; /* do nothing */				\
		if (p >= end) {						\
			LOG(("Overran input"));				\
			continue;					\
		}							\
}

	while (fgets(s, sizeof s, fp)) {
		char *p = s, *end = 0,
			*domain, *path, *name, *value, *scheme, *url,
			*comment;
		int version, domain_specified, path_specified,
			secure, http_only, no_destroy, value_quoted;
		time_t expires, last_used;
		struct cookie_internal_data *c;

		if(s[0] == 0 || s[0] == '#')
			/* Skip blank lines or comments */
			continue;

		s[strlen(s) - 1] = '\0'; /* lose terminating newline */
		end = s + strlen(s);

		/* Look for file version first
		 * (all input is ignored until this is read)
		 */
		if (strncasecmp(s, "Version:", 8) == 0) {
			FIND_T; SKIP_T; loaded_cookie_file_version = atoi(p);

			if (loaded_cookie_file_version < 
					MIN_COOKIE_FILE_VERSION) {
				LOG(("Unsupported Cookie file version"));
				break;
			}

			continue;
		} else if (loaded_cookie_file_version == 0) {
			/* Haven't yet seen version; skip this input */
			continue;
		}

		/* One cookie/line */

		/* Parse input */
		FIND_T; version = atoi(s);
		SKIP_T; domain = p; FIND_T;
		SKIP_T; domain_specified = atoi(p); FIND_T;
		SKIP_T; path = p; FIND_T;
		SKIP_T; path_specified = atoi(p); FIND_T;
		SKIP_T; secure = atoi(p); FIND_T;
		if (loaded_cookie_file_version > 101) {
			/* Introduced in version 1.02 */
			SKIP_T; http_only = atoi(p); FIND_T;
		} else {
			http_only = 0;
		}
		SKIP_T; expires = (time_t)atoi(p); FIND_T;
		SKIP_T; last_used = (time_t)atoi(p); FIND_T;
		SKIP_T; no_destroy = atoi(p); FIND_T;
		SKIP_T; name = p; FIND_T;
		SKIP_T; value = p; FIND_T;
		if (loaded_cookie_file_version > 100) {
			/* Introduced in version 1.01 */
			SKIP_T;	value_quoted = atoi(p); FIND_T;
		} else {
			value_quoted = 0;
		}
		SKIP_T; scheme = p; FIND_T;
		SKIP_T; url = p; FIND_T;

		/* Comment may have no content, so don't
		 * use macros as they'll break */
		for (; *p && *p == '\t'; p++)
			; /* do nothing */
		comment = p;

		assert(p <= end);

		/* Now create cookie */
		c = malloc(sizeof(struct cookie_internal_data));
		if (!c)
			break;

		c->name = strdup(name);
		c->value = strdup(value);
		c->value_was_quoted = value_quoted;
		c->comment = strdup(comment);
		c->domain_from_set = domain_specified;
		c->domain = strdup(domain);
		c->path_from_set = path_specified;
		c->path = strdup(path);
		c->expires = expires;
		c->last_used = last_used;
		c->secure = secure;
		c->http_only = http_only;
		c->version = version;
		c->no_destroy = no_destroy;

		if (!(c->name && c->value && c->comment &&
				c->domain && c->path)) {
			urldb_free_cookie(c);
			break;
		}

		if (c->domain[0] != '.') {
			lwc_string *scheme_lwc = NULL;
			nsurl *url_nsurl = NULL;

			assert(scheme[0] != 'u');

			if (nsurl_create(url, &url_nsurl) != NSERROR_OK) {
				urldb_free_cookie(c);
				break;
			}
			scheme_lwc = nsurl_get_component(url_nsurl,
					NSURL_SCHEME);

			/* And insert it into database */
			if (!urldb_insert_cookie(c, scheme_lwc, url_nsurl)) {
				/* Cookie freed for us */
				nsurl_unref(url_nsurl);
				lwc_string_unref(scheme_lwc);
				break;
			}
			nsurl_unref(url_nsurl);
			lwc_string_unref(scheme_lwc);

		} else {
			if (!urldb_insert_cookie(c, NULL, NULL)) {
				/* Cookie freed for us */
				break;
			}
		}
	}

#undef SKIP_T
#undef FIND_T

	fclose(fp);
}

/**
 * Delete a cookie
 *
 * \param domain The cookie's domain
 * \param path The cookie's path
 * \param name The cookie's name
 */
void urldb_delete_cookie(const char *domain, const char *path,
		const char *name)
{
	urldb_delete_cookie_hosts(domain, path, name, &db_root);
}

void urldb_delete_cookie_hosts(const char *domain, const char *path,
		const char *name, struct host_part *parent)
{
	struct host_part *h;
	assert(parent);

	urldb_delete_cookie_paths(domain, path, name, &parent->paths);

	for (h = parent->children; h; h = h->next)
		urldb_delete_cookie_hosts(domain, path, name, h);
}

void urldb_delete_cookie_paths(const char *domain, const char *path,
		const char *name, struct path_data *parent)
{
	struct cookie_internal_data *c;
	struct path_data *p = parent;

	assert(parent);

	do {
		for (c = p->cookies; c; c = c->next) {
			if (strcmp(c->domain, domain) == 0 && 
					strcmp(c->path, path) == 0 &&
					strcmp(c->name, name) == 0) {
				if (c->prev)
					c->prev->next = c->next;
				else
					p->cookies = c->next;

				if (c->next)
					c->next->prev = c->prev;
				else
					p->cookies_end = c->prev;

				urldb_free_cookie(c);

				return;
			}
		}

		if (p->children) {
			p = p->children;
		} else {
			while (p != parent) {
				if (p->next != NULL) {
					p = p->next;
					break;
				}

				p = p->parent;
			}
		}
	} while(p != parent);
}

/**
 * Save persistent cookies to file
 *
 * \param filename Path to save to
 */
void urldb_save_cookies(const char *filename)
{
	FILE *fp;
	int cookie_file_version = max(loaded_cookie_file_version, 
			COOKIE_FILE_VERSION);

	assert(filename);

	fp = fopen(filename, "w");
	if (!fp)
		return;

	fprintf(fp, "# >%s\n", filename);
	fprintf(fp, "# NetSurf cookies file.\n"
		    "#\n"
		    "# Lines starting with a '#' are comments, "
						"blank lines are ignored.\n"
		    "#\n"
		    "# All lines prior to \"Version:\t%d\" are discarded.\n"
		    "#\n"
		    "# Version\tDomain\tDomain from Set-Cookie\tPath\t"
			"Path from Set-Cookie\tSecure\tHTTP-Only\tExpires\tLast used\t"
			"No destroy\tName\tValue\tValue was quoted\tScheme\t"
			"URL\tComment\n",
			cookie_file_version);
	fprintf(fp, "Version:\t%d\n", cookie_file_version);

	urldb_save_cookie_hosts(fp, &db_root);

	fclose(fp);
}

/**
 * Save a host subtree's cookies
 *
 * \param fp File pointer to write to
 * \param parent Parent host
 */
void urldb_save_cookie_hosts(FILE *fp, struct host_part *parent)
{
	struct host_part *h;
	assert(fp && parent);

	urldb_save_cookie_paths(fp, &parent->paths);

	for (h = parent->children; h; h = h->next)
		urldb_save_cookie_hosts(fp, h);
}

/**
 * Save a path subtree's cookies
 *
 * \param fp File pointer to write to
 * \param parent Parent path
 */
void urldb_save_cookie_paths(FILE *fp, struct path_data *parent)
{
	struct path_data *p = parent;
	time_t now = time(NULL);

	assert(fp && parent);

	do {
		if (p->cookies != NULL) {
			struct cookie_internal_data *c;

			for (c = p->cookies; c != NULL; c = c->next) {
				if (c->expires == -1 || c->expires < now)
					/* Skip expired & session cookies */
					continue;

				fprintf(fp, 
					"%d\t%s\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t"
					"%s\t%s\t%d\t%s\t%s\t%s\n",
					c->version, c->domain,
					c->domain_from_set, c->path,
					c->path_from_set, c->secure,
					c->http_only,
					(int)c->expires, (int)c->last_used,
					c->no_destroy, c->name, c->value,
					c->value_was_quoted,
					p->scheme ? lwc_string_data(p->scheme) :
							"unused",
					p->url ? nsurl_access(p->url) :
							"unused",
					c->comment ? c->comment : "");
			}
		}

		if (p->children != NULL) {
			p = p->children;
		} else {
			while (p != parent) {
				if (p->next != NULL) {
					p = p->next;
					break;
				}

				p = p->parent;
			}
		}
	} while (p != parent);
}


/**
 * Destroy urldb
 */
void urldb_destroy(void)
{
	struct host_part *a, *b;
	int i;

	/* Clean up search trees */
	for (i = 0; i < NUM_SEARCH_TREES; i++) {
		if (search_trees[i] != &empty)
			urldb_destroy_search_tree(search_trees[i]);
	}

	/* And database */
	for (a = db_root.children; a; a = b) {
		b = a->next;
		urldb_destroy_host_tree(a);
	}
        
        /* And the bloom filter */
        if (url_bloom != NULL)
                bloom_destroy(url_bloom);
}

/**
 * Destroy a host tree
 *
 * \param root Root node of tree to destroy
 */
void urldb_destroy_host_tree(struct host_part *root)
{
	struct host_part *a, *b;
	struct path_data *p, *q;
	struct prot_space_data *s, *t;

	/* Destroy children */
	for (a = root->children; a; a = b) {
		b = a->next;
		urldb_destroy_host_tree(a);
	}

	/* Now clean up paths */
	for (p = root->paths.children; p; p = q) {
		q = p->next;
		urldb_destroy_path_tree(p);
	}

	/* Root path */
	urldb_destroy_path_node_content(&root->paths);

	/* Proctection space data */
	for (s = root->prot_space; s; s = t) {
		t = s->next;
		urldb_destroy_prot_space(s);
	}

	/* And ourselves */
	free(root->part);
	free(root);
}

/**
 * Destroy a path tree
 *
 * \param root Root node of tree to destroy
 */
void urldb_destroy_path_tree(struct path_data *root)
{
	struct path_data *p = root;

	do {
		if (p->children != NULL) {
			p = p->children;
		} else {
			struct path_data *q = p;

			while (p != root) {
				if (p->next != NULL) {
					p = p->next;
					break;
				}

				p = p->parent;

				urldb_destroy_path_node_content(q);
				free(q);

				q = p;
			}

			urldb_destroy_path_node_content(q);
			free(q);
		}
	} while (p != root);
}

/**
 * Destroy the contents of a path node
 *
 * \param node Node to destroy contents of (does not destroy node)
 */
void urldb_destroy_path_node_content(struct path_data *node)
{
	struct cookie_internal_data *a, *b;
	unsigned int i;

	if (node->url != NULL)
		nsurl_unref(node->url);

	if (node->scheme != NULL)
		lwc_string_unref(node->scheme);

	free(node->segment);
	for (i = 0; i < node->frag_cnt; i++)
		free(node->fragment[i]);
	free(node->fragment);

	if (node->thumb)
		bitmap_destroy(node->thumb);

	free(node->urld.title);

	for (a = node->cookies; a; a = b) {
		b = a->next;
		urldb_destroy_cookie(a);
	}
}

/**
 * Destroy a cookie node
 *
 * \param c Cookie to destroy
 */
void urldb_destroy_cookie(struct cookie_internal_data *c)
{
	free(c->name);
	free(c->value);
	free(c->comment);
	free(c->domain);
	free(c->path);

	free(c);
}

/**
 * Destroy protection space data
 *
 * \param space Protection space to destroy
 */
void urldb_destroy_prot_space(struct prot_space_data *space)
{
	lwc_string_unref(space->scheme);
	free(space->realm);
	free(space->auth);

	free(space);
}


/**
 * Destroy a search tree
 *
 * \param root Root node of tree to destroy
 */
void urldb_destroy_search_tree(struct search_node *root)
{
	/* Destroy children */
	if (root->left != &empty)
		urldb_destroy_search_tree(root->left);
	if (root->right != &empty)
		urldb_destroy_search_tree(root->right);

	/* And destroy ourselves */
	free(root);
}

