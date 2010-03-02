/*-
 * Copyright (c) 2006, Maxime Henrion <mux@FreeBSD.org>
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

#include <sys/types.h>

#include <assert.h>
#include <regex.h>
#include <stdlib.h>

#include "fnmatch.h"
#include "globtree.h"
#include "misc.h"

/*
 * The "GlobTree" interface allows one to construct arbitrarily complex
 * boolean expressions for evaluating whether to accept or reject a
 * filename.  The globtree_test() function returns true or false
 * according to whether the name is accepted or rejected by the
 * expression.
 *
 * Expressions are trees constructed from nodes representing either
 * primitive matching operations (primaries) or operators that are
 * applied to their subexpressions.  The simplest primitives are
 * globtree_false(), which matches nothing, and globtree_true(), which
 * matches everything.
 *
 * A more useful primitive is the matching operation, constructed with
 * globtree_match().  It will call fnmatch() with the suppliedi
 * shell-style pattern to determine if the filename matches.
 *
 * Expressions can be combined with the boolean operators AND, OR, and
 * NOT, to form more complex expressions.
 */

/* Node types. */
#define	GLOBTREE_NOT		0
#define	GLOBTREE_AND		1
#define	GLOBTREE_OR		2
#define	GLOBTREE_MATCH		3
#define	GLOBTREE_REGEX		4
#define	GLOBTREE_TRUE		5
#define	GLOBTREE_FALSE		6

/* A node. */
struct globtree {
	int type;
	struct globtree *left;
	struct globtree *right;

	/* The "data" field points to the text pattern for GLOBTREE_MATCH
	   nodes, and to the regex_t for GLOBTREE_REGEX nodes. For any
	   other node, it is set to NULL. */
	void *data;
	/* The "flags" field contains the flags to pass to fnmatch() for
	   GLOBTREE_MATCH nodes. */
	int flags;
};

static struct globtree	*globtree_new(int);
static int		 globtree_eval(struct globtree *, const char *);

static struct globtree *
globtree_new(int type)
{
	struct globtree *gt;

	gt = xmalloc(sizeof(struct globtree));
	gt->type = type;
	gt->data = NULL;
	gt->flags = 0;
	gt->left = NULL;
	gt->right = NULL;
	return (gt);
}

struct globtree *
globtree_true(void)
{
	struct globtree *gt;

	gt = globtree_new(GLOBTREE_TRUE);
	return (gt);
}

struct globtree *
globtree_false(void)
{
	struct globtree *gt;

	gt = globtree_new(GLOBTREE_FALSE);
	return (gt);
}

struct globtree *
globtree_match(const char *pattern, int flags)
{
	struct globtree *gt;

	gt = globtree_new(GLOBTREE_MATCH);
	gt->data = xstrdup(pattern);
	gt->flags = flags;
	return (gt);
}

struct globtree *
globtree_regex(const char *pattern)
{
	struct globtree *gt;
	int error;

	gt = globtree_new(GLOBTREE_REGEX);
	gt->data = xmalloc(sizeof(regex_t));
	error = regcomp(gt->data, pattern, REG_NOSUB);
	assert(!error);
	return (gt);
}

struct globtree *
globtree_and(struct globtree *left, struct globtree *right)
{
	struct globtree *gt;

	if (left->type == GLOBTREE_FALSE || right->type == GLOBTREE_FALSE) {
		globtree_free(left);
		globtree_free(right);
		gt = globtree_false();
		return (gt);
	}
	if (left->type == GLOBTREE_TRUE) {
		globtree_free(left);
		return (right);
	}
	if (right->type == GLOBTREE_TRUE) {
		globtree_free(right);
		return (left);
	}
	gt = globtree_new(GLOBTREE_AND);
	gt->left = left;
	gt->right = right;
	return (gt);
}

struct globtree *
globtree_or(struct globtree *left, struct globtree *right)
{
	struct globtree *gt;

	if (left->type == GLOBTREE_TRUE || right->type == GLOBTREE_TRUE) {
		globtree_free(left);
		globtree_free(right);
		gt = globtree_true();
		return (gt);
	}
	if (left->type == GLOBTREE_FALSE) {
		globtree_free(left);
		return (right);
	}
	if (right->type == GLOBTREE_FALSE) {
		globtree_free(right);
		return (left);
	}
	gt = globtree_new(GLOBTREE_OR);
	gt->left = left;
	gt->right = right;
	return (gt);
}

