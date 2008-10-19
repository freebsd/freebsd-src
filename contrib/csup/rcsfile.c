/*-
 * Copyright (c) 2007-2008, Ulf Lilleengen <lulf@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include "diff.h"
#include "misc.h"
#include "keyword.h"
#include "rcsfile.h"
#include "rcsparse.h"
#include "stream.h"
#include "proto.h"
#include "queue.h"

/*
 * RCS parser library. This is the part of the library that handles the
 * importing, editing and exporting of RCS files. It currently supports only the
 * part of the RCS file specification that is needed for csup (for instance,
 * newphrases are not supported), and assumes that you can store the whole RCS
 * file in memory.
 */

/*
 * Linked list for string tokens.
 */
struct string {
	char *str;
	STAILQ_ENTRY(string) string_next;
};

/*
 * Linked list of tags and revision numbers, in the RCS file header.
 */
struct tag {
	char *tag;
	char *revnum;
	STAILQ_ENTRY(tag) tag_next;
};

/*
 * A RCS delta. The delta is identified by a revision number, and contains the
 * most important RCS attributes that is needed by csup. It also contains
 * pointers to other nodes in the RCS file delta structure.
 */
struct delta {
	char *revdate;
	char *revnum;
	char *author;
	char *state;
	struct buf *log;
	struct buf *text;
	int placeholder;
	struct delta *diffbase;
	struct delta *prev;

	LIST_ENTRY(delta) delta_next;
	STAILQ_ENTRY(delta) delta_prev;
	LIST_ENTRY(delta) table_next;
	STAILQ_ENTRY(delta) stack_next;
	STAILQ_HEAD(, branch) branchlist;
	LIST_ENTRY(delta) branch_next_date;
};

/*
 * A branch data structure containing information about deltas in the branch as
 * well as a base revision number.
 */
struct branch {
	char *revnum;
	LIST_HEAD(, delta) deltalist; /* Next delta in our branch. */
	STAILQ_ENTRY(branch) branch_next;
};

/*
 * The rcsfile structure is the "main" structure of the RCS parser library. It
 * contains administrative data as well as pointers to the deltas within the
 * file.
 */
struct rcsfile {
	char *name;
	char *head;
	char *branch;	/* Default branch. */
	char *cvsroot;
	char *colltag;
	STAILQ_HEAD(, string) accesslist;
	STAILQ_HEAD(, tag) taglist;
	int strictlock;
	char *comment;
	int expand;
	struct branch *trunk; /* The tip delta. */

	LIST_HEAD(, delta) deltatable;
	LIST_HEAD(, delta) deltatable_dates;

	char *desc;
};

static void		 rcsfile_freedelta(struct delta *);
static void		 rcsfile_insertdelta(struct branch *, struct delta *,
			     int);
static struct delta	*rcsfile_createdelta(char *);
static int		 rcsfile_write_deltatext(struct rcsfile *,
			     struct stream *);
static int		 rcsfile_puttext(struct rcsfile *, struct stream *,
			     struct delta *, struct delta *);
static struct branch	*rcsfile_getbranch(struct rcsfile *, char *);
static void		 rcsfile_insertsorteddelta(struct rcsfile *,
			     struct delta *);
static struct stream 	*rcsfile_getdeltatext(struct rcsfile *, struct delta *,
			     struct buf **);


/* Space formatting of RCS file. */
const char *head_space = "\t";
const char *branch_space = "\t";
const char *tag_space = "\t";
const char *date_space = "\t";
const char *auth_space = "\t";
const char *state_space = "\t";
const char *next_space = "\t";
const char *branches_space = "\t";
const char *comment_space ="\t";

void print_stream(struct stream *);

/* Print the contents of a stream, for debugging. */
void
print_stream(struct stream *s)
{
	char *line;

	line = stream_getln(s, NULL);
	while (line != NULL) {
		fprintf(stderr, "%s\n", line);
		line = stream_getln(s, NULL);
	}
	fprintf(stderr, "\n");
}

/*
 * Parse rcsfile from path and return a pointer to it.
 */
struct rcsfile *
rcsfile_frompath(char *path, char *name, char *cvsroot, char *colltag)
{
	FILE *infp;
	struct rcsfile *rf;
	char one[10] = "1";
	int error;

	if (path == NULL || name == NULL || cvsroot == NULL || colltag == NULL)
		return (NULL);

	rf = xmalloc(sizeof(struct rcsfile));
	rf->name = xstrdup(name);
	rf->cvsroot = xstrdup(cvsroot);
	rf->colltag = xstrdup(colltag);
	/*fprintf(stderr, "Doing file %s\n", rf->name);*/

	/* Initialize head branch. */
	rf->trunk = xmalloc(sizeof(struct branch));
	rf->trunk->revnum = xstrdup(one);
	LIST_INIT(&rf->trunk->deltalist);
	/* Initialize delta list. */
	LIST_INIT(&rf->deltatable);
	/* Initialize tag list. */
	STAILQ_INIT(&rf->taglist);
	/* Initialize accesslist. */
	STAILQ_INIT(&rf->accesslist);

	/* Initialize all fields. */
	rf->head = NULL;
	rf->branch = NULL;
	rf->strictlock = 0;
	rf->comment = NULL;
	rf->expand = -1;
	rf->desc = NULL;

	infp = fopen(path, "r");
	if (infp == NULL) {
		lprintf(-1, "Cannot open \"%s\": %s\n", path, strerror(errno));
		rcsfile_free(rf);
		return (NULL);
	}
	error = rcsparse_run(rf, infp);
	fclose(infp);
	if (error) {
		lprintf(-1, "Error parsing \"%s\"\n", name);
		rcsfile_free(rf);
		return (NULL);
	}
	return (rf);
}

