/*-
 * Copyright (c) 1999 Takanori Watanabe
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
 *	$Id: aml_evalobj.c,v 1.27 2000/08/16 18:14:53 iwasaki Exp $
 *	$FreeBSD$
 */

#include <sys/param.h>

#include <dev/acpi/aml/aml_amlmem.h>
#include <dev/acpi/aml/aml_common.h>
#include <dev/acpi/aml/aml_env.h>
#include <dev/acpi/aml/aml_evalobj.h>
#include <dev/acpi/aml/aml_name.h>
#include <dev/acpi/aml/aml_obj.h>
#include <dev/acpi/aml/aml_parse.h>
#include <dev/acpi/aml/aml_region.h>
#include <dev/acpi/aml/aml_status.h>
#include <dev/acpi/aml/aml_store.h>

#ifndef _KERNEL
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#else /* _KERNEL */
#include <sys/systm.h>
#endif /* !_KERNEL */

static union aml_object	*aml_eval_fieldobject(struct aml_environ *env,
					      struct aml_name *name);

static union aml_object *
aml_eval_fieldobject(struct aml_environ *env, struct aml_name *name)
{
	int	num;
	struct	aml_name *oname,*wname;
	struct	aml_field *field;
	struct	aml_opregion *or;
	union	aml_object tobj;

	num = 0;
	/* CANNOT OCCUR! */
	if (name == NULL || name->property == NULL ||
	    name->property->type != aml_t_field) {
		printf("????\n");
		env->stat = aml_stat_panic;
		return (NULL);
	}
	field = &name->property->field;
	oname = env->curname;
	if (field->bitlen > 32) {
		env->tempobject.type = aml_t_regfield;
	} else {
		env->tempobject.type = aml_t_num;
	}
	env->curname = name;
	if (field->f.ftype == f_t_field) {
		wname = aml_search_name(env, field->f.fld.regname);
		if (wname == NULL || wname->property == NULL ||
		    wname->property->type != aml_t_opregion) {
			AML_DEBUGPRINT("Inappropreate Type\n");
			env->stat = aml_stat_panic;
			env->curname = oname;
			return (NULL);
		}
		or = &wname->property->opregion;
		if (env->tempobject.type == aml_t_regfield) {
			env->tempobject.regfield.space = or->space;
			env->tempobject.regfield.flags = field->flags;
			env->tempobject.regfield.offset = or->offset;
			env->tempobject.regfield.bitoffset = field->bitoffset;
			env->tempobject.regfield.bitlen = field->bitlen;
		} else {
			env->tempobject.type = aml_t_num;
			env->tempobject.num.number = aml_region_read(env,
			    or->space, field->flags, or->offset,
			    field->bitoffset, field->bitlen);
			AML_DEBUGPRINT("[read(%d, 0x%x)->0x%x]",
			    or->space, or->offset + field->bitoffset / 8,
			    env->tempobject.num.number);
		}
	} else if (field->f.ftype == f_t_index) {
		wname = aml_search_name(env, field->f.ifld.indexname);
		tobj.type = aml_t_num;
		tobj.num.number = field->bitoffset / 8;/* AccessType Boundary */
		aml_store_to_name(env, &tobj, wname);
		wname = aml_search_name(env, field->f.ifld.dataname);
		num = aml_objtonum(env, aml_eval_name(env, wname));
		env->tempobject.type = aml_t_num;
		env->tempobject.num.number = (num >> (field->bitoffset & 7)) &
		    ((1 << field->bitlen) - 1);
	}
	env->curname = oname;
	return (&env->tempobject);
}

