/*
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 *	All rights reserved.
 *
 * Author: Harti Brandt <harti@freebsd.org>
 *
 * Redistribution of this software and documentation and use in source and
 * binary forms, with or without modification, are permitted provided that
 * the following conditions are met:
 *
 * 1. Redistributions of source code or documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE AND DOCUMENTATION IS PROVIDED BY FRAUNHOFER FOKUS
 * AND ITS CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * FRAUNHOFER FOKUS OR ITS CONTRIBUTORS  BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Begemot: bsnmp/gensnmptree/gensnmptree.c,v 1.34 2003/01/28 13:44:34 hbb Exp $
 *
 * Generate OID table from table description.
 *
 * Syntax is:
 * ---------
 * tree := head elements ')'
 *
 * entry := head ':' index STRING elements ')'
 *
 * leaf := head TYPE STRING ACCESS ')'
 *
 * column := head TYPE ACCESS ')'
 *
 * head := '(' INT STRING
 *
 * elements := EMPTY | elements element
 *
 * element := tree | leaf
 *
 * index := TYPE | index TYPE
 *
 */
#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <err.h>
#include <sys/queue.h>
#include "asn1.h"
#include "snmp.h"
#include "snmpagent.h"

/*
 * Constant prefix for all OIDs
 */
static const asn_subid_t prefix[] = { 1, 3, 6 };
#define	PREFIX_LEN	(sizeof(prefix) / sizeof(prefix[0]))

u_int tree_size;
static const char *file_prefix = "";
static FILE *fp;

/* if true generate local include paths */
static int localincs = 0;

static const char usgtxt[] = "\
Generate SNMP tables. Copyright (c) 2001-2002 Fraunhofer Institute for\n\
Open Communication Systems (FhG Fokus). All rights reserved.\n\
usage: gensnmptree [-hel] [-p prefix] [name]...\n\
options:\n\
  -h		print this info\n\
  -e		extrace the named oids\n\
  -l		generate local include directives\n\
  -p prefix	prepend prefix to file and variable names\n\
";

/*
 * A node in the OID tree
 */
enum ntype {
	NODE_LEAF = 1,
	NODE_TREE,
	NODE_ENTRY,
	NODE_COLUMN
};

enum {
	FL_GET	= 0x01,
	FL_SET	= 0x02,
};

struct node;
TAILQ_HEAD(node_list, node);

struct node {
	enum ntype	type;
	asn_subid_t	id;	/* last element of OID */
	char		*name;	/* name of node */
	TAILQ_ENTRY(node) link;
	u_int		lno;	/* starting line number */
	u_int		flags;	/* allowed operations */

	union {
	  struct tree {
	    struct node_list subs;
	  }		tree;

	  struct entry {
	    u_int32_t	index;	/* index for table entry */
	    char	*func;	/* function for tables */
	    struct node_list subs;
	  }		entry;

	  struct leaf {
	    enum snmp_syntax syntax;	/* syntax for this leaf */
	    char	*func;		/* function name */
	  }		leaf;

	  struct column {
	    enum snmp_syntax syntax;	/* syntax for this column */
	  }		column;
	}		u;
};

struct func {
	const char	*name;
	LIST_ENTRY(func) link;
};

static LIST_HEAD(, func) funcs = LIST_HEAD_INITIALIZER(funcs);

/************************************************************
 *
 * Allocate memory and panic just in the case...
 */
static void *
xalloc(size_t size)
{
	void *ptr;

	if ((ptr = malloc(size)) == NULL)
		err(1, "allocing %u bytes", size);

	return (ptr);
}

/************************************************************
 *
 * Parsing input
 */
enum tok {
	TOK_EOF = 0200,	/* end-of-file seen */
	TOK_NUM,	/* number */
	TOK_STR,	/* string */
	TOK_ACCESS,	/* access operator */
	TOK_TYPE,	/* type operator */
};