/*
 * Write content of rcsfile to server. Assumes we have a complete RCS file
 * loaded.
 */
int
rcsfile_send_details(struct rcsfile *rf, struct stream *wr)
{
	struct delta *d;
	struct tag *t;
	int error;

	assert(rf != NULL);

	error = proto_printf(wr, "V %s\n", rf->name);
	if (error)
		return(error);

	/* Write default branch. */
	if (rf->branch == NULL)
		error = proto_printf(wr, "b\n");
	else
		error = proto_printf(wr, "B %s\n", rf->branch);
	if (error)
		return(error);

	/* Write deltas to server. */
	error = proto_printf(wr, "D\n");
	if (error)
		return(error);

	LIST_FOREACH(d, &rf->deltatable, table_next) {
		error = proto_printf(wr, "%s %s\n", d->revnum, d->revdate);
		if (error)
			return(error);
	}
	error = proto_printf(wr, ".\n");

	if (error)
		return(error);
	/* Write expand. */
	if (rf->expand >= 0) {
		error = proto_printf(wr, "E %s\n", keyword_encode_expand(rf->expand));
		if (error)
			return(error);
	}

	/* Write tags to server. */
	error = proto_printf(wr, "T\n");
	if (error)
		return(error);
	STAILQ_FOREACH(t, &rf->taglist, tag_next) {
		error = proto_printf(wr, "%s %s\n", t->tag, t->revnum);
		if (error)
			return(error);
	}
	error = proto_printf(wr, ".\n");
	if (error)
		return(error);
	error = proto_printf(wr, ".\n");
	return (error);
}

/*
 * Write a RCS file to disk represented by the destination stream. Keep track of
 * deltas with a stack and an inverted stack.
 */
int
rcsfile_write(struct rcsfile *rf, struct stream *dest)
{
	STAILQ_HEAD(, delta) deltastack;
	STAILQ_HEAD(, delta) deltalist_inverted;
	struct tag *t;
	struct branch *b;
	struct delta *d, *d_tmp, *d_next;
	int error;

	/* First write head. */
	d = LIST_FIRST(&rf->trunk->deltalist);
	stream_printf(dest, "head%s%s;\n", head_space, d->revnum);

	/* Write branch, if we have. */
	if (rf->branch != NULL)
		stream_printf(dest, "branch%s%s;\n", branch_space, rf->branch);

	/* Write access. */
	stream_printf(dest, "access");
#if 0
	if (!STAILQ_EMPTY(&rf->accesslist)) {
		/*
		 * XXX: Write out access. This doesn't seem to be necessary for
		 * the time being.
		 */
	}
#endif
	stream_printf(dest, ";\n");

	/* Write out taglist. */
	stream_printf(dest, "symbols");
	if (!STAILQ_EMPTY(&rf->taglist)) {
		STAILQ_FOREACH(t, &rf->taglist, tag_next) {
			stream_printf(dest, "\n%s%s:%s", tag_space, t->tag,
			    t->revnum);
		}
	}
	stream_printf(dest, ";\n");

	/* Write out locks and strict. */
	stream_printf(dest, "locks;");
	if (rf->strictlock)
		stream_printf(dest, " strict;");
	stream_printf(dest, "\n");

	/* Write out the comment. */
	if (rf->comment != NULL)
		stream_printf(dest, "comment%s%s;\n", comment_space, rf->comment);

	stream_printf(dest, "\n\n");

	/* 
	 * Write out deltas. We use a stack where we push the appropriate deltas
	 * that is to be written out during the loop.
	 */
	STAILQ_INIT(&deltastack);
	d = LIST_FIRST(&rf->trunk->deltalist);
	STAILQ_INSERT_HEAD(&deltastack, d, stack_next);
	while (!STAILQ_EMPTY(&deltastack)) {
		d = STAILQ_FIRST(&deltastack);
		STAILQ_REMOVE_HEAD(&deltastack, stack_next);
		/* Do not write out placeholders just to be safe. */
		if (d->placeholder)
			continue;
		stream_printf(dest, "%s\n", d->revnum);
		stream_printf(dest, "date%s%s;%sauthor %s;%sstate",
		    date_space, d->revdate, auth_space, d->author,
		    state_space);
		if (d->state != NULL)
			stream_printf(dest, " %s", d->state);
		stream_printf(dest, ";\n");
		stream_printf(dest, "branches");
		/*
		 * Write out our branches. Add them to a reversed list for use
		 * later when we write out the text.
		 */
		STAILQ_INIT(&deltalist_inverted);
		STAILQ_FOREACH(b, &d->branchlist, branch_next) {
			d_tmp = LIST_FIRST(&b->deltalist);
			STAILQ_INSERT_HEAD(&deltalist_inverted, d_tmp, delta_prev);
			STAILQ_INSERT_HEAD(&deltastack, d_tmp, stack_next);
		}

		/* Push branch heads on stack. */
		STAILQ_FOREACH(d_tmp, &deltalist_inverted, delta_prev) {
			if (d_tmp == NULL)
				err(1, "empty branch!");
			stream_printf(dest, "\n%s%s", branches_space,
			    d_tmp->revnum);
		}
		stream_printf(dest, ";\n");

		stream_printf(dest, "next%s", next_space);
		/* Push next delta on stack. */
		d_next = LIST_NEXT(d, delta_next);
		if (d_next != NULL) {
			stream_printf(dest, "%s", d_next->revnum);
			STAILQ_INSERT_HEAD(&deltastack, d_next, stack_next);
		}
		stream_printf(dest, ";\n\n");
	}
	stream_printf(dest, "\n");
	/* Write out desc. */
	stream_printf(dest, "desc\n@@");
	d = LIST_FIRST(&rf->trunk->deltalist);

	/* 
	 * XXX: We do not take as much care as cvsup to cope with hand-hacked
	 * RCS-files, and therefore we'll just let them be updated. If having
	 * them correct is important, it will be catched by the checksum anyway.
	 */

	/* Write out deltatexts. */
	error = rcsfile_write_deltatext(rf, dest);
	stream_printf(dest, "\n");
	return (error);
}