union aml_object *
aml_eval_objref(struct aml_environ *env, union aml_object *obj)
{
	int	offset;
	union	aml_object num1;
	union	aml_object *ref, *ret;

	ret = obj;
	if (obj->objref.deref == 1) {
		num1.type = aml_t_num;
		offset = obj->objref.offset;
		ref = obj->objref.ref;
		if (ref == NULL) {
			goto out;
		}
		switch (ref->type) {
		case aml_t_package:
			if (ref->package.elements > offset) {
				ret = ref->package.objects[offset];
			} else {
				num1.num.number = 0;
				env->tempobject = num1;
				ret = &env->tempobject;
			}
			break;
		case aml_t_buffer:
			if (ref->buffer.size > offset) {
				num1.num.number = ref->buffer.data[offset] & 0xff;
			} else {
				num1.num.number = 0;
			}
			env->tempobject = num1;
			ret = &env->tempobject;
			break;
		default:
			break;
		}
	}
	if (obj->objref.alias == 1) {
		ret = aml_eval_name(env, obj->objref.nameref);
		goto out;
	}
out:
	return (ret);
}

/*
 * Eval named object.
 */
union aml_object *
aml_eval_name(struct aml_environ *env, struct aml_name *aname)
{
	int	argnum, i;
	int	num;
	struct	aml_name *tmp;
	struct	aml_environ *copy;
	struct	aml_local_stack *stack;
	union	aml_object *obj, *ret;
	union	aml_object *src;

	ret = NULL;
	if (aname == NULL || aname->property == NULL) {
		return (NULL);
	}
	if (env->stat == aml_stat_panic) {
		return (NULL);
	}
	copy = memman_alloc(aml_memman, memid_aml_environ);
	if (copy == NULL) {
		return (NULL);
	}
	ret = aname->property;
	i = 0;
reevaluate:
	if (i > 10) {
		env->stat = aml_stat_panic;
		printf("TOO MANY LOOP\n");
		ret = NULL;
		goto out;
	}
	switch (aname->property->type) {
	case aml_t_namestr:
		tmp = aname;
		aname = aml_search_name(env, aname->property->nstr.dp);
		if (aname == NULL) {
			aname = tmp;
		}
		i++;
		goto reevaluate;
	case aml_t_objref:
		ret = aml_eval_objref(env, aname->property);
		goto out;
	case aml_t_num:
	case aml_t_string:
	case aml_t_buffer:
	case aml_t_package:
	case aml_t_debug:
		ret = aname->property;
		goto out;
	case aml_t_field:
		aml_free_objectcontent(&env->tempobject);
		ret = aml_eval_fieldobject(env, aname);
		goto out;
	case aml_t_method:
		aml_free_objectcontent(&env->tempobject);
		argnum = aname->property->meth.argnum & 7;
		*copy = *env;
		copy->curname = aname;
		copy->dp = aname->property->meth.from;
		copy->end = aname->property->meth.to;
		copy->stat = aml_stat_none;
		stack = aml_local_stack_create();
		AML_DEBUGPRINT("(");
		for (i = 0; i < argnum; i++) {
			aml_local_stack_getArgX(stack, i)->property =
			    aml_copy_object(env,
				aml_eval_name(env,
				    aml_parse_termobj(env, 0)));
			if (i < argnum - 1)
				AML_DEBUGPRINT(", ");
		}
		AML_DEBUGPRINT(")\n");
		aml_local_stack_push(stack);
		if (env->stat == aml_stat_step) {
			AML_DEBUGGER(env, copy);
		}
		tmp = aml_execute_method(copy);
		obj = aml_eval_name(env, tmp);
		if (copy->stat == aml_stat_panic) {
			AML_DEBUGPRINT("PANIC OCCURED IN METHOD");
			env->stat = aml_stat_panic;
			ret = NULL;
			aml_local_stack_delete(aml_local_stack_pop());
			goto out;
		}
		if (aml_debug) {
			aml_showobject(obj);
		}

		if (tmp)
			tmp->property = NULL;
		aml_local_stack_delete(aml_local_stack_pop());
		if (obj) {
			aml_create_local_object()->property = obj;
			ret = obj;
		} else {
			env->tempobject.type = aml_t_num;
			env->tempobject.num.number = 0;
		}

		goto out;
	case aml_t_bufferfield:
		aml_free_objectcontent(&env->tempobject);
		if (aname->property->bfld.bitlen > 32) {
			ret = aname->property;
		} else {
			src = aname->property;
			num = aml_bufferfield_read(src->bfld.origin,
			    src->bfld.bitoffset, src->bfld.bitlen);
			env->tempobject.type = aml_t_num;
			env->tempobject.num.number = num;
			ret = &env->tempobject;
		}
		goto out;
	default:
		AML_DEBUGPRINT("I eval the object that I should not eval, %s%d",
		    aname->name, aname->property->type);
		AML_SYSABORT();
		ret = NULL;
		goto out;
	}
out:
	memman_free(aml_memman, memid_aml_environ, copy);
	return (ret);
}

