/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mknodes.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

/*
 * This program reads the nodetypes file and nodes.c.pat file.  It generates
 * the files nodes.h and nodes.c.
 */

#include <stdio.h>


#define MAXTYPES 50		/* max number of node types */
#define MAXFIELDS 20		/* max fields in a structure */
#define BUFLEN 100		/* size of character buffers */

/* field types */
#define T_NODE 1		/* union node *field */
#define T_NODELIST 2		/* struct nodelist *field */
#define T_STRING 3
#define T_INT 4			/* int field */
#define T_OTHER 5		/* other */
#define T_TEMP 6		/* don't copy this field */


struct field {			/* a structure field */
	char *name;		/* name of field */
	int type;			/* type of field */
	char *decl;		/* declaration of field */
};


struct str {			/* struct representing a node structure */
	char *tag;		/* structure tag */
	int nfields;		/* number of fields in the structure */
	struct field field[MAXFIELDS];	/* the fields of the structure */
	int done;			/* set if fully parsed */
};


int ntypes;			/* number of node types */
char *nodename[MAXTYPES];	/* names of the nodes */
struct str *nodestr[MAXTYPES];	/* type of structure used by the node */
int nstr;			/* number of structures */
struct str str[MAXTYPES];	/* the structures */
struct str *curstr;		/* current structure */


FILE *infp = stdin;
char line[1024];
int linno;
char *linep;


char *savestr();
#define equal(s1, s2)	(strcmp(s1, s2) == 0)


main(argc, argv)
	char **argv;
	{
	if (argc != 3)
		error("usage: mknodes file\n");
	if ((infp = fopen(argv[1], "r")) == NULL)
		error("Can't open %s", argv[1]);
	while (readline()) {
		if (line[0] == ' ' || line[0] == '\t')
			parsefield();
		else if (line[0] != '\0')
			parsenode();
	}
	output(argv[2]);
	exit(0);
}



parsenode() {
	char name[BUFLEN];
	char tag[BUFLEN];
	struct str *sp;

	if (curstr && curstr->nfields > 0)
		curstr->done = 1;
	nextfield(name);
	if (! nextfield(tag))
		error("Tag expected");
	if (*linep != '\0')
		error("Garbage at end of line");
	nodename[ntypes] = savestr(name);
	for (sp = str ; sp < str + nstr ; sp++) {
		if (equal(sp->tag, tag))
			break;
	}
	if (sp >= str + nstr) {
		sp->tag = savestr(tag);
		sp->nfields = 0;
		curstr = sp;
		nstr++;
	}
	nodestr[ntypes] = sp;
	ntypes++;
}


parsefield() {
	char name[BUFLEN];
	char type[BUFLEN];
	char decl[2 * BUFLEN];
	struct field *fp;

	if (curstr == NULL || curstr->done)
		error("No current structure to add field to");
	if (! nextfield(name))
		error("No field name");
	if (! nextfield(type))
		error("No field type");
	fp = &curstr->field[curstr->nfields];
	fp->name = savestr(name);
	if (equal(type, "nodeptr")) {
		fp->type = T_NODE;
		sprintf(decl, "union node *%s", name);
	} else if (equal(type, "nodelist")) {
		fp->type = T_NODELIST;
		sprintf(decl, "struct nodelist *%s", name);
	} else if (equal(type, "string")) {
		fp->type = T_STRING;
		sprintf(decl, "char *%s", name);
	} else if (equal(type, "int")) {
		fp->type = T_INT;
		sprintf(decl, "int %s", name);
	} else if (equal(type, "other")) {
		fp->type = T_OTHER;
	} else if (equal(type, "temp")) {
		fp->type = T_TEMP;
	} else {
		error("Unknown type %s", type);
	}
	if (fp->type == T_OTHER || fp->type == T_TEMP) {
		skipbl();
		fp->decl = savestr(linep);
	} else {
		if (*linep)
			error("Garbage at end of line");
		fp->decl = savestr(decl);
	}
	curstr->nfields++;
}


char writer[] = "\
/*\n\
 * This file was generated by the mknodes program.\n\
 */\n\
\n";

output(file)
	char *file;
	{
	FILE *hfile;
	FILE *cfile;
	FILE *patfile;
	int i;
	struct str *sp;
	struct field *fp;
	char *p;

	if ((patfile = fopen(file, "r")) == NULL)
		error("Can't open %s", file);
	if ((hfile = fopen("nodes.h", "w")) == NULL)
		error("Can't create nodes.h");
	if ((cfile = fopen("nodes.c", "w")) == NULL)
		error("Can't create nodes.c");
	fputs(writer, hfile);
	for (i = 0 ; i < ntypes ; i++)
		fprintf(hfile, "#define %s %d\n", nodename[i], i);
	fputs("\n\n\n", hfile);
	for (sp = str ; sp < &str[nstr] ; sp++) {
		fprintf(hfile, "struct %s {\n", sp->tag);
		for (i = sp->nfields, fp = sp->field ; --i >= 0 ; fp++) {
			fprintf(hfile, "      %s;\n", fp->decl);
		}
		fputs("};\n\n\n", hfile);
	}
	fputs("union node {\n", hfile);
	fprintf(hfile, "      int type;\n");
	for (sp = str ; sp < &str[nstr] ; sp++) {
		fprintf(hfile, "      struct %s %s;\n", sp->tag, sp->tag);
	}
	fputs("};\n\n\n", hfile);
	fputs("struct nodelist {\n", hfile);
	fputs("\tstruct nodelist *next;\n", hfile);
	fputs("\tunion node *n;\n", hfile);
	fputs("};\n\n\n", hfile);
	fputs("#ifdef __STDC__\n", hfile);
	fputs("union node *copyfunc(union node *);\n", hfile);
	fputs("void freefunc(union node *);\n", hfile);
	fputs("#else\n", hfile);
	fputs("union node *copyfunc();\n", hfile);
	fputs("void freefunc();\n", hfile);
	fputs("#endif\n", hfile);

	fputs(writer, cfile);
	while (fgets(line, sizeof line, patfile) != NULL) {
		for (p = line ; *p == ' ' || *p == '\t' ; p++);
		if (equal(p, "%SIZES\n"))
			outsizes(cfile);
		else if (equal(p, "%CALCSIZE\n"))
			outfunc(cfile, 1);
		else if (equal(p, "%COPY\n"))
			outfunc(cfile, 0);
		else
			fputs(line, cfile);
	}
}