/*
 * Write out deltatexts of a delta and it's subbranches recursively.
 */
int
rcsfile_write_deltatext(struct rcsfile *rf, struct stream *dest)
{
	STAILQ_HEAD(, delta) deltastack;
	LIST_HEAD(, delta) branchlist_datesorted;
	struct stream *in;
	struct delta *d, *d_tmp, *d_next, *d_tmp2, *d_tmp3;
	struct branch *b;
	char *line;
	size_t size;
	int error;

	error = 0;
	STAILQ_INIT(&deltastack);
	d = LIST_FIRST(&rf->trunk->deltalist);
	d->prev = NULL;
	STAILQ_INSERT_HEAD(&deltastack, d, stack_next);
	while (!STAILQ_EMPTY(&deltastack)) {
		d = STAILQ_FIRST(&deltastack);
		STAILQ_REMOVE_HEAD(&deltastack, stack_next);
		/* Do not write out placeholders just to be safe. */
		if (d->placeholder)
			return (0);
		stream_printf(dest, "\n\n\n%s\n", d->revnum);
		stream_printf(dest, "log\n@");
		in = stream_open_buf(d->log);
		line = stream_getln(in, &size);
		while (line != NULL) {
			stream_write(dest, line, size);
			line = stream_getln(in, &size);
		}
		stream_close(in);
		stream_printf(dest, "@\n");
		stream_printf(dest, "text\n@");
		error = rcsfile_puttext(rf, dest, d, d->prev);
		if (error)
			return (error);
		stream_printf(dest, "@");
	
		LIST_INIT(&branchlist_datesorted);
		d_next = LIST_NEXT(d, delta_next);
		if (d_next != NULL) {
			d_next->prev = d;
			/*
			 * If it's trunk, treat it like the oldest, if not treat
			 * it like a child.
			 */
			if (rcsrev_istrunk(d_next->revnum))
				STAILQ_INSERT_HEAD(&deltastack, d_next, stack_next);
			else
				LIST_INSERT_HEAD(&branchlist_datesorted, d_next,
				    branch_next_date);
		}

		/*
		 * First, we need to sort our branches based on their date to
		 * take into account some self-hacked RCS files.
		 */
		STAILQ_FOREACH(b, &d->branchlist, branch_next) {
			d_tmp = LIST_FIRST(&b->deltalist);
			if (LIST_EMPTY(&branchlist_datesorted)) {
				LIST_INSERT_HEAD(&branchlist_datesorted, d_tmp,
				    branch_next_date);
				continue;
			}

			d_tmp2 = LIST_FIRST(&branchlist_datesorted);
			if (rcsnum_cmp(d_tmp->revdate, d_tmp2->revdate) < 0) {
				LIST_INSERT_BEFORE(d_tmp2, d_tmp, branch_next_date);
				continue;
			}
			while ((d_tmp3 = LIST_NEXT(d_tmp2, branch_next_date))
			    != NULL) {
				if (rcsnum_cmp(d_tmp->revdate, d_tmp3->revdate)
				    < 0)
					break;
				d_tmp2 = d_tmp3;
			}
			LIST_INSERT_AFTER(d_tmp2, d_tmp, branch_next_date);
		}
		/* 
		 * Invert the deltalist of a branch, since we're writing them
		 * the opposite way. 
		 */
		LIST_FOREACH(d_tmp, &branchlist_datesorted, branch_next_date) {
                        d_tmp->prev = d;
			STAILQ_INSERT_HEAD(&deltastack, d_tmp, stack_next);
		}
	}
	return (0);
}

/*
 * Generates text given a delta and a diffbase.
 */