static const struct {
	const char *str;
	enum tok tok;
	u_int val;
} keywords[] = {
	{ "GET", TOK_ACCESS, FL_GET },
	{ "SET", TOK_ACCESS, FL_SET },
	{ "NULL", TOK_TYPE, SNMP_SYNTAX_NULL },
	{ "INTEGER", TOK_TYPE, SNMP_SYNTAX_INTEGER },
	{ "INTEGER32", TOK_TYPE, SNMP_SYNTAX_INTEGER },
	{ "UNSIGNED32", TOK_TYPE, SNMP_SYNTAX_GAUGE },
	{ "OCTETSTRING", TOK_TYPE, SNMP_SYNTAX_OCTETSTRING },
	{ "IPADDRESS", TOK_TYPE, SNMP_SYNTAX_IPADDRESS },
	{ "OID", TOK_TYPE, SNMP_SYNTAX_OID },
	{ "TIMETICKS", TOK_TYPE, SNMP_SYNTAX_TIMETICKS },
	{ "COUNTER", TOK_TYPE, SNMP_SYNTAX_COUNTER },
	{ "GAUGE", TOK_TYPE, SNMP_SYNTAX_GAUGE },
	{ "COUNTER64", TOK_TYPE, SNMP_SYNTAX_COUNTER64 },
	{ NULL, 0, 0 }
};

/* arbitrary upper limit on node names and function names */
#define	MAXSTR	1000
char	str[MAXSTR];
u_long	val;		/* integer values */
u_int 	lno = 1;	/* current line number */

static void report(const char *, ...) __dead2 __printflike(1, 2);
static void report_node(const struct node *, const char *, ...)
    __dead2 __printflike(2, 3);

/*
 * Report an error and exit.
 */
static void
report(const char *fmt, ...)
{
	va_list ap;
	int c;

	va_start(ap, fmt);
	fprintf(stderr, "line %u: ", lno);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	fprintf(stderr, "context: \"");
	while ((c = getchar()) != EOF && c != '\n')
		fprintf(stderr, "%c", c);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(1);
}
static void
report_node(const struct node *np, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "line %u, node %s: ", np->lno, np->name);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(1);
}

/*
 * Return a fresh copy of the string constituting the current token.
 */
static char *
savetok(void)
{
	return (strcpy(xalloc(strlen(str)+1), str));
}

/*
 * Get the next token from input.
 */
static int
gettoken(void)
{
	int c;

  again:
	/*
	 * Skip any whitespace before the next token
	 */
	while ((c = getchar()) != EOF) {
		if (c == '\n')
			lno++;
		if (!isspace(c))
			break;
	}
	if (c == EOF)
		return (TOK_EOF);
	if (!isascii(c))
		report("unexpected character %#2x", (u_int)c);

	/*
	 * Skip comments
	 */
	if (c == '#') {
		while ((c = getchar()) != EOF) {
			if (c == '\n') {
				lno++;
				goto again;
			}
		}
		report("unexpected EOF in comment");
	}

	/*
	 * Single character tokens
	 */
	if (c == ')' || c == '(' || c == ':')
		return (c);

	/*
	 * Sort out numbers
	 */
	if (isdigit(c)) {
		ungetc(c, stdin);
		scanf("%lu", &val);
		return (TOK_NUM);
	}

	/*
	 * So that has to be a string.
	 */
	if (isalpha(c) || c == '_') {
		size_t n = 0;
		str[n++] = c;
		while ((c = getchar()) != EOF) {
			if (!isalnum(c) && c != '_') {
				ungetc(c, stdin);
				break;
			}
			if (n == sizeof(str) - 1) {
				str[n++] = '\0';
				report("string too long '%s...'", str);
			}
			str[n++] = c;
		}
		str[n++] = '\0';

		/*
		 * Keywords
		 */
		for (c = 0; keywords[c].str != NULL; c++)
			if (strcmp(keywords[c].str, str) == 0) {
				val = keywords[c].val;
				return (keywords[c].tok);
			}

		return (TOK_STR);
	}
	if (isprint(c))
		errx(1, "%u: unexpected character '%c'", lno, c);
	else
		errx(1, "%u: unexpected character 0x%02x", lno, (u_int)c);
}

/*
 * Parse the next node (complete with all subnodes)
 */
