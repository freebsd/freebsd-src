/* $Id: tag.c,v 1.36 2020/04/19 16:36:16 schwarze Exp $ */
/*
 * Copyright (c) 2015,2016,2018,2019,2020 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Functions to tag syntax tree nodes.
 * For internal use by mandoc(1) validation modules only.
 */
#include "config.h"

#include <sys/types.h>

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc_aux.h"
#include "mandoc_ohash.h"
#include "roff.h"
#include "mdoc.h"
#include "roff_int.h"
#include "tag.h"

struct tag_entry {
	struct roff_node **nodes;
	size_t	 maxnodes;
	size_t	 nnodes;
	int	 prio;
	char	 s[];
};

static void		 tag_move_href(struct roff_man *,
				struct roff_node *, const char *);
static void		 tag_move_id(struct roff_node *);

static struct ohash	 tag_data;


/*
 * Set up the ohash table to collect nodes
 * where various marked-up terms are documented.
 */
void
tag_alloc(void)
{
	mandoc_ohash_init(&tag_data, 4, offsetof(struct tag_entry, s));
}

void
tag_free(void)
{
	struct tag_entry	*entry;
	unsigned int		 slot;

	if (tag_data.info.free == NULL)
		return;
	entry = ohash_first(&tag_data, &slot);
	while (entry != NULL) {
		free(entry->nodes);
		free(entry);
		entry = ohash_next(&tag_data, &slot);
	}
	ohash_delete(&tag_data);
	tag_data.info.free = NULL;
}

/*
 * Set a node where a term is defined,
 * unless it is already defined at a lower priority.
 */