/*
 * Eval named object but env variable is not required and return
 * status of evaluation (success is zero).  This function is assumed
 * to be called by aml_apply_foreach_found_objects().
 * Note that no arguments are passed if object is a method.
 */

int
aml_eval_name_simple(struct aml_name *name, va_list ap)
{
	struct	aml_environ *env;
	union	aml_object *ret;

	if (name == NULL || name->property == NULL) {
		return (1);
	}

	env = memman_alloc(aml_memman, memid_aml_environ);
	if (env == NULL) {
		return (1);
	}
	bzero(env, sizeof(struct aml_environ));

	aml_local_stack_push(aml_local_stack_create());

	AML_DEBUGPRINT("Evaluating ");
	aml_print_curname(name);
	ret = aml_eval_name(env, name);
	if (name->property->type != aml_t_method) {
		AML_DEBUGPRINT("\n");
		if (aml_debug) {
			aml_showobject(ret);
		}
	}

	aml_local_stack_delete(aml_local_stack_pop());

	memman_free(aml_memman, memid_aml_environ, env);
	return (0);
}

int
aml_objtonum(struct aml_environ *env, union aml_object *obj)
{

	if (obj != NULL && obj->type == aml_t_num) {
		return (obj->num.number);
	} else {
		env->stat = aml_stat_panic;
		return (-1);
	}
}

struct aml_name *
aml_execute_method(struct aml_environ *env)
{
	struct	aml_name *name;
	struct	aml_name_group *newgrp;

	newgrp = aml_new_name_group(AML_NAME_GROUP_IN_METHOD);

	AML_DEBUGPRINT("[");
	aml_print_curname(env->curname);
	AML_DEBUGPRINT(" START]\n");

	name = aml_parse_objectlist(env, 0);
	AML_DEBUGPRINT("[");
	aml_print_curname(env->curname);
	AML_DEBUGPRINT(" END]\n");

	aml_delete_name_group(newgrp);
	return (name);
}

union aml_object *
aml_invoke_method(struct aml_name *name, int argc, union aml_object *argv)
{
	int	i;
	struct	aml_name *tmp;
	struct	aml_environ *env;
	struct	aml_local_stack *stack;
	union	aml_object *retval;
	union	aml_object *obj;

	retval = NULL;
	env = memman_alloc(aml_memman, memid_aml_environ);
	if (env == NULL) {
		return (NULL);
	}
	bzero(env, sizeof(struct aml_environ));

	if (name != NULL && name->property != NULL &&
	    name->property->type == aml_t_method) {
		env->curname = name;
		env->dp = name->property->meth.from;
		env->end = name->property->meth.to;
		AML_DEBUGGER(env, env);
		stack = aml_local_stack_create();
		for (i = 0; i < argc; i++) {
			aml_local_stack_getArgX(stack, i)->property =
			    aml_alloc_object(argv[i].type, &argv[i]);
		}
		aml_local_stack_push(stack);
		obj = aml_eval_name(env, tmp = aml_execute_method(env));
		if (aml_debug) {
			aml_showtree(name, 0);
		}

		if (tmp)
			tmp->property = NULL;
		aml_local_stack_delete(aml_local_stack_pop());
		if (obj) {
			aml_create_local_object()->property = obj;
			retval = obj;
		}
	}
	memman_free(aml_memman, memid_aml_environ, env);
	return (retval);
}

union aml_object *
aml_invoke_method_by_name(char *method, int argc, union aml_object *argv)
{
	struct	aml_name *name;

	name = aml_find_from_namespace(aml_get_rootname(), method);
	if (name == NULL) {
		return (NULL);
	}

	return (aml_invoke_method(name, argc, argv));
}