static struct node *
parse(enum tok tok)
{
	struct node *node;
	struct node *sub;
	u_int index_count;

	node = xalloc(sizeof(struct node));
	node->lno = lno;

	if (tok != '(')
		report("'(' expected at begin of node");
	if (gettoken() != TOK_NUM)
		report("node id expected after opening '('");
	if (val > ASN_MAXID)
		report("subid too large '%lu'", val);
	node->id = (asn_subid_t)val;
	if (gettoken() != TOK_STR)
		report("node name expected after '(' ID");
	node->name = savetok();

	if ((tok = gettoken()) == TOK_TYPE) {
		/* LEAF or COLUM */
		u_int syntax = val;

		if ((tok = gettoken()) == TOK_STR) {
			/* LEAF */
			node->type = NODE_LEAF;
			node->u.leaf.func = savetok();
			node->u.leaf.syntax = syntax;
			tok = gettoken();
		} else {
			/* COLUMN */
			node->type = NODE_COLUMN;
			node->u.column.syntax = syntax;
		}

		while (tok != ')') {
			if (tok != TOK_ACCESS)
				report("access keyword or ')' expected");
			node->flags |= (u_int)val;
			tok = gettoken();
		}

	} else if (tok == ':') {
		/* ENTRY */
		node->type = NODE_ENTRY;
		TAILQ_INIT(&node->u.entry.subs);

		index_count = 0;
		node->u.entry.index = 0;
		while ((tok = gettoken()) == TOK_TYPE) {
			if (index_count++ == SNMP_INDEXES_MAX)
				report("too many table indexes");
			node->u.entry.index |=
			    val << (SNMP_INDEX_SHIFT * index_count);
		}
		node->u.entry.index |= index_count;
		if (index_count == 0)
			report("need at least one index");

		if (tok != TOK_STR)
			report("function name expected");

		node->u.entry.func = savetok();

		tok = gettoken();

		while (tok != ')') {
			sub = parse(tok);
			TAILQ_INSERT_TAIL(&node->u.entry.subs, sub, link);
			tok = gettoken();
		}

	} else {
		/* subtree */
		node->type = NODE_TREE;
		TAILQ_INIT(&node->u.tree.subs);

		while (tok != ')') {
			sub = parse(tok);
			TAILQ_INSERT_TAIL(&node->u.tree.subs, sub, link);
			tok = gettoken();
		}
	}
	return (node);
}

/*
 * Generate the C-code table part for one node.
 */
static void
gen_node(struct node *np, struct asn_oid *oid, u_int idx, const char *func)
{
	u_int n;
	struct node *sub;
	u_int syntax;

	if (oid->len == ASN_MAXOIDLEN)
		report_node(np, "OID too long");
	oid->subs[oid->len++] = np->id;

	if (np->type == NODE_TREE) {
		TAILQ_FOREACH(sub, &np->u.tree.subs, link)
			gen_node(sub, oid, 0, NULL);
		oid->len--;
		return;
	}
	if (np->type == NODE_ENTRY) {
		TAILQ_FOREACH(sub, &np->u.entry.subs, link)
			gen_node(sub, oid, np->u.entry.index, np->u.entry.func);
		oid->len--;
		return;
	}

	/* leaf or column */
	if ((np->flags & (FL_GET|FL_SET)) == 0) {
		oid->len--;
		return;
	}

	fprintf(fp, "    {{ %u, {", oid->len);
	for (n = 0; n < oid->len; n++)
		fprintf(fp, " %u,", oid->subs[n]);
	fprintf(fp, " }}, \"%s\", ", np->name);

	if (np->type == NODE_COLUMN) {
		syntax = np->u.column.syntax;
		fprintf(fp, "SNMP_NODE_COLUMN, ");
	} else {
		syntax = np->u.leaf.syntax;
		fprintf(fp, "SNMP_NODE_LEAF, ");
	}

	switch (syntax) {

	  case SNMP_SYNTAX_NULL:
		fprintf(fp, "SNMP_SYNTAX_NULL, ");
		break;

	  case SNMP_SYNTAX_INTEGER:
		fprintf(fp, "SNMP_SYNTAX_INTEGER, ");
		break;

	  case SNMP_SYNTAX_OCTETSTRING:
		fprintf(fp, "SNMP_SYNTAX_OCTETSTRING, ");
		break;

	  case SNMP_SYNTAX_IPADDRESS:
		fprintf(fp, "SNMP_SYNTAX_IPADDRESS, ");
		break;

	  case SNMP_SYNTAX_OID:
		fprintf(fp, "SNMP_SYNTAX_OID, ");
		break;

	  case SNMP_SYNTAX_TIMETICKS:
		fprintf(fp, "SNMP_SYNTAX_TIMETICKS, ");
		break;

	  case SNMP_SYNTAX_COUNTER:
		fprintf(fp, "SNMP_SYNTAX_COUNTER, ");
		break;

	  case SNMP_SYNTAX_GAUGE:
		fprintf(fp, "SNMP_SYNTAX_GAUGE, ");
		break;

	  case SNMP_SYNTAX_COUNTER64:
		fprintf(fp, "SNMP_SYNTAX_COUNTER64, ");
		break;

	  case SNMP_SYNTAX_NOSUCHOBJECT:
	  case SNMP_SYNTAX_NOSUCHINSTANCE:
	  case SNMP_SYNTAX_ENDOFMIBVIEW:
		abort();
	}

	if (np->type == NODE_COLUMN)
		fprintf(fp, "%s, ", func);
	else
		fprintf(fp, "%s, ", np->u.leaf.func);

	fprintf(fp, "0");
	if (np->flags & FL_SET)
		fprintf(fp, "|SNMP_NODE_CANSET");
	fprintf(fp, ", %#x, NULL },\n", idx);
	oid->len--;
	return;
}

