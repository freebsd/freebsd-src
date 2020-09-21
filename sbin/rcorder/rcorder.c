# if 0
/*	$NetBSD: rcorder.c,v 1.7 2000/08/04 07:33:55 enami Exp $	*/
#endif

/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 1998, 1999 Matthew R. Green
 * All rights reserved.
 * Copyright (c) 1998
 * 	Perry E. Metzger.  All rights reserved.
 * Copyright (c) 2020
 *     Boris N. Lytochkin. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project
 *	by Perry E. Metzger.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
__FBSDID("$FreeBSD$");

#include <sys/stat.h>

#include <err.h>
#include <stdio.h>
#include <libutil.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <stdbool.h>

#include "ealloc.h"
#include "sprite.h"
#include "hash.h"

#ifdef DEBUG
static int debug = 0;
# define	DPRINTF(args) if (debug) { fflush(stdout); fprintf args; }
#else
# define	DPRINTF(args)
#endif

#define REQUIRE_STR	"# REQUIRE:"
#define REQUIRE_LEN	(sizeof(REQUIRE_STR) - 1)
#define REQUIRES_STR	"# REQUIRES:"
#define REQUIRES_LEN	(sizeof(REQUIRES_STR) - 1)
#define PROVIDE_STR	"# PROVIDE:"
#define PROVIDE_LEN	(sizeof(PROVIDE_STR) - 1)
#define PROVIDES_STR	"# PROVIDES:"
#define PROVIDES_LEN	(sizeof(PROVIDES_STR) - 1)
#define BEFORE_STR	"# BEFORE:"
#define BEFORE_LEN	(sizeof(BEFORE_STR) - 1)
#define KEYWORD_STR	"# KEYWORD:"
#define KEYWORD_LEN	(sizeof(KEYWORD_STR) - 1)
#define KEYWORDS_STR	"# KEYWORDS:"
#define KEYWORDS_LEN	(sizeof(KEYWORDS_STR) - 1)

#define	FAKE_PROV_NAME	"fake_prov_"

static int exit_code;
static int file_count;
static char **file_list;

#define TRUE 1
#define FALSE 0
typedef bool flag;
#define SET TRUE
#define RESET FALSE

static flag do_graphviz = false;
static flag do_parallel = false;

static Hash_Table provide_hash_s, *provide_hash;

typedef struct provnode provnode;
typedef struct filenode filenode;
typedef struct f_provnode f_provnode;
typedef struct f_reqnode f_reqnode;
typedef struct strnodelist strnodelist;

struct provnode {
	flag		head;
	flag		in_progress;
	int		sequence;
	filenode	*fnode;
	provnode	*next, *last;
};

struct f_provnode {
	provnode	*pnode;
	Hash_Entry	*entry;
	f_provnode	*next;
};

struct f_reqnode {
	Hash_Entry	*entry;
	f_reqnode	*next;
};

struct strnodelist {
	filenode	*node;
	strnodelist	*next;
	char		s[1];
};

struct filenode {
	char		*filename;
	flag		in_progress;
	filenode	*next, *last;
	f_reqnode	*req_list;
	f_provnode	*prov_list;
	strnodelist	*keyword_list;
	int		issues_count;
	int		sequence;
};

static filenode fn_head_s, *fn_head, **fn_seqlist;
static int max_sequence = 0;

static strnodelist *bl_list;
static strnodelist *keep_list;
static strnodelist *skip_list;