static int
rcsfile_puttext(struct rcsfile *rf, struct stream *dest, struct delta *d,
    struct delta *diffbase)
{
	struct stream *in, *rd, *orig;
	struct keyword *k;
	struct diffinfo dibuf, *di;
	struct buf *b;
	char *line;
	int error;
	size_t size;

	di = &dibuf;
	b = NULL;
	error = 0;

	/* Write if the diffbase is the previous */
	if (d->diffbase == diffbase) {

		/* Write out the text. */
		in = stream_open_buf(d->text);
		line = stream_getln(in, &size);
		while (line != NULL) {
			stream_write(dest, line, size);
			line = stream_getln(in, &size);
		}
		stream_close(in);
	/* We need to apply diff to produce text, this is probably HEAD. */
	} else if (diffbase == NULL) {
		/* Apply diff. */
		orig = rcsfile_getdeltatext(rf, d, &b);
		if (orig == NULL) {
			error = -1;
			goto cleanup;
		}
		line = stream_getln(orig, &size);
		while (line != NULL) {
			stream_write(dest, line, size);
			line = stream_getln(orig, &size);
		}
		stream_close(orig);
	/* 
	 * A new head was probably added, and now the previous HEAD must be
	 * changed to include the diff instead.
	 */
	} else if (diffbase->diffbase == d) {
		/* Get reverse diff. */
		orig = rcsfile_getdeltatext(rf, d, &b);
		if (orig == NULL) {
			error = -1;
			goto cleanup;
		}
		rd = stream_open_buf(diffbase->text);
		di->di_rcsfile = rf->name;
		di->di_cvsroot = rf->cvsroot;
		di->di_revnum = d->revnum;
		di->di_revdate = d->revdate;
		di->di_author = d->author;
		di->di_tag = rf->colltag;
		di->di_state = d->state;
		di->di_expand = rf->expand;
		k = keyword_new();
	
		rd = stream_open_buf(diffbase->text);
		error = diff_reverse(rd, orig, dest, k, di);
		if (error) {
			fprintf(stderr, "Error applying reverse diff: %d\n",
			    error);
			goto cleanup;
		}
		keyword_free(k);
		stream_close(rd);
		stream_close(orig);
	}
cleanup:
	if (b != NULL)
		buf_free(b);
	return (error);
}

/*
 * Return a stream with an applied diff of a delta.
 * XXX: extra overhead on the last apply. Could write directly to file, but
 * makes things complicated though.
 */
static struct stream *
rcsfile_getdeltatext(struct rcsfile *rf, struct delta *d, struct buf **buf_dest)
{
	struct diffinfo dibuf, *di;
	struct stream *orig, *dest, *rd;
	struct buf *buf_orig;
	struct keyword *k;
	int error;

	buf_orig = NULL;
	error = 0;

	/* If diffbase is NULL or we are head (the old head), we have a normal complete deltatext. */
	if (d->diffbase == NULL && !strcmp(rf->head, d->revnum)) {
		orig = stream_open_buf(d->text);
		return (orig);
	}

	di = &dibuf;
	/* If not, we need to apply our diff to that of our diffbase. */
	orig = rcsfile_getdeltatext(rf, d->diffbase, &buf_orig);
	if (orig == NULL)
		return (NULL);

	/*
	 * Now that we are sure we have a complete deltatext in ret, let's apply
	 * our diff to it.
	 */
	*buf_dest = buf_new(128);
	dest = stream_open_buf(*buf_dest);

	di->di_rcsfile = rf->name;
	di->di_cvsroot = rf->cvsroot;
	di->di_revnum = d->revnum;
	di->di_revdate = d->revdate;
	di->di_author = d->author;
	di->di_tag = rf->colltag;
	di->di_state = d->state;
	di->di_expand = rf->expand;
	rd = stream_open_buf(d->text);
	k = keyword_new();
	error = diff_apply(rd, orig, dest, k, di, 0);
	stream_flush(dest);
	stream_close(rd);
	stream_close(orig);
	stream_close(dest);
	keyword_free(k);
	if (buf_orig != NULL)
		buf_free(buf_orig);
	if (error) {
		lprintf(-1, "Error applying diff: %d\n", error);
		return (NULL);
	}
	
	/* Now reopen the stream for the reading. */
	dest = stream_open_buf(*buf_dest);
	return (dest);
}

