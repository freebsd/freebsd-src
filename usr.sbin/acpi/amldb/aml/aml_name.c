/*-
 * Copyright (c) 1999 Takanori Watanabe
 * Copyright (c) 1999, 2000 Yasuo Yokoyama
 * Copyright (c) 1999, 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *	$Id: aml_name.c,v 1.15 2000/08/16 18:14:53 iwasaki Exp $
 *	$FreeBSD$
 */

#include <sys/param.h>

#include <aml/aml_amlmem.h>
#include <aml/aml_common.h>
#include <aml/aml_env.h>
#include <aml/aml_name.h>

#ifndef _KERNEL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#else /* _KERNEL */
#include <sys/systm.h>
#endif /* !_KERNEL */

static struct aml_name	*aml_find_name(struct aml_name *, char *);
static struct aml_name	*aml_new_name(struct aml_name *, char *);
static void		 aml_delete_name(struct aml_name *);

static struct	aml_name rootname = {"\\", NULL, NULL, NULL, NULL, NULL};

static struct	aml_name_group root_group = {
	AML_NAME_GROUP_ROOT,
	&rootname,
	NULL
};

struct	aml_name_group *name_group_list = &root_group;
struct	aml_local_stack *stack_top = NULL;

struct aml_name *
aml_get_rootname()
{

	return (&rootname);
}

static struct aml_name *
aml_find_name(struct aml_name *parent, char *name)
{
	struct	aml_name *result;

	if (!parent)
		parent = &rootname;
	for (result = parent->child; result; result = result->brother)
		if (!strncmp(result->name, name, 4))
			break;
	return (result);
}

/*
 * Parse given namesppace expression and find a first matched object
 * under given level of the tree by depth first search.
 */

struct aml_name *
aml_find_from_namespace(struct aml_name *parent, char *name)
{
	char	*ptr;
	int	len;
	struct	aml_name *result;

	ptr = name;
	if (!parent)
		parent = &rootname;

	if (ptr[0] == '\\') {
		ptr++;
		parent = &rootname;
	}
	for (len = 0; ptr[len] != '.' && ptr[len] != '\0'; len++)
		;

	for (result = parent->child; result; result = result->brother) {
		if (!strncmp(result->name, ptr, len)) {
			if (ptr[len] == '\0' || ptr[len + 1] == '\0') {
				return (result);
			}
			ptr += len;
			if (ptr[0] != '.') {
				return (NULL);
			}
			ptr++;
			return (aml_find_from_namespace(result, ptr));
		}
	}

	return (NULL);
}

static void
_aml_apply_foreach_found_objects(struct aml_name *parent, char *name,
    int len, int shallow, int (*func)(struct aml_name *, va_list), va_list ap)
{
	struct	aml_name *child, *ptr;

	child = ptr = NULL;

	/* function to apply must be specified */
	if (func == NULL) {
		return;
	}

	for (child = parent->child; child; child = child->brother) {
		if (!strncmp(child->name, name, len)) {
			/* if function call was failed, stop searching */
			if (func(child, ap) != 0) {
				return;
			}
		}
	}

	if (shallow == 1) {
		return;
	}

	for (ptr = parent->child; ptr; ptr = ptr->brother) {
		/* do more searching */
		_aml_apply_foreach_found_objects(ptr, name, len, 0, func, ap);
	}
}

/*
 * Find named objects as many as possible under given level of
 * namespace, and apply given callback function for each
 * named objects found.  If the callback function returns non-zero
 * value, then the search terminates immediately.
 * Note that object name expression is used as forward substring match,
 * not exact match.  The name expression "_L" will match for objects
 * which have name starting with "_L" such as "\_SB_.LID_._LID" and
 * "\_GPE._L00" and so on. The name expression can include parent object
 * name in it like "\_GPE._L".  In this case, GPE X level wake handlers
 * will be found under "\_GPE" in shallow level.
 */