static void do_file(filenode *fnode, strnodelist *);
static void strnode_add(strnodelist **, char *, filenode *);
static int skip_ok(filenode *fnode);
static int keep_ok(filenode *fnode);
static char *generate_loop_for_req(strnodelist *, provnode *, filenode *);
static void satisfy_req(f_reqnode *rnode, filenode *fnode, strnodelist *);
static void crunch_file(char *);
static void parse_require(filenode *, char *);
static void parse_provide(filenode *, char *);
static void parse_before(filenode *, char *);
static void parse_keywords(filenode *, char *);
static filenode *filenode_new(char *);
static void add_require(filenode *, char *);
static void add_provide(filenode *, char *);
static void add_before(filenode *, char *);
static void add_keyword(filenode *, char *);
static void insert_before(void);
static Hash_Entry *make_fake_provision(filenode *);
static void crunch_all_files(void);
static void initialize(void);
static void generate_graphviz_header(void);
static void generate_graphviz_footer(void);
static void generate_graphviz_file_links(Hash_Entry *, filenode *);
static void generate_graphviz_providers(void);
static inline int is_fake_prov(const char *);
static int sequence_cmp(const void *, const void *);
static void generate_ordering(void);

int
main(int argc, char *argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "dgk:ps:")) != -1)
		switch (ch) {
		case 'd':
#ifdef DEBUG
			debug = 1;
#else
			warnx("debugging not compiled in, -d ignored");
#endif
			break;
		case 'g':
			do_graphviz = true;
			break;
		case 'k':
			strnode_add(&keep_list, optarg, 0);
			break;
		case 'p':
			do_parallel = true;
			break;
		case 's':
			strnode_add(&skip_list, optarg, 0);
			break;
		default:
			/* XXX should crunch it? */
			break;
		}
	argc -= optind;
	argv += optind;

	file_count = argc;
	file_list = argv;

	DPRINTF((stderr, "parse_args\n"));
	initialize();
	DPRINTF((stderr, "initialize\n"));
	generate_graphviz_header();
	crunch_all_files();
	DPRINTF((stderr, "crunch_all_files\n"));
	generate_graphviz_providers();
	generate_ordering();
	DPRINTF((stderr, "generate_ordering\n"));
	generate_graphviz_footer();

	exit(exit_code);
}

/*
 * initialise various variables.
 */
static void
initialize(void)
{

	fn_head = &fn_head_s;

	provide_hash = &provide_hash_s;
	Hash_InitTable(provide_hash, file_count);
}

/* generic function to insert a new strnodelist element */
static void
strnode_add(strnodelist **listp, char *s, filenode *fnode)
{
	strnodelist *ent;

	ent = emalloc(sizeof *ent + strlen(s));
	ent->node = fnode;
	strcpy(ent->s, s);
	ent->next = *listp;
	*listp = ent;
}

/*
 * below are the functions that deal with creating the lists
 * from the filename's given dependencies and provisions
 * in each of these files.  no ordering or checking is done here.
 */

/*
 * we have a new filename, create a new filenode structure.
 * fill in the bits, and put it in the filenode linked list
 */
static filenode *
filenode_new(char *filename)
{
	filenode *temp;

	temp = emalloc(sizeof(*temp));
	memset(temp, 0, sizeof(*temp));
	temp->filename = estrdup(filename);
	temp->req_list = NULL;
	temp->prov_list = NULL;
	temp->keyword_list = NULL;
	temp->in_progress = RESET;
	/*
	 * link the filenode into the list of filenodes.
	 * note that the double linking means we can delete a
	 * filenode without searching for where it belongs.
	 */
	temp->next = fn_head->next;
	if (temp->next != NULL)
		temp->next->last = temp;
	temp->last = fn_head;
	fn_head->next = temp;
	return (temp);
}

/*
 * add a requirement to a filenode.
 */
static void
add_require(filenode *fnode, char *s)
{
	Hash_Entry *entry;
	f_reqnode *rnode;
	int new;

	entry = Hash_CreateEntry(provide_hash, s, &new);
	if (new)
		Hash_SetValue(entry, NULL);
	rnode = emalloc(sizeof(*rnode));
	rnode->entry = entry;
	rnode->next = fnode->req_list;
	fnode->req_list = rnode;
}

/*
 * add a provision to a filenode.  if this provision doesn't
 * have a head node, create one here.
 */
