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
 *	$Id: aml_name.h,v 1.17 2000/08/16 18:14:54 iwasaki Exp $
 *	$FreeBSD$
 */

#ifndef _AML_NAME_H_
#define _AML_NAME_H_

#include <machine/stdarg.h>

#include <dev/acpi/aml/aml_obj.h>

struct aml_name {
	char	name[4];
	union	aml_object *property;
	struct	aml_name *parent;
	struct	aml_name *brother;
	struct	aml_name *child;
	struct	aml_name *chain;
};

#define AML_NAME_GROUP_ROOT		0
#define AML_NAME_GROUP_OS_DEFINED	1
#define AML_NAME_GROUP_IN_METHOD	2

struct	aml_name_group {
	int	id;			/* DSDT address or DBHANDLE */
	struct	aml_name *head;
	struct	aml_name_group *next;
};

struct	aml_local_stack {
	struct	aml_name localvalue[8];
	struct	aml_name argumentvalue[7];
	struct	aml_name *temporary;
	struct	aml_local_stack *next;
};

/* forward declarement */
struct aml_envrion;

struct aml_name		*aml_get_rootname(void);
struct aml_name_group	*aml_new_name_group(int);
void			 aml_delete_name_group(struct aml_name_group *);

struct aml_name		*aml_find_from_namespace(struct aml_name *, char *);
void			 aml_apply_foreach_found_objects(struct aml_name *,
			     char *, int (*)(struct aml_name *, va_list), ...);
struct aml_name		*aml_search_name(struct aml_environ *, u_int8_t *);
struct aml_name		*aml_create_name(struct aml_environ *, u_int8_t *);

struct aml_local_stack	*aml_local_stack_create(void);
void			 aml_local_stack_push(struct aml_local_stack *);
struct aml_local_stack	*aml_local_stack_pop(void);
void			 aml_local_stack_delete(struct aml_local_stack *);
struct aml_name		*aml_local_stack_getLocalX(int);
struct aml_name		*aml_local_stack_getArgX(struct aml_local_stack *, int);
struct aml_name		*aml_create_local_object(void);

extern struct	aml_name_group *name_group_list;

#endif /* !_AML_NAME_H_ */
