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
 *	$Id: aml_store.c,v 1.22 2000/08/09 14:47:44 iwasaki Exp $
 *	$FreeBSD$
 */

#include <sys/param.h>

#include <aml/aml_amlmem.h>
#include <aml/aml_common.h>
#include <aml/aml_env.h>
#include <aml/aml_evalobj.h>
#include <aml/aml_name.h>
#include <aml/aml_obj.h>
#include <aml/aml_region.h>
#include <aml/aml_status.h>
#include <aml/aml_store.h>

#ifndef _KERNEL
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "debug.h"
#else /* _KERNEL */
#include <sys/systm.h>
#endif /* !_KERNEL */

static void
aml_store_to_fieldname(struct aml_environ *env, union aml_object *obj,
    struct aml_name *name)
{
	char	*buffer;
	struct	aml_name *wname, *oname, *iname;
	struct	aml_field *field;
	struct	aml_opregion *or;
	union	aml_object tobj, iobj, *tmpobj;

	field = &name->property->field;
	oname = env->curname;
	iname = NULL;
	env->curname = name->parent;
	if (field->f.ftype == f_t_field) {
		wname = aml_search_name(env, field->f.fld.regname);
		if (wname == NULL ||
		    wname->property == NULL ||
		    wname->property->type != aml_t_opregion) {
			AML_DEBUGPRINT("Inappropreate Type\n");
			env->stat = aml_stat_panic;
			env->curname = oname;
			return;
		}
		or = &wname->property->opregion;
		switch (obj->type) {
		case aml_t_num:
			aml_region_write(env, or->space, field->flags,
			    obj->num.number, or->offset,
			    field->bitoffset, field->bitlen);
			AML_DEBUGPRINT("[write(%d, 0x%x, 0x%x)]",
			    or->space, obj->num.number,
			    or->offset + field->bitoffset / 8);
			break;
		case aml_t_buffer:
		case aml_t_bufferfield:
			if (obj->type == aml_t_buffer) {
				buffer = obj->buffer.data;
			} else {
				buffer = obj->bfld.origin;
				buffer += obj->bfld.bitoffset / 8;
			}
			aml_region_write_from_buffer(env, or->space,
			    field->flags, buffer, or->offset, field->bitoffset,
			    field->bitlen);
			break;
		case aml_t_regfield:
			if (or->space != obj->regfield.space) {
				AML_DEBUGPRINT("aml_store_to_fieldname: "
					       "Different type of space\n");
				break;
			}
			aml_region_bcopy(env, obj->regfield.space,
			    obj->regfield.flags, obj->regfield.offset,
			    obj->regfield.bitoffset, obj->regfield.bitlen,
			    field->flags, or->offset, field->bitoffset,
			    field->bitlen);
			break;
		default:
			AML_DEBUGPRINT("aml_store_to_fieldname: "
				       "Inappropreate Type of src object\n");
			break;
		}
	} else if (field->f.ftype == f_t_index) {
		iname = aml_search_name(env, field->f.ifld.indexname);
		wname = aml_search_name(env, field->f.ifld.dataname);
		iobj.type = aml_t_num;
		iobj.num.number = field->bitoffset / 8;	/* AccessType Boundary */

		/* read whole values of IndexField */
		aml_store_to_name(env, &iobj, iname);
		tmpobj = aml_eval_name(env, wname);

		/* make the values to be written */
		tobj.num = obj->num;
 		tobj.num.number = aml_adjust_updatevalue(field->flags,
 		    field->bitoffset & 7, field->bitlen,
 		    tmpobj->num.number, obj->num.number);

		/* write the values to IndexField */
		aml_store_to_name(env, &iobj, iname);
		aml_store_to_name(env, &tobj, wname);
	}
	env->curname = oname;
}

static void
aml_store_to_buffer(struct aml_environ *env, union aml_object *obj,
    union aml_object *buf, int offset)
{
	int	size;
	int	bitlen;

	switch (obj->type) {
	case aml_t_num:
		if (offset > buf->buffer.size) {
			aml_realloc_object(buf, offset);
		}
		buf->buffer.data[offset] = obj->num.number & 0xff;
		AML_DEBUGPRINT("[Store number 0x%x to buffer]",
		    obj->num.number & 0xff);
		break;
	case aml_t_string:
		size = strlen(obj->str.string);
		if (buf->buffer.size - offset < size) {
			aml_realloc_object(buf, offset + size + 1);
		}
		strcpy(&buf->buffer.data[offset], obj->str.string);
		AML_DEBUGPRINT("[Store string to buffer]");
		break;
	case aml_t_buffer:
		bzero(buf->buffer.data, buf->buffer.size);
		if (obj->buffer.size > buf->buffer.size) {
			size = buf->buffer.size;
		} else {
			size = obj->buffer.size;
		}
		bcopy(obj->buffer.data, buf->buffer.data, size);
		break;
	case aml_t_regfield:
		bitlen = (buf->buffer.size - offset) * 8;
		if (bitlen > obj->regfield.bitlen) {
			bitlen = obj->regfield.bitlen;
		}
		aml_region_read_into_buffer(env, obj->regfield.space,
		    obj->regfield.flags, obj->regfield.offset,
		    obj->regfield.bitoffset, bitlen,
		    buf->buffer.data + offset);
		break;
	default:
		goto not_yet;
	}
	return;
not_yet:
	AML_DEBUGPRINT("[XXX not supported yet]");
}