static void
add_provide(filenode *fnode, char *s)
{
	Hash_Entry *entry;
	f_provnode *f_pnode;
	provnode *pnode, *head;
	int new;

	entry = Hash_CreateEntry(provide_hash, s, &new);
	head = Hash_GetValue(entry);

	/* create a head node if necessary. */
	if (head == NULL) {
		head = emalloc(sizeof(*head));
		head->head = SET;
		head->in_progress = RESET;
		head->fnode = NULL;
		head->sequence = 0;
		head->last = head->next = NULL;
		Hash_SetValue(entry, head);
	}
#if 0
	/*
	 * Don't warn about this.  We want to be able to support
	 * scripts that do two complex things:
	 *
	 *	- Two independent scripts which both provide the
	 *	  same thing.  Both scripts must be executed in
	 *	  any order to meet the barrier.  An example:
	 *
	 *		Script 1:
	 *
	 *			PROVIDE: mail
	 *			REQUIRE: LOGIN
	 *
	 *		Script 2:
	 *
	 *			PROVIDE: mail
	 *			REQUIRE: LOGIN
	 *
	 * 	- Two interdependent scripts which both provide the
	 *	  same thing.  Both scripts must be executed in
	 *	  graph order to meet the barrier.  An example:
	 *
	 *		Script 1:
	 *
	 *			PROVIDE: nameservice dnscache
	 *			REQUIRE: SERVERS
	 *
	 *		Script 2:
	 *
	 *			PROVIDE: nameservice nscd
	 *			REQUIRE: dnscache
	 */
	else if (new == 0) {
		warnx("file `%s' provides `%s'.", fnode->filename, s);
		warnx("\tpreviously seen in `%s'.",
		    head->next->fnode->filename);
	}
#endif

	pnode = emalloc(sizeof(*pnode));
	pnode->head = RESET;
	pnode->in_progress = RESET;
	pnode->fnode = fnode;
	pnode->next = head->next;
	pnode->last = head;
	head->next = pnode;
	if (pnode->next != NULL)
		pnode->next->last = pnode;

	f_pnode = emalloc(sizeof(*f_pnode));
	f_pnode->pnode = pnode;
	f_pnode->entry = entry;
	f_pnode->next = fnode->prov_list;
	fnode->prov_list = f_pnode;
}

/*
 * put the BEFORE: lines to a list and handle them later.
 */
static void
add_before(filenode *fnode, char *s)
{
	strnodelist *bf_ent;

	bf_ent = emalloc(sizeof *bf_ent + strlen(s));
	bf_ent->node = fnode;
	strcpy(bf_ent->s, s);
	bf_ent->next = bl_list;
	bl_list = bf_ent;
}

/*
 * add a key to a filenode.
 */
static void
add_keyword(filenode *fnode, char *s)
{

	strnode_add(&fnode->keyword_list, s, fnode);
}

/*
 * loop over the rest of a REQUIRE line, giving each word to
 * add_require() to do the real work.
 */
static void
parse_require(filenode *node, char *buffer)
{
	char *s;
	
	while ((s = strsep(&buffer, " \t\n")) != NULL)
		if (*s != '\0')
			add_require(node, s);
}

/*
 * loop over the rest of a PROVIDE line, giving each word to
 * add_provide() to do the real work.
 */
static void
parse_provide(filenode *node, char *buffer)
{
	char *s;
	
	while ((s = strsep(&buffer, " \t\n")) != NULL)
		if (*s != '\0')
			add_provide(node, s);
}

/*
 * loop over the rest of a BEFORE line, giving each word to
 * add_before() to do the real work.
 */
static void
parse_before(filenode *node, char *buffer)
{
	char *s;
	
	while ((s = strsep(&buffer, " \t\n")) != NULL)
		if (*s != '\0')
			add_before(node, s);
}

/*
 * loop over the rest of a KEYWORD line, giving each word to
 * add_keyword() to do the real work.
 */