/* Print content of rcsfile. Useful for debugging. */
void
rcsfile_print(struct rcsfile *rf)
{
	struct delta *d;
	struct tag *t;
	struct string *s;
	char *line;
	struct stream *in;

	printf("\n");
	if (rf->name != NULL)
		printf("name: '%s'\n", rf->name);
	if (rf->head != NULL)
		printf("head: '%s'\n", rf->head);
	if (rf->branch != NULL)
		printf("branch: '%s'\n", rf->branch);
	printf("Access: ");
	STAILQ_FOREACH(s, &rf->accesslist, string_next) {
		printf("'%s' ", s->str);
	}
	printf("\n");

	/* Print all tags. */
	STAILQ_FOREACH(t, &rf->taglist, tag_next) {
		printf("Tag: ");
		if (t->tag != NULL)
			printf("name: %s ", t->tag);
		if (t->revnum != NULL)
			printf("rev: %s", t->revnum);
		printf("\n");
	}

	if (rf->strictlock)
		printf("Strict!\n");
	if (rf->comment != NULL)
		printf("comment: '%s'\n", rf->comment);
	if (rf->expand >= 0)
		printf("expand: '%s'\n", keyword_encode_expand(rf->expand));
	
	/* Print all deltas. */
	LIST_FOREACH(d, &rf->deltatable, table_next) {
		printf("Delta: ");
		if (d->revdate != NULL)
			printf("date: %s ", d->revdate);
		if (d->revnum != NULL)
			printf("rev: %s", d->revnum);
		if (d->author != NULL)
			printf("author: %s", d->author);
		if (d->state != NULL)
			printf("state: %s", d->state);

		printf("Text:\n");
		in = stream_open_buf(d->text);
		line = stream_getln(in, NULL);
		while (line != NULL) {
			lprintf(1, "TEXT: %s\n", line);
			line = stream_getln(in, NULL);
		}
		stream_close(in);
		printf("branches: ");
		printf("\n");
	}

	if (rf->desc != NULL)
		printf("desc: '%s'\n", rf->desc);
}

/* Free all memory associated with a struct rcsfile. */
void
rcsfile_free(struct rcsfile *rf)
{
	struct delta *d;
	struct tag *t;
	struct string *s;

	if (rf->name != NULL)
		free(rf->name);
	if (rf->head != NULL)
		free(rf->head);
	if (rf->branch != NULL)
		free(rf->branch);
	if (rf->cvsroot != NULL)
		free(rf->cvsroot);
	if (rf->colltag != NULL)
		free(rf->colltag);

	/* Free all access ids. */
	while (!STAILQ_EMPTY(&rf->accesslist)) {
		s = STAILQ_FIRST(&rf->accesslist);
		STAILQ_REMOVE_HEAD(&rf->accesslist, string_next);
		if (s->str != NULL)
			free(s->str);
		free(s);
	}

	/* Free all tags. */
	while (!STAILQ_EMPTY(&rf->taglist)) {
		t = STAILQ_FIRST(&rf->taglist);
		STAILQ_REMOVE_HEAD(&rf->taglist, tag_next);
		if (t->tag != NULL)
			free(t->tag);
		if (t->revnum != NULL)
			free(t->revnum);
		free(t);
	}

	if (rf->comment != NULL)
		free(rf->comment);

	/* Free all deltas in global list */
	while (!LIST_EMPTY(&rf->deltatable)) {
		d = LIST_FIRST(&rf->deltatable);
		LIST_REMOVE(d, table_next);
		rcsfile_freedelta(d);
	}

	/* Free global branch. */
	if (rf->trunk->revnum != NULL)
		free(rf->trunk->revnum);
	free(rf->trunk);

	if (rf->desc != NULL)
		free(rf->desc);

	free(rf);
}

/*
 * Free a RCS delta.
 */
static void
rcsfile_freedelta(struct delta *d)
{
	struct branch *b;

	if (d->revdate != NULL)
		free(d->revdate);
	if (d->revnum != NULL)
		free(d->revnum);
	if (d->author != NULL)
		free(d->author);
	if (d->state != NULL)
		free(d->state);
	if (d->log != NULL)
		buf_free(d->log);
	if (d->text != NULL)
		buf_free(d->text);

	/* Free all subbranches of a delta. */
	/* XXX: Is this ok? Since the branchpoint is removed, there is no good
	 * reason for the branch to exists, but we might still have deltas in
	 * these branches.
	 */
	while (!STAILQ_EMPTY(&d->branchlist)) {
		b = STAILQ_FIRST(&d->branchlist);
		STAILQ_REMOVE_HEAD(&d->branchlist, branch_next);
		free(b->revnum);
		free(b);
	}
	free(d);
}

/*
 * Functions for editing RCS deltas.
 */

/* Add a new entry to the access list. */
void
rcsfile_addaccess(struct rcsfile *rf, char *id)
{
	struct string *s;

	s = xmalloc(sizeof(struct string));
	s->str = xstrdup(id);
	STAILQ_INSERT_TAIL(&rf->accesslist, s, string_next);
}

/* Add a tag to a RCS file. */
void
rcsfile_addtag(struct rcsfile *rf, char *tag, char *revnum)
{
	struct tag *t;

	t = xmalloc(sizeof(struct tag));
	t->tag = xstrdup(tag);
	t->revnum = xstrdup(revnum);

	STAILQ_INSERT_HEAD(&rf->taglist, t, tag_next);
}

/* Import a tag to a RCS file. */
void
rcsfile_importtag(struct rcsfile *rf, char *tag, char *revnum)
{
	struct tag *t;

	t = xmalloc(sizeof(struct tag));
	t->tag = xstrdup(tag);
	t->revnum = xstrdup(revnum);

	STAILQ_INSERT_TAIL(&rf->taglist, t, tag_next);
}

/*
 * Delete a revision from the global delta list and the branch it is in. Csup
 * will tell us to delete the tags involved.
 */