void
aml_apply_foreach_found_objects(struct aml_name *start, char *name,
    int (*func)(struct aml_name *, va_list), ...)
{
	int	i, len, has_dot, last_is_dot, shallow;
	struct	aml_name *child, *parent;
	va_list	ap;

	shallow = 0;
	if (start == NULL) {
		parent = &rootname;
	} else {
		parent = start;
	}
	if (name[0] == '\\') {
		name++;
		parent = &rootname;
		shallow = 1;
	}

	len = strlen(name);
	last_is_dot = 0;
	/* the last dot should be ignored */
	if (len > 0 && name[len - 1] == '.') {
		len--;
		last_is_dot = 1;
	}

	has_dot = 0;
	for (i = 0; i < len - 1; i++) {
		if (name[i] == '.') {
			has_dot = 1;
			break;
		}
	}

	/* try to parse expression and find any matched object. */
	if (has_dot == 1) {
		child = aml_find_from_namespace(parent, name);
		if (child == NULL) {
			return;
		}

		/*
		 * we have at least one object matched, search all objects
		 * under upper level of the found object.
		 */
		parent = child->parent;

		/* find the last `.' */
		for (name = name + len - 1; *name != '.'; name--)
			;
		name++;
		len = strlen(name) - last_is_dot;
		shallow = 1;
	}

	if (len > 4) {
		return;
	}

	va_start(ap, func);
	_aml_apply_foreach_found_objects(parent, name, len, shallow, func, ap);
	va_end(ap);
}

struct aml_name_group *
aml_new_name_group(int id)
{
	struct	aml_name_group *result;

	result = memman_alloc(aml_memman, memid_aml_name_group);
	result->id = id;
	result->head = NULL;
	result->next = name_group_list;
	name_group_list = result;
	return (result);
}

void
aml_delete_name_group(struct aml_name_group *target)
{
	struct	aml_name_group *previous;

	previous = name_group_list;
	if (previous == target)
		name_group_list = target->next;
	else {
		while (previous && previous->next != target)
			previous = previous->next;
		if (previous)
			previous->next = target->next;
	}
	target->next = NULL;
	if (target->head)
		aml_delete_name(target->head);
	memman_free(aml_memman, memid_aml_name_group, target);
}

static struct aml_name *
aml_new_name(struct aml_name *parent, char *name)
{
	struct	aml_name *newname;

	if ((newname = aml_find_name(parent, name)) != NULL)
		return (newname);

	newname = memman_alloc(aml_memman, memid_aml_name);
	strncpy(newname->name, name, 4);
	newname->parent = parent;
	newname->child = NULL;
	newname->property = NULL;
	if (parent->child)
		newname->brother = parent->child;
	else
		newname->brother = NULL;
	parent->child = newname;

	newname->chain = name_group_list->head;
	name_group_list->head = newname;

	return (newname);
}

/*
 * NOTE:
 * aml_delete_name() doesn't maintain aml_name_group::{head,tail}.
 */
static void
aml_delete_name(struct aml_name *target)
{
	struct	aml_name *next;
	struct	aml_name *ptr;

	for (; target; target = next) {
		next = target->chain;
		if (target->child) {
			target->chain = NULL;
			continue;
		}
		if (target->brother) {
			if (target->parent) {
				if (target->parent->child == target) {
					target->parent->child = target->brother;
				} else {
					ptr = target->parent->child;
					while (ptr && ptr->brother != target)
						ptr = ptr->brother;
					if (ptr)
						ptr->brother = target->brother;
				}
				target->brother = NULL;
			}
		} else if (target->parent) {
			target->parent->child = NULL;
		}
		aml_free_object(&target->property);
		memman_free(aml_memman, memid_aml_name, target);
	}
}

#define AML_SEARCH_NAME 0
#define AML_CREATE_NAME 1
static struct aml_name	*aml_nameman(struct aml_environ *, u_int8_t *, int);