static void
parse_keywords(filenode *node, char *buffer)
{
	char *s;
	
	while ((s = strsep(&buffer, " \t\n")) != NULL)
		if (*s != '\0')
			add_keyword(node, s);
}

/*
 * given a file name, create a filenode for it, read in lines looking
 * for provision and requirement lines, building the graphs as needed.
 */
static void
crunch_file(char *filename)
{
	FILE *fp;
	char *buf;
	int require_flag, provide_flag, before_flag, keywords_flag;
	enum { BEFORE_PARSING, PARSING, PARSING_DONE } state;
	filenode *node;
	char delims[3] = { '\\', '\\', '\0' };
	struct stat st;

	if ((fp = fopen(filename, "r")) == NULL) {
		warn("could not open %s", filename);
		return;
	}

	if (fstat(fileno(fp), &st) == -1) {
		warn("could not stat %s", filename);
		fclose(fp);
		return;
	}

	if (!S_ISREG(st.st_mode)) {
#if 0
		warnx("%s is not a file", filename);
#endif
		fclose(fp);
		return;
	}

	node = filenode_new(filename);

	/*
	 * we don't care about length, line number, don't want # for comments,
	 * and have no flags.
	 */
	for (state = BEFORE_PARSING; state != PARSING_DONE &&
	    (buf = fparseln(fp, NULL, NULL, delims, 0)) != NULL; free(buf)) {
		require_flag = provide_flag = before_flag = keywords_flag = 0;
		if (strncmp(REQUIRE_STR, buf, REQUIRE_LEN) == 0)
			require_flag = REQUIRE_LEN;
		else if (strncmp(REQUIRES_STR, buf, REQUIRES_LEN) == 0)
			require_flag = REQUIRES_LEN;
		else if (strncmp(PROVIDE_STR, buf, PROVIDE_LEN) == 0)
			provide_flag = PROVIDE_LEN;
		else if (strncmp(PROVIDES_STR, buf, PROVIDES_LEN) == 0)
			provide_flag = PROVIDES_LEN;
		else if (strncmp(BEFORE_STR, buf, BEFORE_LEN) == 0)
			before_flag = BEFORE_LEN;
		else if (strncmp(KEYWORD_STR, buf, KEYWORD_LEN) == 0)
			keywords_flag = KEYWORD_LEN;
		else if (strncmp(KEYWORDS_STR, buf, KEYWORDS_LEN) == 0)
			keywords_flag = KEYWORDS_LEN;
		else {
			if (state == PARSING)
				state = PARSING_DONE;
			continue;
		}

		state = PARSING;
		if (require_flag)
			parse_require(node, buf + require_flag);
		else if (provide_flag)
			parse_provide(node, buf + provide_flag);
		else if (before_flag)
			parse_before(node, buf + before_flag);
		else if (keywords_flag)
			parse_keywords(node, buf + keywords_flag);
	}
	fclose(fp);
}

static Hash_Entry *
make_fake_provision(filenode *node)
{
	Hash_Entry *entry;
	f_provnode *f_pnode;
	provnode *head, *pnode;
	static	int i = 0;
	int	new;
	char buffer[30];

	do {
		snprintf(buffer, sizeof buffer, FAKE_PROV_NAME "%08d", i++);
		entry = Hash_CreateEntry(provide_hash, buffer, &new);
	} while (new == 0);
	head = emalloc(sizeof(*head));
	head->head = SET;
	head->in_progress = RESET;
	head->fnode = NULL;
	head->last = head->next = NULL;
	Hash_SetValue(entry, head);

	pnode = emalloc(sizeof(*pnode));
	pnode->head = RESET;
	pnode->in_progress = RESET;
	pnode->fnode = node;
	pnode->next = head->next;
	pnode->last = head;
	head->next = pnode;
	if (pnode->next != NULL)
		pnode->next->last = pnode;

	f_pnode = emalloc(sizeof(*f_pnode));
	f_pnode->entry = entry;
	f_pnode->pnode = pnode;
	f_pnode->next = node->prov_list;
	node->prov_list = f_pnode;

	return (entry);
}

