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
 *	$Id: aml_obj.c,v 1.17 2000/08/12 15:20:45 iwasaki Exp $
 *	$FreeBSD$
 */

#include <sys/param.h>

#include <dev/acpi/aml/aml_amlmem.h>
#include <dev/acpi/aml/aml_env.h>
#include <dev/acpi/aml/aml_name.h>
#include <dev/acpi/aml/aml_obj.h>
#include <dev/acpi/aml/aml_status.h>
#include <dev/acpi/aml/aml_store.h>

#ifndef _KERNEL
#include <sys/stat.h>
#include <sys/mman.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#else /* _KERNEL */
#include <sys/systm.h>
#endif /* !_KERNEL */

union aml_object *
aml_copy_object(struct aml_environ *env, union aml_object *orig)
{
	int	i;
	union	aml_object *ret;

	if (orig == NULL)
		return (NULL);
	switch (orig->type) {
	case aml_t_regfield:
		ret = aml_alloc_object(aml_t_buffer, 0);
		ret->buffer.size = (orig->regfield.bitlen / 8) +
		    ((orig->regfield.bitlen % 8) ? 1 : 0);
		if (ret->buffer.size == 0) {
			goto out;
		}
		ret->buffer.data = memman_alloc_flexsize(aml_memman, ret->buffer.size);
		aml_store_to_object(env, orig, ret);
		break;

	default:
		ret = aml_alloc_object(0, orig);
		break;
	}

	if (1 || orig != &env->tempobject) {	/* XXX */
		if (orig->type == aml_t_buffer) {
			if (orig->buffer.size == 0) {
				goto out;
			}
			ret->buffer.data = memman_alloc_flexsize(aml_memman,
			    orig->buffer.size);
			bcopy(orig->buffer.data, ret->buffer.data, orig->buffer.size);
		} else if (orig->type == aml_t_package) {
			if (ret->package.elements == 0) {
				goto out;
			}
			ret->package.objects = memman_alloc_flexsize(aml_memman,
			    ret->package.elements * sizeof(union aml_object *));
			for (i = 0; i < ret->package.elements; i++) {
				ret->package.objects[i] = aml_copy_object(env, orig->package.objects[i]);
			}
		} else if (orig->type == aml_t_string && orig->str.needfree != 0) {
			ret->str.string = memman_alloc_flexsize(aml_memman,
			    strlen(orig->str.string) + 1);
			strcpy(orig->str.string, ret->str.string);
		} else if (orig->type == aml_t_num) {
                        ret->num.constant = 0;
                }
	} else {
		printf("%s:%d\n", __FILE__, __LINE__);
		env->tempobject.type = aml_t_null;
	}
out:
	return ret;
}

/*
 * This function have two function: copy or allocate.  if orig != NULL,
 *  orig is duplicated.
 */

union aml_object *
aml_alloc_object(enum aml_objtype type, union aml_object *orig)
{
	unsigned	int memid;
	union	aml_object *ret;

	if (orig != NULL) {
		type = orig->type;
	}
	switch (type) {
	case aml_t_namestr:
		memid = memid_aml_namestr;
		break;
	case aml_t_buffer:
		memid = memid_aml_buffer;
		break;
	case aml_t_string:
		memid = memid_aml_string;
		break;
	case aml_t_bufferfield:
		memid = memid_aml_bufferfield;
		break;
	case aml_t_package:
		memid = memid_aml_package;
		break;
	case aml_t_num:
		memid = memid_aml_num;
		break;
	case aml_t_powerres:
		memid = memid_aml_powerres;
		break;
	case aml_t_opregion:
		memid = memid_aml_opregion;
		break;
	case aml_t_method:
		memid = memid_aml_method;
		break;
	case aml_t_processor:
		memid = memid_aml_processor;
		break;
	case aml_t_field:
		memid = memid_aml_field;
		break;
	case aml_t_mutex:
		memid = memid_aml_mutex;
		break;
	case aml_t_device:
		memid = memid_aml_objtype;
		break;
	case aml_t_objref:
		memid = memid_aml_objref;
		break;
	default:
		memid = memid_aml_objtype;
		break;
	}
	ret = memman_alloc(aml_memman, memid);
	ret->type = type;

	if (orig != NULL) {
		bcopy(orig, ret, memman_memid2size(aml_memman, memid));
	}
	return (ret);
}

void
aml_free_objectcontent(union aml_object *obj)
{
	int	i;

	if (obj->type == aml_t_buffer && obj->buffer.data != NULL) {
		memman_free_flexsize(aml_memman, obj->buffer.data);
		obj->buffer.data = NULL;
	}
	if (obj->type == aml_t_string && obj->str.string != NULL) {
		if (obj->str.needfree != 0) {
			memman_free_flexsize(aml_memman, obj->str.string);
			obj->str.string = NULL;
		}
	}
	if (obj->type == aml_t_package && obj->package.objects != NULL) {
		for (i = 0; i < obj->package.elements; i++) {
			aml_free_object(&obj->package.objects[i]);
		}
		memman_free_flexsize(aml_memman, obj->package.objects);
		obj->package.objects = NULL;
	}
}

void
aml_free_object(union aml_object **obj)
{
	union	aml_object *body;

	body = *obj;
	if (body == NULL) {
		return;
	}
	aml_free_objectcontent(*obj);
	memman_free(aml_memman, memid_unkown, *obj);
	*obj = NULL;
}

void
aml_realloc_object(union aml_object *obj, int size)
{
	int	i;
	enum	aml_objtype type;
	union	aml_object tmp;

	type = obj->type;
	switch (type) {
	case aml_t_buffer:
		if (obj->buffer.size >= size) {
			return;
		}
		tmp.buffer.size = size;
		tmp.buffer.data = memman_alloc_flexsize(aml_memman, size);
		bzero(tmp.buffer.data, size);
		bcopy(obj->buffer.data, tmp.buffer.data, obj->buffer.size);
		aml_free_objectcontent(obj);
		*obj = tmp;
		break;
	case aml_t_string:
		if (strlen(obj->str.string) >= size) {
			return;
		}
		tmp.str.string = memman_alloc_flexsize(aml_memman, size + 1);
		strcpy(tmp.str.string, obj->str.string);
		aml_free_objectcontent(obj);
		*obj = tmp;
		break;
	case aml_t_package:
		if (obj->package.elements >= size) {
			return;
		}
		tmp.package.objects = memman_alloc_flexsize(aml_memman,
		    size * sizeof(union aml_object *));
		bzero(tmp.package.objects, size * sizeof(union aml_object *));
		for (i = 0; i < obj->package.elements; i++) {
			tmp.package.objects[i] = obj->package.objects[i];
		}
		memman_free_flexsize(aml_memman, obj->package.objects);
		obj->package.objects = tmp.package.objects;
		break;
	default:
		break;
	}
}