outsizes(cfile)
	FILE *cfile;
	{
	int i;

	fprintf(cfile, "static const short nodesize[%d] = {\n", ntypes);
	for (i = 0 ; i < ntypes ; i++) {
		fprintf(cfile, "      ALIGN(sizeof (struct %s)),\n", nodestr[i]->tag);
	}
	fprintf(cfile, "};\n");
}


outfunc(cfile, calcsize)
	FILE *cfile;
	{
	struct str *sp;
	struct field *fp;
	int i;

	fputs("      if (n == NULL)\n", cfile);
	if (calcsize)
		fputs("	    return;\n", cfile);
	else
		fputs("	    return NULL;\n", cfile);
	if (calcsize)
		fputs("      funcblocksize += nodesize[n->type];\n", cfile);
	else {
		fputs("      new = funcblock;\n", cfile);
		fputs("      funcblock += nodesize[n->type];\n", cfile);
	}
	fputs("      switch (n->type) {\n", cfile);
	for (sp = str ; sp < &str[nstr] ; sp++) {
		for (i = 0 ; i < ntypes ; i++) {
			if (nodestr[i] == sp)
				fprintf(cfile, "      case %s:\n", nodename[i]);
		}
		for (i = sp->nfields ; --i >= 1 ; ) {
			fp = &sp->field[i];
			switch (fp->type) {
			case T_NODE:
				if (calcsize) {
					indent(12, cfile);
					fprintf(cfile, "calcsize(n->%s.%s);\n",
						sp->tag, fp->name);
				} else {
					indent(12, cfile);
					fprintf(cfile, "new->%s.%s = copynode(n->%s.%s);\n",
						sp->tag, fp->name, sp->tag, fp->name);
				}
				break;
			case T_NODELIST:
				if (calcsize) {
					indent(12, cfile);
					fprintf(cfile, "sizenodelist(n->%s.%s);\n",
						sp->tag, fp->name);
				} else {
					indent(12, cfile);
					fprintf(cfile, "new->%s.%s = copynodelist(n->%s.%s);\n",
						sp->tag, fp->name, sp->tag, fp->name);
				}
				break;
			case T_STRING:
				if (calcsize) {
					indent(12, cfile);
					fprintf(cfile, "funcstringsize += strlen(n->%s.%s) + 1;\n",
						sp->tag, fp->name);
				} else {
					indent(12, cfile);
					fprintf(cfile, "new->%s.%s = nodesavestr(n->%s.%s);\n",
						sp->tag, fp->name, sp->tag, fp->name);
				}
				break;
			case T_INT:
			case T_OTHER:
				if (! calcsize) {
					indent(12, cfile);
					fprintf(cfile, "new->%s.%s = n->%s.%s;\n",
						sp->tag, fp->name, sp->tag, fp->name);
				}
				break;
			}
		}
		indent(12, cfile);
		fputs("break;\n", cfile);
	}
	fputs("      };\n", cfile);
	if (! calcsize)
		fputs("      new->type = n->type;\n", cfile);
}


indent(amount, fp)
	FILE *fp;
	{
	while (amount >= 8) {
		putc('\t', fp);
		amount -= 8;
	}
	while (--amount >= 0) {
		putc(' ', fp);
	}
}


int
nextfield(buf)
	char *buf;
	{
	register char *p, *q;

	p = linep;
	while (*p == ' ' || *p == '\t')
		p++;
	q = buf;
	while (*p != ' ' && *p != '\t' && *p != '\0')
		*q++ = *p++;
	*q = '\0';
	linep = p;
	return (q > buf);
}


skipbl() {
	while (*linep == ' ' || *linep == '\t')
		linep++;
}


int
readline() {
	register char *p;

	if (fgets(line, 1024, infp) == NULL)
		return 0;
	for (p = line ; *p != '#' && *p != '\n' && *p != '\0' ; p++);
	while (p > line && (p[-1] == ' ' || p[-1] == '\t'))
		p--;
	*p = '\0';
	linep = line;
	linno++;
	if (p - line > BUFLEN)
		error("Line too long");
	return 1;
}



error(msg, a1, a2, a3, a4, a5, a6)
	char *msg;
	{
	fprintf(stderr, "line %d: ", linno);
	fprintf(stderr, msg, a1, a2, a3, a4, a5, a6);
	putc('\n', stderr);
	exit(2);
}



char *
savestr(s)
	char *s;
	{
	register char *p;
	char *malloc();

	if ((p = malloc(strlen(s) + 1)) == NULL)
		error("Out of space");
	strcpy(p, s);
	return p;
}