/*
 * go through the BEFORE list, inserting requirements into the graph(s)
 * as required.  in the before list, for each entry B, we have a file F
 * and a string S.  we create a "fake" provision (P) that F provides.
 * for each entry in the provision list for S, add a requirement to
 * that provisions filenode for P.
 */
static void
insert_before(void)
{
	Hash_Entry *entry, *fake_prov_entry;
	provnode *pnode;
	f_reqnode *rnode;
	strnodelist *bl;
	int new;
	
	while (bl_list != NULL) {
		bl = bl_list->next;

		fake_prov_entry = make_fake_provision(bl_list->node);

		entry = Hash_CreateEntry(provide_hash, bl_list->s, &new);
		if (new == 1)
			warnx("file `%s' is before unknown provision `%s'", bl_list->node->filename, bl_list->s);

		if (new == 1 && do_graphviz == true)
			generate_graphviz_file_links(
			    Hash_FindEntry(provide_hash, bl_list->s),
			    bl_list->node);

		for (pnode = Hash_GetValue(entry); pnode; pnode = pnode->next) {
			if (pnode->head)
				continue;

			rnode = emalloc(sizeof(*rnode));
			rnode->entry = fake_prov_entry;
			rnode->next = pnode->fnode->req_list;
			pnode->fnode->req_list = rnode;
		}

		free(bl_list);
		bl_list = bl;
	}
}

/*
 * loop over all the files calling crunch_file() on them to do the
 * real work.  after we have built all the nodes, insert the BEFORE:
 * lines into graph(s).
 */
static void
crunch_all_files(void)
{
	int i;
	
	for (i = 0; i < file_count; i++)
		crunch_file(file_list[i]);
	insert_before();
}

static inline int
is_fake_prov(const char *name)
{

	return (name == strstr(name, FAKE_PROV_NAME));
}

/* loop though provide list of vnode drawing all non-fake dependencies */
static void
generate_graphviz_file_links(Hash_Entry *entry, filenode *fnode)
{
	char *dep_name, *fname;
	provnode *head;
	f_provnode *fpnode, *rfpnode;
	int is_before = 0;

	dep_name = Hash_GetKey(entry);
	if (is_fake_prov(dep_name))
		is_before = 1;
	head = Hash_GetValue(entry);

	for (fpnode = fnode->prov_list; fpnode && fpnode->entry;
	    fpnode = fpnode->next) {
		fname = Hash_GetKey(fpnode->entry);
		if (is_fake_prov(fname))
			continue;
		rfpnode = NULL;
		do {
			if (rfpnode)
				dep_name = Hash_GetKey(rfpnode->entry);
			else
				dep_name = Hash_GetKey(entry);

			if (!is_fake_prov(dep_name)) {
				printf("\"%s\" -> \"%s\" [%s%s];\n",
				    fname, dep_name,
				    /* edge style */
				    (is_before ? "style=dashed" : "style=solid"),
				    /* circular dep? */
				    ((head == NULL ||
				    (head->next && head->in_progress == SET)) ?
				    ", color=red, penwidth=4" : ""));
				if (rfpnode == NULL)
					break;
			}
			/* dependency is solved already */
			if (head == NULL || head->next == NULL)
				break;

			if (rfpnode == NULL)
				rfpnode = head->next->fnode->prov_list;
			else
				rfpnode = rfpnode->next;
		} while (rfpnode);
	}
}

/*
 * Walk the stack, find the looping point and generate traceback.
 * NULL is returned on failure, otherwize pointer to a buffer holding
 * text representation is returned, caller must run free(3) for the
 * pointer.
 */