/*
 * Generate the header file with the function declarations.
 */
static void
gen_header(struct node *np, u_int oidlen, const char *func)
{
	char f[MAXSTR + 4];
	struct node *sub;
	struct func *ptr;

	oidlen++;
	if (np->type == NODE_TREE) {
		TAILQ_FOREACH(sub, &np->u.tree.subs, link)
			gen_header(sub, oidlen, NULL);
		return;
	}
	if (np->type == NODE_ENTRY) {
		TAILQ_FOREACH(sub, &np->u.entry.subs, link)
			gen_header(sub, oidlen, np->u.entry.func);
		return;
	}

 	if((np->flags & (FL_GET|FL_SET)) == 0)
		return;

	if (np->type == NODE_COLUMN)
		sprintf(f, "%s", func);
	else
		sprintf(f, "%s", np->u.leaf.func);

	LIST_FOREACH(ptr, &funcs, link)
		if (strcmp(ptr->name, f) == 0)
			break;

	if (ptr == NULL) {
		ptr = xalloc(sizeof(*ptr));
		ptr->name = strcpy(xalloc(strlen(f)+1), f);
		LIST_INSERT_HEAD(&funcs, ptr, link);

		fprintf(fp, "int	%s(struct snmp_context *, "
		    "struct snmp_value *, u_int, u_int, "
		    "enum snmp_op);\n", f);
	}

	fprintf(fp, "# define LEAF_%s %u\n", np->name, np->id);
}

/*
 * Generate the OID table.
 */
static void
gen_table(struct node *node)
{
	struct asn_oid oid;

	fprintf(fp, "#include <sys/types.h>\n");
	fprintf(fp, "#include <stdio.h>\n");
	if (localincs) {
		fprintf(fp, "#include \"asn1.h\"\n");
		fprintf(fp, "#include \"snmp.h\"\n");
		fprintf(fp, "#include \"snmpagent.h\"\n");
	} else {
		fprintf(fp, "#include <bsnmp/asn1.h>\n");
		fprintf(fp, "#include <bsnmp/snmp.h>\n");
		fprintf(fp, "#include <bsnmp/snmpagent.h>\n");
	}
	fprintf(fp, "#include \"%stree.h\"\n", file_prefix);
	fprintf(fp, "\n");

	fprintf(fp, "const struct snmp_node %sctree[] = {\n", file_prefix);

	oid.len = PREFIX_LEN;
	memcpy(oid.subs, prefix, sizeof(prefix));
	gen_node(node, &oid, 0, NULL);

	fprintf(fp, "};\n\n");
}