void
rcsfile_deleterev(struct rcsfile *rf, char *revname)
{
	struct delta *d;

	d = rcsfile_getdelta(rf, revname);
	LIST_REMOVE(d, delta_next);
	LIST_REMOVE(d, table_next);
	rcsfile_freedelta(d);
}

/* Delete a tag from the tag list. */
void
rcsfile_deletetag(struct rcsfile *rf, char *tag, char *revnum)
{
	struct tag *t;

	STAILQ_FOREACH(t, &rf->taglist, tag_next) {
		if ((strcmp(tag, t->tag) == 0) &&
		    (strcmp(revnum, t->revnum) == 0)) {
			STAILQ_REMOVE(&rf->taglist, t, tag, tag_next);
			free(t->tag);
			free(t->revnum);
			free(t);
			return;
		}
	}
}

/* 
 * Searches the global deltalist for a delta.
 */
struct delta *
rcsfile_getdelta(struct rcsfile *rf, char *revnum)
{
	struct delta *d;

	LIST_FOREACH(d, &rf->deltatable, table_next) {
		if (strcmp(revnum, d->revnum) == 0)
			return (d);
	}
	return (NULL);
}

/* Set rcsfile head. */
void
rcsfile_setval(struct rcsfile *rf, int field, char *val)
{

	switch (field) {
		case RCSFILE_HEAD:
			if (rf->head != NULL)
				free(rf->head);
			rf->head = xstrdup(val);
		break;
		case RCSFILE_BRANCH:
			if (rf->branch != NULL)
				free(rf->branch);
			rf->branch = (val == NULL) ? NULL : xstrdup(val);
		break;
		case RCSFILE_STRICT:
			if (val != NULL)
				rf->strictlock = 1;
		break;
		case RCSFILE_COMMENT:
			if (rf->comment != NULL)
				free(rf->comment);
			rf->comment = xstrdup(val);
		break;
		case RCSFILE_EXPAND:
			rf->expand = keyword_decode_expand(val);
		break;
		case RCSFILE_DESC:
			if (rf->desc != NULL)
				free(rf->desc);
			rf->desc = xstrdup(val);
		break;
		default:
			lprintf(-1, "Setting invalid RCSfile value.\n");
		break;
	}
}

/* Create and initialize a delta. */
static struct delta *
rcsfile_createdelta(char *revnum)
{
	struct delta *d;

	d = xmalloc(sizeof(struct delta));
	d->revnum = xstrdup(revnum);
	d->revdate = NULL;
	d->state = NULL;
	d->author = NULL;
	/* XXX: default. */
	d->log = buf_new(128);
	d->text = buf_new(128);
	d->diffbase = NULL;

	STAILQ_INIT(&d->branchlist);
	return (d);
}

/* Add a delta to a imported delta tree. Used by the updater. */
struct delta *
rcsfile_addelta(struct rcsfile *rf, char *revnum, char *revdate, char *author,
    char *diffbase)
{
	struct branch *b;
	struct delta *d, *d_bp, *d_next;
	char *brev, *bprev;
	int trunk;

	d_next = NULL;
	d = rcsfile_getdelta(rf, revnum);
	if (d != NULL) {
		lprintf(-1, "Delta %s already exists!\n", revnum);
		return (NULL);
	}
	d = rcsfile_createdelta(revnum);
	d->placeholder = 0;
	d->revdate = xstrdup(revdate);
	d->author = xstrdup(author);
	d->diffbase = rcsfile_getdelta(rf, diffbase);

	/* If it's trunk, insert it in the head branch list. */
	b = rcsrev_istrunk(d->revnum) ? rf->trunk : rcsfile_getbranch(rf,
	    d->revnum);

	/*
	 * We didn't find a branch, check if we can find a branchpoint and
	 * create a branch there. 
	 */
	if (b == NULL) {
		brev = rcsrev_prefix(d->revnum);
		bprev = rcsrev_prefix(brev);

		d_bp = rcsfile_getdelta(rf, bprev);
		free(bprev);
		if (d_bp == NULL) {
			lprintf(-1, "No branch point for adding delta %s\n",
			    d->revnum);
			return (NULL);
		}

		/* Create the branch and insert in delta. */
		b = xmalloc(sizeof(struct branch));
		b->revnum = brev;
		LIST_INIT(&b->deltalist);
		STAILQ_INSERT_HEAD(&d_bp->branchlist, b, branch_next);
	}

	/* Insert both into the tree, and into the lookup list. */
	trunk = rcsrev_istrunk(d->revnum);
	rcsfile_insertdelta(b, d, trunk);
	rcsfile_insertsorteddelta(rf, d);
	return (d);
}