void
aml_store_to_object(struct aml_environ *env, union aml_object *src,
    union aml_object * dest)
{
	char	*buffer, *srcbuf;
	int	offset, bitlen;

	switch (dest->type) {
	case aml_t_num:
		if (src->type == aml_t_num) {
			dest->num = src->num;
			AML_DEBUGPRINT("[Store number 0x%x]", src->num.number);
		} else {
			env->stat = aml_stat_panic;
		}
		break;
	case aml_t_string:
	case aml_t_package:
		break;
	case aml_t_buffer:
		aml_store_to_buffer(env, src, dest, 0);
		break;
	case aml_t_bufferfield:
		buffer = dest->bfld.origin;
		offset = dest->bfld.bitoffset;
		bitlen = dest->bfld.bitlen;

		switch (src->type) {
		case aml_t_num:
			if (aml_bufferfield_write(src->num.number, buffer, offset, bitlen)) {
				AML_DEBUGPRINT("aml_bufferfield_write() failed\n");
			}
			break;
		case aml_t_buffer:
		case aml_t_bufferfield:
			if (src->type == aml_t_buffer) {
				srcbuf = src->buffer.data;
			} else {
				srcbuf = src->bfld.origin;
				srcbuf += src->bfld.bitoffset / 8;
			}
			bcopy(srcbuf, buffer, bitlen / 8);
			break;
		case aml_t_regfield:
			aml_region_read_into_buffer(env, src->regfield.space,
			    src->regfield.flags, src->regfield.offset,
			    src->regfield.bitoffset, src->regfield.bitlen,
			    buffer);
			break;
		default:
			AML_DEBUGPRINT("not implemented yet");
			break;
		}
		break;
	case aml_t_debug:
		aml_showobject(src);
		break;
	default:
		AML_DEBUGPRINT("[Unimplemented %d]", dest->type);
		break;
	}
}

static void
aml_store_to_objref(struct aml_environ *env, union aml_object *obj,
    union aml_object *r)
{
	int	offset;
	union	aml_object *ref;

	if (r->objref.ref == NULL) {
		r->objref.ref = aml_alloc_object(obj->type, NULL);	/* XXX */
		r->objref.nameref->property = r->objref.ref;
	}
	ref = r->objref.ref;

	switch (ref->type) {
	case aml_t_buffer:
		offset = r->objref.offset;
		aml_store_to_buffer(env, obj, ref, r->objref.offset);
		break;
	case aml_t_package:
		offset = r->objref.offset;
		if (r->objref.ref->package.elements < offset) {
			aml_realloc_object(ref, offset);
		}
		if (ref->package.objects[offset] == NULL) {
			ref->package.objects[offset] = aml_copy_object(env, obj);
		} else {
			aml_store_to_object(env, obj, ref->package.objects[offset]);
		}
		break;
	default:
		aml_store_to_object(env, obj, ref);
		break;
	}
}

/*
 * Store to Named object
 */
void
aml_store_to_name(struct aml_environ *env, union aml_object *obj,
    struct aml_name *name)
{
	struct	aml_name *wname;

	if (env->stat == aml_stat_panic) {
		return;
	}
	if (name == NULL || obj == NULL) {
		AML_DEBUGPRINT("[Try to store no existant name ]");
		return;
	}
	if (name->property == NULL) {
		name->property = aml_copy_object(env, obj);
		AML_DEBUGPRINT("[Copy number 0x%x]", obj->num.number);
		return;
	}
	if (name->property->type == aml_t_namestr) {
		wname = aml_search_name(env, name->property->nstr.dp);
		name = wname;
	}
	if (name == NULL) {
		env->stat = aml_stat_panic;
		return;
	}
	if (name->property == NULL || name->property->type == aml_t_null) {
		name->property = aml_copy_object(env, obj);
		AML_DEBUGPRINT("[Copy number 0x%x]", obj->num.number);
		return;
	}
	/* Writes to constant object are not allowed */
	if (name->property != NULL && name->property->type == aml_t_num &&
	    name->property->num.constant == 1) {
		return;
	}
	/* try to dereference */
	if (obj->type == aml_t_objref && obj->objref.deref == 0) {
		AML_DEBUGPRINT("Source object isn't dereferenced yet, "
			       "try to dereference anyway\n");
		obj->objref.deref = 1;
		obj = aml_eval_objref(env, obj);
	}
	switch (name->property->type) {
	case aml_t_field:
		aml_store_to_fieldname(env, obj, name);
		break;
	case aml_t_objref:
		aml_store_to_objref(env, obj, name->property);
		break;
	case aml_t_num:
		if (name == &env->tempname)
			break;
	default:
		aml_store_to_object(env, obj, name->property);
		break;
	}
}