struct globtree *
globtree_not(struct globtree *child)
{
	struct globtree *gt;

	if (child->type == GLOBTREE_TRUE) {
		globtree_free(child);
		gt = globtree_new(GLOBTREE_FALSE);
		return (gt);
	}
	if (child->type == GLOBTREE_FALSE) {
		globtree_free(child);
		gt = globtree_new(GLOBTREE_TRUE);
		return (gt);
	}
	gt = globtree_new(GLOBTREE_NOT);
	gt->left = child;
	return (gt);
}

/* Evaluate one node (must be a leaf node). */
static int
globtree_eval(struct globtree *gt, const char *path)
{
	int rv;

	switch (gt->type) {
	case GLOBTREE_TRUE:
		return (1);
	case GLOBTREE_FALSE:
		return (0);
	case GLOBTREE_MATCH:
		assert(gt->data != NULL);
		rv = fnmatch(gt->data, path, gt->flags);
		if (rv == 0)
			return (1);
		assert(rv == FNM_NOMATCH);
		return (0);
	case GLOBTREE_REGEX:
		assert(gt->data != NULL);
		rv = regexec(gt->data, path, 0, NULL, 0);
		if (rv == 0)
			return (1);
		assert(rv == REG_NOMATCH);
		return (0);
	}

	assert(0);
	return (-1);
}

/* Small stack API to walk the tree iteratively. */
typedef enum {
	STATE_DOINGLEFT,
	STATE_DOINGRIGHT
} walkstate_t;

struct stack {
	struct stackelem *stack;
	size_t size;
	size_t in;
};

struct stackelem {
	struct globtree *node;
	walkstate_t state;
};

static void
stack_init(struct stack *stack)
{

	stack->in = 0;
	stack->size = 8;	/* Initial size. */
	stack->stack = xmalloc(sizeof(struct stackelem) * stack->size);
}

static size_t
stack_size(struct stack *stack)
{

	return (stack->in);
}

static void
stack_push(struct stack *stack, struct globtree *node, walkstate_t state)
{
	struct stackelem *e;

	if (stack->in == stack->size) {
		stack->size *= 2;
		stack->stack = xrealloc(stack->stack,
		    sizeof(struct stackelem) * stack->size);
	}
	e = stack->stack + stack->in++;
	e->node = node;
	e->state = state;
}

static void
stack_pop(struct stack *stack, struct globtree **node, walkstate_t *state)
{
	struct stackelem *e;

	assert(stack->in > 0);
	e = stack->stack + --stack->in;
	*node = e->node;
	*state = e->state;
}

static void
stack_free(struct stack *s)
{

	free(s->stack);
}

/* Tests if the supplied filename matches. */
int
globtree_test(struct globtree *gt, const char *path)
{
	struct stack stack;
	walkstate_t state;
	int val;

	stack_init(&stack);
	for (;;) {
doleft:
		/* Descend to the left until we hit bottom. */
		while (gt->left != NULL) {
			stack_push(&stack, gt, STATE_DOINGLEFT);
			gt = gt->left;
		}

		/* Now we're at a leaf node.  Evaluate it. */
		val = globtree_eval(gt, path);
		/* Ascend, propagating the value through operator nodes. */
		for (;;) {
			if (stack_size(&stack) == 0) {
				stack_free(&stack);
				return (val);
			}
			stack_pop(&stack, &gt, &state);
			switch (gt->type) {
			case GLOBTREE_NOT:
				val = !val;
				break;
			case GLOBTREE_AND:
				/* If we haven't yet evaluated the right subtree
				   and the partial result is true, descend to
				   the right.  Otherwise the result is already
				   determined to be val. */
				if (state == STATE_DOINGLEFT && val) {
					stack_push(&stack, gt,
					    STATE_DOINGRIGHT);
					gt = gt->right;
					goto doleft;
				}
				break;
			case GLOBTREE_OR:
				/* If we haven't yet evaluated the right subtree
				   and the partial result is false, descend to
				   the right.  Otherwise the result is already
				   determined to be val. */
				if (state == STATE_DOINGLEFT && !val) {
					stack_push(&stack, gt,
					    STATE_DOINGRIGHT);
					gt = gt->right;
					goto doleft;
				}
				break;
			default:
				/* We only push nodes that have children. */
				assert(0);
				return (-1);
			}
		}
	}
}

/*
 * We could de-recursify this function using a stack, but it would be
 * overkill since it is never called from a thread context with a
 * limited stack size nor used in a critical path, so I think we can
 * afford keeping it recursive.
 */
void
globtree_free(struct globtree *gt)
{

	if (gt->data != NULL) {
		if (gt->type == GLOBTREE_REGEX)
			regfree(gt->data);
		free(gt->data);
	}
	if (gt->left != NULL)
		globtree_free(gt->left);
	if (gt->right != NULL)
		globtree_free(gt->right);
	free(gt);
}