static char *
generate_loop_for_req(strnodelist *stack_tail, provnode *head,
    filenode *fnode)
{
	provnode *pnode;
	strnodelist *stack_ptr, *loop_entry;
	char *buf, **revstack;
	size_t bufsize;
	int i, stack_depth;

	loop_entry = NULL;
	/* fast forward stack to the component that is required now */
	for (pnode = head->next; pnode; pnode = pnode->next) {
		if (loop_entry)
			break;
		stack_depth = 0;
		for (stack_ptr = stack_tail; stack_ptr;
		    stack_ptr = stack_ptr->next) {
			stack_depth++;
			if (stack_ptr->node == pnode->fnode) {
				loop_entry = stack_ptr;
				break;
			}
		}
	}

	if (loop_entry == NULL)
		return (NULL);

	stack_depth += 2; /* fnode + loop_entry */
	revstack = emalloc(sizeof(char *) * stack_depth);
	bzero(revstack, (sizeof(char *) * stack_depth));

	/* reverse stack and estimate buffer size to allocate */
	bufsize = 1; /* tralining \0 */

	revstack[stack_depth - 1] = loop_entry->node->filename;
	bufsize += strlen(revstack[stack_depth - 1]);

	revstack[stack_depth - 2] = fnode->filename;
	bufsize += strlen(revstack[stack_depth - 2]);
	fnode->issues_count++;

	stack_ptr = stack_tail;
	for (i = stack_depth - 3; i >= 0; i--) {
		revstack[i] = stack_ptr->node->filename;
		stack_ptr->node->issues_count++;
		stack_ptr = stack_ptr->next;
		bufsize += strlen(revstack[i]);
	}
	bufsize += strlen(" -> ") * (stack_depth - 1);

	buf = emalloc(bufsize);
	bzero(buf, bufsize);

	for (i = 0; i < stack_depth; i++) {
		strlcat(buf, revstack[i], bufsize);
		if (i < stack_depth - 1)
			strlcat(buf, " -> ", bufsize);
	}

	free(revstack);
	return (buf);
}
/*
 * below are the functions that traverse the graphs we have built
 * finding out the desired ordering, printing each file in turn.
 * if missing requirements, or cyclic graphs are detected, a
 * warning will be issued, and we will continue on..
 */

/*
 * given a requirement node (in a filename) we attempt to satisfy it.
 * we do some sanity checking first, to ensure that we have providers,
 * aren't already satisfied and aren't already being satisfied (ie,
 * cyclic).  if we pass all this, we loop over the provision list
 * calling do_file() (enter recursion) for each filenode in this
 * provision.
 */
static void
satisfy_req(f_reqnode *rnode, filenode *fnode, strnodelist *stack_ptr)
{
	Hash_Entry *entry;
	provnode *head;
	strnodelist stack_item;
	char *buf;

	entry = rnode->entry;
	head = Hash_GetValue(entry);

	if (do_graphviz == true)
		generate_graphviz_file_links(entry, fnode);

	if (head == NULL) {
		warnx("requirement `%s' in file `%s' has no providers.",
		    Hash_GetKey(entry), fnode->filename);
		exit_code = 1;
		return;
	}

	/* return if the requirement is already satisfied. */
	if (head->next == NULL)
		return;

	/* 
	 * if list is marked as in progress,
	 *	print that there is a circular dependency on it and abort
	 */
	if (head->in_progress == SET) {
		exit_code = 1;
		buf = generate_loop_for_req(stack_ptr, head,
		    fnode);

		if (buf == NULL) {
			warnx("Circular dependency on provision `%s' in "
			    "file `%s' (tracing has failed).",
			    Hash_GetKey(entry), fnode->filename);
			return;
		}

		warnx("Circular dependency on provision `%s': %s.",
		    Hash_GetKey(entry), buf);
		free(buf);
		return;
	}

	head->in_progress = SET;
	
	stack_item.next = stack_ptr;
	stack_item.node = fnode;

	/*
	 * while provision_list is not empty
	 *	do_file(first_member_of(provision_list));
	 */
	while (head->next != NULL)
		do_file(head->next->fnode, &stack_item);
}