/* Adds a delta to a rcsfile struct. Used by the parser. */
void
rcsfile_importdelta(struct rcsfile *rf, char *revnum, char *revdate, char *author,
    char *state, char *next)
{
	struct branch *b;
	struct delta *d, *d_bp, *d_next;
	char *brev, *bprev;
	int trunk;

	d_next = NULL;
	d = rcsfile_getdelta(rf, revnum);

	if (d == NULL) {
		/* If not, we'll just create a new entry. */
		d = rcsfile_createdelta(revnum);
		d->placeholder = 0;
	} else {
		if (d->placeholder == 0) {
			lprintf(-1, "Trying to import already existing delta\n");
			return;
		}
	}
	/*
	 * If already exists, assume that only revnum is filled out, and set the
	 * rest of the fields. This should be an OK assumption given that we can
	 * be sure internally that the structure is sufficiently initialized so
	 * we won't have any unfreed memory.
	 */
	d->revdate = xstrdup(revdate);
	d->author = xstrdup(author);
	if (state != NULL)
		d->state = xstrdup(state);

	/* If we have a next, create a placeholder for it. */
	if (next != NULL) {
		d_next = rcsfile_createdelta(next);
		d_next->placeholder = 1;
		/* Diffbase should be the previous. */
		d_next->diffbase = d;
	}

	/* If it's trunk, insert it in the head branch list. */
	b = rcsrev_istrunk(d->revnum) ? rf->trunk : rcsfile_getbranch(rf,
	    d->revnum);

	/*
	 * We didn't find a branch, check if we can find a branchpoint and
	 * create a branch there. 
	 */
	if (b == NULL) {
		brev = rcsrev_prefix(d->revnum);
		bprev = rcsrev_prefix(brev);

		d_bp = rcsfile_getdelta(rf, bprev);
		free(bprev);
		if (d_bp == NULL) {
			lprintf(-1, "No branch point for adding delta %s\n",
			    d->revnum);
			return;
		}

		/* Create the branch and insert in delta. */
		b = xmalloc(sizeof(struct branch));
		b->revnum = brev;
		LIST_INIT(&b->deltalist);
		STAILQ_INSERT_HEAD(&d_bp->branchlist, b, branch_next);
	}

	/* Insert if not a placeholder. */ 
	if (!d->placeholder) {
		/*fprintf(stderr, "Insert %s\n", d->revnum);*/
		/* Insert both into the tree, and into the lookup list. */
		if (rcsrev_istrunk(d->revnum))
			rcsfile_insertdelta(b, d, 1);
		else {
			rcsfile_insertdelta(b, d, 0);
			/* 
			 * On import we need to set the diffbase to our
			 * branchpoint for writing out later.
			 * XXX: this could perhaps be done at a better time.
			 */
			if (LIST_FIRST(&b->deltalist) == d) {
				brev = rcsrev_prefix(d->revnum);
				bprev = rcsrev_prefix(brev);
				d_bp = rcsfile_getdelta(rf, bprev);
				/* This should really not happen. */
				assert(d_bp != NULL);
				d->diffbase = d_bp;
				free(brev);
				free(bprev);
			}
		}
		rcsfile_insertsorteddelta(rf, d);
	} else /* Not a placeholder anymore. */ {
		d->placeholder = 0;
		/* Put it into the tree. */
		trunk = rcsrev_istrunk(d->revnum);
		rcsfile_insertdelta(b, d, trunk);
	}

	/* If we have a next, insert the placeholder into the lookup list. */
	if (d_next != NULL)
		rcsfile_insertsorteddelta(rf, d_next);
}

/*
 * Find the branch of a revision number.
 */
static struct branch *
rcsfile_getbranch(struct rcsfile *rf, char *revnum)
{
	struct branch *b;
	struct delta *d;
	char *branchrev;
	char *bprev;

	branchrev = rcsrev_prefix(revnum);
	bprev = rcsrev_prefix(branchrev);
	d = rcsfile_getdelta(rf, bprev);
	free(bprev);
	STAILQ_FOREACH(b, &d->branchlist, branch_next) {
		if(rcsnum_cmp(b->revnum, branchrev) == 0) {
			free(branchrev);
			return (b);
		}
	}
	free(branchrev);
	return (NULL);
}

#if 0
/* Add a new branch to a delta. */
void
rcsfile_addbranch(struct rcsfile *rf, char *branch)
{
	struct delta *d;
	struct branch *b;
	char *branchrev, *deltarev;
	int trunk;

	/* 
	 * Branchrev is our branches revision, the delta actual delta will be
	 * taken care of later.
	 */
	branchrev = rcsrev_prefix(branch);
	deltarev = rcsrev_prefix(branchrev);

	/* XXX: Could we refer to a delta that is not added yet? If we're
	 * refferring to branches without having been added before, this could
	 * happen in the head branch.
	 */
	/*fprintf(stderr, "Add branch %s to delta %s\n", branchrev, deltarev);*/
	d = rcsfile_getdelta(rf, deltarev);
	if (d == NULL) {
		/* We must create a placeholder for the delta holding the
		 * branch. */
		d = rcsfile_createdelta(deltarev);
		d->placeholder = 1;
		/* XXX: Can we assume this branch exists? */
		trunk = rcsrev_istrunk(d->revnum);
		b = trunk ? rf->trunk : rcsfile_getbranch(rf, d->revnum);
		rcsfile_insertdelta(b, d, trunk);
		rcsfile_insertsorteddelta(rf, d);
	}
	b = xmalloc(sizeof(struct branch));
	b->revnum = branchrev;
	LIST_INIT(&b->deltalist);
	STAILQ_INSERT_HEAD(&d->branchlist, b, branch_next);
	/* Free only deltarev, branchrev is used by branch. */
	free(deltarev);
}
#endif