struct aml_name *
aml_search_name(struct aml_environ *env, u_int8_t *dp)
{

	return (aml_nameman(env, dp, AML_SEARCH_NAME));
}

struct aml_name *
aml_create_name(struct aml_environ *env, u_int8_t *dp)
{

	return (aml_nameman(env, dp, AML_CREATE_NAME));
}

static struct aml_name *
aml_nameman(struct aml_environ *env, u_int8_t *dp, int flag)
{
	int	segcount;
	int	i;
	struct	aml_name *newname, *curname;
	struct	aml_name *(*searchfunc) (struct aml_name *, char *);

#define CREATECHECK() do {						\
	if (newname == NULL) {						\
		AML_DEBUGPRINT("ERROR CANNOT FIND NAME\n");		\
		env->stat = aml_stat_panic;				\
		return (NULL);						\
	}								\
} while(0)

	searchfunc = (flag == AML_CREATE_NAME) ? aml_new_name : aml_find_name;
	newname = env->curname;
	if (dp[0] == '\\') {
		newname = &rootname;
		dp++;
	} else if (dp[0] == '^') {
		while (dp[0] == '^') {
			newname = newname->parent;
			CREATECHECK();
			dp++;
		}
	}
	if (dp[0] == 0x00) {	/* NullName */
		dp++;
	} else if (dp[0] == 0x2e) {	/* DualNamePrefix */
		newname = (*searchfunc) (newname, dp + 1);
		CREATECHECK();
		newname = (*searchfunc) (newname, dp + 5);
		CREATECHECK();
	} else if (dp[0] == 0x2f) {	/* MultiNamePrefix */
		segcount = dp[1];
		for (i = 0, dp += 2; i < segcount; i++, dp += 4) {
			newname = (*searchfunc) (newname, dp);
			CREATECHECK();
		}
	} else if (flag == AML_CREATE_NAME) {	/* NameSeg */
		newname = aml_new_name(newname, dp);
		CREATECHECK();
	} else {
		curname = newname;
		for (;;) {
			newname = aml_find_name(curname, dp);
			if (newname != NULL)
				break;
			if (curname == &rootname)
				break;
			curname = curname->parent;
		}
	}
	return (newname);
}

#undef CREATECHECK

struct aml_local_stack *
aml_local_stack_create()
{
	struct aml_local_stack *result;

	result = memman_alloc(aml_memman, memid_aml_local_stack);
	memset(result, 0, sizeof(struct aml_local_stack));
	return (result);
}

void 
aml_local_stack_push(struct aml_local_stack *stack)
{

	stack->next = stack_top;
	stack_top = stack;
}

struct aml_local_stack *
aml_local_stack_pop()
{
	struct aml_local_stack *result;

	result = stack_top;
	stack_top = result->next;
	result->next = NULL;
	return (result);
}

void
aml_local_stack_delete(struct aml_local_stack *stack)
{
	int	i;

	for (i = 0; i < 8; i++)
		aml_free_object(&stack->localvalue[i].property);
	for (i = 0; i < 7; i++)
		aml_free_object(&stack->argumentvalue[i].property);
	aml_delete_name(stack->temporary);
	memman_free(aml_memman, memid_aml_local_stack, stack);
}

struct aml_name *
aml_local_stack_getLocalX(int index)
{

	if (stack_top == NULL)
		return (NULL);
	return (&stack_top->localvalue[index]);
}

struct aml_name *
aml_local_stack_getArgX(struct aml_local_stack *stack, int index)
{

	if (!stack)
		stack = stack_top;
	if (stack == NULL)
		return (NULL);
	return (&stack->argumentvalue[index]);
}

struct aml_name *
aml_create_local_object()
{
	struct aml_name *result;

	result = memman_alloc(aml_memman, memid_aml_name);
	result->child = result->brother = result->parent = NULL;
	result->property = NULL;
	result->chain = stack_top->temporary;
	stack_top->temporary = result;
	return (result);
}