static int
skip_ok(filenode *fnode)
{
	strnodelist *s;
	strnodelist *k;

	for (s = skip_list; s; s = s->next)
		for (k = fnode->keyword_list; k; k = k->next)
			if (strcmp(k->s, s->s) == 0)
				return (0);

	return (1);
}

static int
keep_ok(filenode *fnode)
{
	strnodelist *s;
	strnodelist *k;

	for (s = keep_list; s; s = s->next)
		for (k = fnode->keyword_list; k; k = k->next)
			if (strcmp(k->s, s->s) == 0)
				return (1);

	/* an empty keep_list means every one */
	return (!keep_list);
}

/*
 * given a filenode, we ensure we are not a cyclic graph.  if this
 * is ok, we loop over the filenodes requirements, calling satisfy_req()
 * for each of them.. once we have done this, remove this filenode
 * from each provision table, as we are now done.
 *
 * NOTE: do_file() is called recursively from several places and cannot
 * safely free() anything related to items that may be recursed on.
 * Circular dependencies will cause problems if we do.
 */
static void
do_file(filenode *fnode, strnodelist *stack_ptr)
{
	f_reqnode *r;
	f_provnode *p, *p_tmp;
	provnode *pnode, *head;
	int was_set;	
	char *dep_name;

	DPRINTF((stderr, "do_file on %s.\n", fnode->filename));

	/*
	 * if fnode is marked as in progress,
	 *	 print that fnode; is circularly depended upon and abort.
	 */
	if (fnode->in_progress == SET) {
		warnx("Circular dependency on file `%s'.",
			fnode->filename);
		was_set = exit_code = 1;
	} else
		was_set = 0;

	/* mark fnode */
	fnode->in_progress = SET;

	/*
	 * for each requirement of fnode -> r
	 *	satisfy_req(r, filename)
	 */
	r = fnode->req_list;
	fnode->sequence = 0;
	while (r != NULL) {
		satisfy_req(r, fnode, stack_ptr);
		/* find sequence number where all requirements are satisfied */
		head = Hash_GetValue(r->entry);
		if (head && head->sequence > fnode->sequence)
			fnode->sequence = head->sequence;
		r = r->next;
	}
	fnode->req_list = NULL;
	fnode->sequence++;

	/* if we've seen issues with this file - put it to the tail */
	if (fnode->issues_count)
		fnode->sequence = max_sequence + 1;

	if (max_sequence < fnode->sequence)
		max_sequence = fnode->sequence;

	/*
	 * for each provision of fnode -> p
	 *	remove fnode from provision list for p in hash table
	 */
	p = fnode->prov_list;
	while (p != NULL) {
		/* mark all troublemakers on graphviz */
		if (do_graphviz == true && fnode->issues_count) {
			dep_name = Hash_GetKey(p->entry);
			if (!is_fake_prov(dep_name))
				printf("\"%s\" [ color=red, penwidth=4 ];\n",
				    dep_name);
		}

		/* update sequence when provided requirements are satisfied */
		head = Hash_GetValue(p->entry);
		if (head->sequence < fnode->sequence)
			head->sequence = fnode->sequence;

		p_tmp = p;
		pnode = p->pnode;
		if (pnode->next != NULL) {
			pnode->next->last = pnode->last;
		}
		if (pnode->last != NULL) {
			pnode->last->next = pnode->next;
		}
		free(pnode);
		p = p->next;
		free(p_tmp);
	}
	fnode->prov_list = NULL;

	/* do_it(fnode) */
	DPRINTF((stderr, "next do: "));

	/* if we were already in progress, don't print again */
	if (do_graphviz != true && was_set == 0 && skip_ok(fnode) &&
	    keep_ok(fnode)) {
		*fn_seqlist = fnode;
		fn_seqlist++;
	}
	
	if (fnode->next != NULL) {
		fnode->next->last = fnode->last;
	}
	if (fnode->last != NULL) {
		fnode->last->next = fnode->next;
	}

	if (fnode->issues_count)
		warnx("`%s' was seen in circular dependencies for %d times.",
		    fnode->filename, fnode->issues_count);

	DPRINTF((stderr, "nuking %s\n", fnode->filename));
}