/*
 * Insert a delta into the correct place in the table of the rcsfile. Sorted by
 * date.
 */
static void
rcsfile_insertsorteddelta(struct rcsfile *rf, struct delta *d)
{
	struct delta *d2;

	/* If it's empty, insert into head. */
	if (LIST_EMPTY(&rf->deltatable)) {
		LIST_INSERT_HEAD(&rf->deltatable, d, table_next);
		return;
	}

	/* Just put it in before the revdate that is lower. */
	LIST_FOREACH(d2, &rf->deltatable, table_next) {
		if (rcsnum_cmp(d->revnum, d2->revnum) <= 0) {
			LIST_INSERT_BEFORE(d2, d, table_next);
			return;
		}
		if (LIST_NEXT(d2, table_next) == NULL)
			break;
	}
	/* Insert after last element. */
	LIST_INSERT_AFTER(d2, d, table_next);
}

/*
 * Insert a delta into the correct place in branch. A trunk branch will have
 * different ordering scheme and be sorted by revision number, but a normal
 * branch will be sorted by date to maintain compability with branches that is
 * "hand-hacked".
 */
static void
rcsfile_insertdelta(struct branch *b, struct delta *d, int trunk)
{
	struct delta *d2;

	/* If it's empty, insert into head. */
	if (LIST_EMPTY(&b->deltalist)) {
		LIST_INSERT_HEAD(&b->deltalist, d, delta_next);
		return;
	}

	/*
	 * Just put it in before the revnum that is lower. Sort trunk branch by
	 * branchnum but the subbranches after deltadate.
	 */
	LIST_FOREACH(d2, &b->deltalist, delta_next) {
		if (trunk) {
			/*fprintf(stderr, "Comparing %s and %s\n", d->revnum,
			 * d2->revnum);*/
			if (rcsnum_cmp(d->revnum, d2->revnum) >= 0) {
				LIST_INSERT_BEFORE(d2, d, delta_next);
				return;
			}
		} else {
			/* XXX: here we depend on the date being set, but it
			 * should be before this is called anyway. */
			if (rcsnum_cmp(d->revdate, d2->revdate) <= 0) {
				LIST_INSERT_BEFORE(d2, d, delta_next);
				return;
			}
		}
		if (LIST_NEXT(d2, delta_next) == NULL)
			break;
	}
	/* Insert after last element. */
	LIST_INSERT_AFTER(d2, d, delta_next);
}


/* Add logtext to a delta. Assume the delta already exists. */
int
rcsdelta_addlog(struct delta *d, char *log)
{
	struct stream *dest;

	assert(d != NULL);
	log++;
	log[strlen(log) - 1] = '\0';

	dest = stream_open_buf(d->log);
	stream_write(dest, log, strlen(log));
	stream_close(dest);
	return (0);
}

/* Add deltatext to a delta. Assume the delta already exists. */
int
rcsdelta_addtext(struct delta *d, char *text)
{
	struct stream *dest;

	assert(d != NULL);
	text++;
	text[strlen(text) - 1] = '\0';

	dest = stream_open_buf(d->text);
	stream_write(dest, text, strlen(text));
	stream_close(dest);
	return (0);
}

/* Add a deltatext logline to a delta. */
void
rcsdelta_appendlog(struct delta *d, char *logline, size_t size)
{
	struct stream *dest;
	char buf[3];
	size_t i;
	int count;

	assert(d != NULL);
	dest = stream_open_buf(d->log);
	for (i = 0; i < size; i++) {
		buf[0] = logline[i];
		buf[1] = '\0';
		count = 1;
		/* Expand @'s */
		if (buf[0] == '@') {
			buf[1] = '@';
			buf[2] = '\0';
			count = 2;
		}
		stream_write(dest, buf, count);
	}
	stream_close(dest);
}

/* Add a deltatext textline to a delta. */
void
rcsdelta_appendtext(struct delta *d, char *textline, size_t size)
{
	struct stream *dest;
	char buf[3];
	size_t i;
	int count;

	assert(d != NULL);
	dest = stream_open_buf(d->text);
	/* XXX: code reuse. */
	for (i = 0; i < size; i++) {
		buf[0] = textline[i];
		buf[1] = '\0';
		count = 1;
		/* Expand @'s */
		if (buf[0] == '@') {
			buf[1] = '@';
			buf[2] = '\0';
			count = 2;
		}
		stream_write(dest, buf, count);
	}

	stream_close(dest);
}


/* Set delta state. */
void
rcsdelta_setstate(struct delta *d, char *state)
{

	if (d->state != NULL)
		free(state);
	if (state != NULL) {
		d->state = xstrdup(state);
		return;
	}
	d->state = NULL;
}

/* Truncate the deltalog with a certain offset. */
/* XXX: error values for these. */
void
rcsdelta_truncatelog(struct delta *d, off_t offset)
{

	stream_truncate_buf(d->log, offset);
}

/* Truncate the deltatext with a certain offset. */
void
rcsdelta_truncatetext(struct delta *d, off_t offset)
{

	stream_truncate_buf(d->text, offset);
}