void
tag_put(const char *s, int prio, struct roff_node *n)
{
	struct tag_entry	*entry;
	struct roff_node	*nold;
	const char		*se;
	size_t			 len;
	unsigned int		 slot;

	assert(prio <= TAG_FALLBACK);

	if (s == NULL) {
		if (n->child == NULL || n->child->type != ROFFT_TEXT)
			return;
		s = n->child->string;
		switch (s[0]) {
		case '-':
			s++;
			break;
		case '\\':
			switch (s[1]) {
			case '&':
			case '-':
			case 'e':
				s += 2;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}

	/*
	 * Skip whitespace and escapes and whatever follows,
	 * and if there is any, downgrade the priority.
	 */

	len = strcspn(s, " \t\\");
	if (len == 0)
		return;

	se = s + len;
	if (*se != '\0' && prio < TAG_WEAK)
		prio = TAG_WEAK;

	slot = ohash_qlookupi(&tag_data, s, &se);
	entry = ohash_find(&tag_data, slot);

	/* Build a new entry. */

	if (entry == NULL) {
		entry = mandoc_malloc(sizeof(*entry) + len + 1);
		memcpy(entry->s, s, len);
		entry->s[len] = '\0';
		entry->nodes = NULL;
		entry->maxnodes = entry->nnodes = 0;
		ohash_insert(&tag_data, slot, entry);
	}

	/*
	 * Lower priority numbers take precedence.
	 * If a better entry is already present, ignore the new one.
	 */

	else if (entry->prio < prio)
			return;

	/*
	 * If the existing entry is worse, clear it.
	 * In addition, a tag with priority TAG_FALLBACK
	 * is only used if the tag occurs exactly once.
	 */

	else if (entry->prio > prio || prio == TAG_FALLBACK) {
		while (entry->nnodes > 0) {
			nold = entry->nodes[--entry->nnodes];
			nold->flags &= ~NODE_ID;
			free(nold->tag);
			nold->tag = NULL;
		}
		if (prio == TAG_FALLBACK) {
			entry->prio = TAG_DELETE;
			return;
		}
	}

	/* Remember the new node. */

	if (entry->maxnodes == entry->nnodes) {
		entry->maxnodes += 4;
		entry->nodes = mandoc_reallocarray(entry->nodes,
		    entry->maxnodes, sizeof(*entry->nodes));
	}
	entry->nodes[entry->nnodes++] = n;
	entry->prio = prio;
	n->flags |= NODE_ID;
	if (n->child == NULL || n->child->string != s || *se != '\0') {
		assert(n->tag == NULL);
		n->tag = mandoc_strndup(s, len);
	}
}

int
tag_exists(const char *tag)
{
	return ohash_find(&tag_data, ohash_qlookup(&tag_data, tag)) != NULL;
}

/*
 * For in-line elements, move the link target
 * to the enclosing paragraph when appropriate.
 */
static void
tag_move_id(struct roff_node *n)
{
	struct roff_node *np;

	np = n;
	for (;;) {
		if (np->prev != NULL)
			np = np->prev;
		else if ((np = np->parent) == NULL)
			return;
		switch (np->tok) {
		case MDOC_It:
			switch (np->parent->parent->norm->Bl.type) {
			case LIST_column:
				/* Target the ROFFT_BLOCK = <tr>. */
				np = np->parent;
				break;
			case LIST_diag:
			case LIST_hang:
			case LIST_inset:
			case LIST_ohang:
			case LIST_tag:
				/* Target the ROFFT_HEAD = <dt>. */
				np = np->parent->head;
				break;
			default:
				/* Target the ROFF_BODY = <li>. */
				break;
			}
			/* FALLTHROUGH */
		case MDOC_Pp:	/* Target the ROFFT_ELEM = <p>. */
			if (np->tag == NULL) {
				np->tag = mandoc_strdup(n->tag == NULL ?
				    n->child->string : n->tag);
				np->flags |= NODE_ID;
				n->flags &= ~NODE_ID;
			}
			return;
		case MDOC_Sh:
		case MDOC_Ss:
		case MDOC_Bd:
		case MDOC_Bl:
		case MDOC_D1:
		case MDOC_Dl:
		case MDOC_Rs:
			/* Do not move past major blocks. */
			return;
		default:
			/*
			 * Move past in-line content and partial
			 * blocks, for example .It Xo or .It Bq Er.
			 */
			break;
		}
	}
}

/*
 * When a paragraph is tagged and starts with text,
 * move the permalink to the first few words.
 */
static void
tag_move_href(struct roff_man *man, struct roff_node *n, const char *tag)
{
	char	*cp;

	if (n == NULL || n->type != ROFFT_TEXT ||
	    *n->string == '\0' || *n->string == ' ')
		return;

	cp = n->string;
	while (cp != NULL && cp - n->string < 5)
		cp = strchr(cp + 1, ' ');

	/* If the first text node is longer, split it. */

	if (cp != NULL && cp[1] != '\0') {
		man->last = n;
		man->next = ROFF_NEXT_SIBLING;
		roff_word_alloc(man, n->line,
		    n->pos + (cp - n->string), cp + 1);
		man->last->flags = n->flags & ~NODE_LINE;
		*cp = '\0';
	}

	assert(n->tag == NULL);
	n->tag = mandoc_strdup(tag);
	n->flags |= NODE_HREF;
}

/*
 * When all tags have been set, decide where to put
 * the associated permalinks, and maybe move some tags
 * to the beginning of the respective paragraphs.
 */
void
tag_postprocess(struct roff_man *man, struct roff_node *n)
{
	if (n->flags & NODE_ID) {
		switch (n->tok) {
		case MDOC_Pp:
			tag_move_href(man, n->next, n->tag);
			break;
		case MDOC_Bd:
		case MDOC_D1:
		case MDOC_Dl:
			tag_move_href(man, n->child, n->tag);
			break;
		case MDOC_Bl:
			/* XXX No permalink for now. */
			break;
		default:
			if (n->type == ROFFT_ELEM || n->tok == MDOC_Fo)
				tag_move_id(n);
			if (n->tok != MDOC_Tg)
				n->flags |= NODE_HREF;
			else if ((n->flags & NODE_ID) == 0) {
				n->flags |= NODE_NOPRT;
				free(n->tag);
				n->tag = NULL;
			}
			break;
		}
	}
	for (n = n->child; n != NULL; n = n->next)
		tag_postprocess(man, n);
}