static void
generate_graphviz_header(void)
{

	if (do_graphviz != true)
		return;

	printf("digraph rcorder {\n"
	    "rankdir=\"BT\";\n"
	    "node [style=rounded, shape=record];\n"
	    "graph [overlap = false];\n");
}

static void
generate_graphviz_footer(void)
{

	if (do_graphviz == true)
		printf("}\n");
}

static void
generate_graphviz_providers(void)
{
	Hash_Entry *entry;
	Hash_Search psearch;
	provnode *head, *pnode;
	char *dep_name;

	if (do_graphviz != true)
		return;

	entry = Hash_EnumFirst(provide_hash, &psearch);
	if (entry == NULL)
		return;

	do {
		dep_name = Hash_GetKey(entry);
		if (is_fake_prov(dep_name))
			continue;
		head = Hash_GetValue(entry);
		/* no providers for this requirement */
		if (head == NULL || head->next == NULL) {
			printf("\"%s\" [label=\"{ %s | ENOENT }\", "
			    "style=\"rounded,filled\", color=red];\n",
			    dep_name, dep_name);
			continue;
		}
		/* one PROVIDE word for one file that matches */
		if (head->next->next == NULL &&
		    strcmp(dep_name,
		    basename(head->next->fnode->filename)) == 0) {
		        continue;
		}
		printf("\"%s\" [label=\"{ %s | ", dep_name, dep_name);
		for (pnode = head->next; pnode; pnode = pnode->next)
			printf("%s\\n", basename(pnode->fnode->filename));

		printf("}\"];\n");
	} while (NULL != (entry = Hash_EnumNext(&psearch)));
}

static int
sequence_cmp(const void *a, const void *b)
{
	const filenode *fna = *((const filenode * const *)a);
	const filenode *fnb = *((const filenode * const *)b);
	int left, right;

	/* push phantom files to the end */
	if (fna == NULL || fnb == NULL)
		return ((fna < fnb) - (fna > fnb));

	left =  fna->sequence;
	right = fnb->sequence;

	return ((left > right) - (left < right));
}

static void
generate_ordering(void)
{
	filenode **seqlist, **psl;
	int last_seq = 0;

	/* Prepare order buffer, use an additional one as a list terminator */
	seqlist = emalloc(sizeof(filenode *) * (file_count + 1));
	bzero(seqlist, sizeof(filenode *) * (file_count + 1));
	fn_seqlist = seqlist;

	/*
	 * while there remain undone files{f},
	 *	pick an arbitrary f, and do_file(f)
	 * Note that the first file in the file list is perfectly
	 * arbitrary, and easy to find, so we use that.
	 */

	/*
	 * N.B.: the file nodes "self delete" after they execute, so
	 * after each iteration of the loop, the head will be pointing
	 * to something totally different. The loop ends up being
	 * executed only once for every strongly connected set of
	 * nodes.
	 */
	while (fn_head->next != NULL) {
		DPRINTF((stderr, "generate on %s\n", fn_head->next->filename));
		do_file(fn_head->next, NULL);
	}

	/* Sort filenode list based on sequence */
	qsort(seqlist, file_count, sizeof(filenode *), sequence_cmp);

	for (psl = seqlist; *psl; psl++) {
		printf("%s%s",
		    (last_seq == 0 ? "" :
		    (do_parallel != true || last_seq != (*psl)->sequence) ?
		    "\n" : " "),
		(*psl)->filename);
		last_seq = (*psl)->sequence;
		free((*psl)->filename);
		free(*psl);
	}
	if (last_seq)
		printf("\n");

	free(seqlist);
}