static int
extract(const struct node *np, struct asn_oid *oid, const char *obj)
{
	struct node *sub;
	u_long n;

	if (oid->len == ASN_MAXOIDLEN)
		report_node(np, "OID too long");
	oid->subs[oid->len++] = np->id;

	if (strcmp(obj, np->name) == 0) {
		fprintf(fp, "#define OID_%s\t%u\n", np->name, np->id);
		fprintf(fp, "#define OIDLEN_%s\t%u\n", np->name, oid->len);
		fprintf(fp, "#define OIDX_%s\t{ %u, {", np->name, oid->len);
		for (n = 0; n < oid->len; n++)
			fprintf(fp, " %u,", oid->subs[n]);
		fprintf(fp, " } }\n");
		return (0);
	}

	if (np->type == NODE_TREE) {
		TAILQ_FOREACH(sub, &np->u.tree.subs, link)
			if (!extract(sub, oid, obj))
				return (0);
	} else if (np->type == NODE_ENTRY) {
		TAILQ_FOREACH(sub, &np->u.entry.subs, link)
			if (!extract(sub, oid, obj))
				return (0);
	}
	oid->len--;
	return (1);
}

static int
gen_extract(const struct node *root, const char *object)
{
	struct asn_oid oid;

	oid.len = PREFIX_LEN;
	memcpy(oid.subs, prefix, sizeof(prefix));
	return (extract(root, &oid, object));
}


static void
check_sub_order(const struct node *np, const struct node_list *subs)
{
	int first;
	const struct node *sub;
	asn_subid_t maxid = 0;

	/* ensure, that subids are ordered */
	first = 1;
	TAILQ_FOREACH(sub, subs, link) {
		if (!first && sub->id <= maxid)
			report_node(np, "subids not ordered at %s", sub->name);
		maxid = sub->id;
		first = 0;
	}
}

/*
 * Do some sanity checks on the tree definition and do some computations.
 */
static void
check_tree(struct node *np)
{
	struct node *sub;

	if (np->type == NODE_LEAF || np->type == NODE_COLUMN) {
		if ((np->flags & (FL_GET|FL_SET)) != 0)
			tree_size++;
		return;
	}

	if (np->type == NODE_ENTRY) {
		check_sub_order(np, &np->u.entry.subs);

		/* ensure all subnodes are columns */
		TAILQ_FOREACH(sub, &np->u.entry.subs, link) {
			if (sub->type != NODE_COLUMN)
				report_node(np, "entry subnode '%s' is not "
				    "a column", sub->name);
			check_tree(sub);
		}
	} else {
		check_sub_order(np, &np->u.tree.subs);

		TAILQ_FOREACH(sub, &np->u.tree.subs, link)
			check_tree(sub);
	}
}

int
main(int argc, char *argv[])
{
	int do_extract = 0;
	int opt;
	struct node *root;
	char fname[MAXPATHLEN + 1];

	while ((opt = getopt(argc, argv, "help:")) != EOF)
		switch (opt) {

		  case 'h':
			fprintf(stderr, "%s", usgtxt);
			exit(0);

		  case 'e':
			do_extract = 1;
			break;

		  case 'l':
			localincs = 1;
			break;

		  case 'p':
			file_prefix = optarg;
			if (strlen(file_prefix) + strlen("tree.c") >
			    MAXPATHLEN)
				errx(1, "prefix too long");
			break;
		}

	if (!do_extract && argc != optind)
		errx(1, "no arguments allowed");
	if (do_extract && argc == optind)
		errx(1, "no objects specified");

	root = parse(gettoken());
	if (gettoken() != TOK_EOF)
		report("junk after closing ')'");

	check_tree(root);

	if (do_extract) {
		fp = stdout;
		while (optind < argc) {
			if (gen_extract(root, argv[optind]))
				errx(1, "object not found: %s", argv[optind]);
			optind++;
		}

		return (0);
	}
	sprintf(fname, "%stree.h", file_prefix);
	if ((fp = fopen(fname, "w")) == NULL)
		err(1, "%s: ", fname);
	gen_header(root, PREFIX_LEN, NULL);

	fprintf(fp, "#define %sCTREE_SIZE %u\n", file_prefix, tree_size);
	fprintf(fp, "extern const struct snmp_node %sctree[];\n", file_prefix);

	fclose(fp);

	sprintf(fname, "%stree.c", file_prefix);
	if ((fp = fopen(fname, "w")) == NULL)
		err(1, "%s: ", fname);
	gen_table(root);
	fclose(fp);

	return (0);
}
