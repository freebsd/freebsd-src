/*-
 * Copyright (c) 1999 Doug Rabson
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
 *	$Id: aml_parse.c,v 1.32 2000/08/12 15:20:45 iwasaki Exp $
 *	$FreeBSD$
 */

#include <sys/param.h>

#include <aml/aml_amlmem.h>
#include <aml/aml_common.h>
#include <aml/aml_env.h>
#include <aml/aml_evalobj.h>
#include <aml/aml_name.h>
#include <aml/aml_obj.h>
#include <aml/aml_parse.h>
#include <aml/aml_status.h>
#include <aml/aml_store.h>

#ifndef _KERNEL
#include <sys/stat.h>
#include <sys/mman.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#else /* _KERNEL */
#include <sys/systm.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#ifndef ACPI_NO_OSDFUNC_INLINE
#include <machine/acpica_osd.h>
#endif
#endif /* !_KERNEL */

static int		 findsetleftbit(int num);
static int		 findsetrightbit(int num);
static int		 frombcd(int num);
static int		 tobcd(int num);

static u_int32_t	 aml_parse_pkglength(struct aml_environ *env);
static u_int8_t		 aml_parse_bytedata(struct aml_environ *env);
static u_int16_t	 aml_parse_worddata(struct aml_environ *env);
static u_int32_t	 aml_parse_dworddata(struct aml_environ *env);
static u_int8_t		*aml_parse_namestring(struct aml_environ *env);
static void		 aml_parse_defscope(struct aml_environ *env,
					    int indent);
static union aml_object	*aml_parse_defbuffer(struct aml_environ *env,
					     int indent);
static struct aml_name	*aml_parse_concat_number(struct aml_environ *env,
						 int num1, int indent);
static struct aml_name	*aml_parse_concat_buffer(struct aml_environ *env,
						 union aml_object *obj,
						 int indent);
static struct aml_name	*aml_parse_concat_string(struct aml_environ *env,
						 union aml_object *obj,
						 int indent);
static struct aml_name	*aml_parse_concatop(struct aml_environ *env,
					    int indent);
static union aml_object	*aml_parse_defpackage(struct aml_environ *env,
					      int indent);
static void		 aml_parse_defmethod(struct aml_environ *env,
					     int indent);
static void		 aml_parse_defopregion(struct aml_environ *env,
					       int indent);
static int		 aml_parse_field(struct aml_environ *env,
					 struct aml_field *template);
static void		 aml_parse_fieldlist(struct aml_environ *env,
					     struct aml_field *template,
					     int indent);
static void		 aml_parse_deffield(struct aml_environ *env,
					    int indent);
static void		 aml_parse_defindexfield(struct aml_environ *env,
						 int indent);
static void		 aml_parse_defbankfield(struct aml_environ *env,
						int indent);
static void		 aml_parse_defdevice(struct aml_environ *env,
					     int indent);
static void		 aml_parse_defprocessor(struct aml_environ *env,
						int indent);
static void		 aml_parse_defpowerres(struct aml_environ *env,
					       int indent);
static void		 aml_parse_defthermalzone(struct aml_environ *env,
						  int indent);
static struct aml_name	*aml_parse_defelse(struct aml_environ *env,
					   int indent, int num);
static struct aml_name	*aml_parse_defif(struct aml_environ *env,
					 int indent);
static struct aml_name	*aml_parse_defwhile(struct aml_environ *env,
					    int indent);
static void		 aml_parse_defmutex(struct aml_environ *env,
					    int indent);
static void		 aml_createfield_generic(struct aml_environ *env,
						 union aml_object *srcbuf,
						 int index, int len,
						 char *newname);
static void		 aml_parse_defcreatefield(struct aml_environ *env,
						  int indent);

static int
findsetleftbit(int num)
{
	int	i, filter;

	filter = 0;
	for (i = 0; i < 32; i++) {
		filter = filter >> 1;
		filter |= 1 << 31;
		if (filter & num) {
			break;
		}
	}
	i = (i == 32) ? 0 : i + 1;
	return (i);
}

static int
findsetrightbit(int num)
{
	int	i, filter;

	filter = 0;
	for (i = 0; i < 32; i++) {
		filter = filter << 1;
		filter |= 1;
		if (filter & num) {
			break;
		}
	}
	i = (i == 32) ? 0 : i + 1;
	return (i);
}

static int
frombcd(int num)
{
	int	res, factor;

	res = 0;
	factor = 1;
	while (num != 0) {
		res += ((num & 0xf) * factor);
		num = num / 16;
		factor *= 10;
	}
	return (res);
}

static int
tobcd(int num)
{
	int	res, factor;

	res = 0;
	factor = 1;
	while (num != 0) {
		res += ((num % 10) * factor);
		num = num / 10;
		factor *= 16;
	}
	return (res);
}

static u_int32_t
aml_parse_pkglength(struct aml_environ *env)
{
	u_int8_t	*dp;
	u_int32_t	pkglength;

	dp = env->dp;
	pkglength = *dp++;
	switch (pkglength >> 6) {
	case 0:
		break;
	case 1:
		pkglength = (pkglength & 0xf) + (dp[0] << 4);
		dp += 1;
		break;
	case 2:
		pkglength = (pkglength & 0xf) + (dp[0] << 4) + (dp[1] << 12);
		dp += 2;
		break;
	case 3:
		pkglength = (pkglength & 0xf)
		    + (dp[0] << 4) + (dp[1] << 12) + (dp[2] << 20);
		dp += 3;
		break;
	}

	env->dp = dp;
	return (pkglength);
}

static u_int8_t
aml_parse_bytedata(struct aml_environ *env)
{
	u_int8_t	data;

	data = env->dp[0];
	env->dp++;
	return (data);
}

static u_int16_t
aml_parse_worddata(struct aml_environ *env)
{
	u_int16_t	data;

	data = env->dp[0] + (env->dp[1] << 8);
	env->dp += 2;
	return (data);
}

static u_int32_t
aml_parse_dworddata(struct aml_environ *env)
{
	u_int32_t	data;

	data = env->dp[0] + (env->dp[1] << 8) +
	    (env->dp[2] << 16) + (env->dp[3] << 24);
	env->dp += 4;
	return (data);
}

static u_int8_t *
aml_parse_namestring(struct aml_environ *env)
{
	u_int8_t	*name;
	int		segcount;

	name = env->dp;
	if (env->dp[0] == '\\')
		env->dp++;
	else if (env->dp[0] == '^')
		while (env->dp[0] == '^')
			env->dp++;
	if (env->dp[0] == 0x00)	/* NullName */
		env->dp++;
	else if (env->dp[0] == 0x2e)	/* DualNamePrefix */
		env->dp += 1 + 4 + 4;	/* NameSeg, NameSeg */
	else if (env->dp[0] == 0x2f) {	/* MultiNamePrefix */
		segcount = env->dp[1];
		env->dp += 1 + 1 + segcount * 4;	/* segcount * NameSeg */
	} else
		env->dp += 4;	/* NameSeg */

	return (name);
}

struct aml_name *
aml_parse_objectlist(struct aml_environ *env, int indent)
{
	union	aml_object *obj;

	obj = NULL;
	while (env->dp < env->end) {
		aml_print_indent(indent);
		obj = aml_eval_name(env, aml_parse_termobj(env, indent));
		AML_DEBUGPRINT("\n");
		if (env->stat == aml_stat_step) {
			AML_DEBUGGER(env, env);
			continue;
		}
		if (env->stat != aml_stat_none) {
			env->tempname.property = obj;
			return (&env->tempname);
		}
	}
	return (NULL);
}

#define AML_CREATE_NAME(amlname, env, namestr, ret) do {		\
	amlname = aml_create_name(env, namestr);			\
	if (env->stat == aml_stat_panic)				\
		return ret;						\
} while(0)

#define AML_COPY_OBJECT(dest, env, src, ret) do {			\
	dest = aml_copy_object(env, src);				\
	if (dest == NULL) {						\
		env->stat = aml_stat_panic;				\
		return ret;						\
	}								\
} while(0)

#define AML_ALLOC_OBJECT(dest, env, type, ret) do {			\
	dest = aml_alloc_object(type, NULL);				\
	if (dest == NULL) {						\
		env->stat= aml_stat_panic;				\
		return ret;						\
	}								\
} while(0)

static void
aml_parse_defscope(struct aml_environ *env, int indent)
{
	u_int8_t	*start, *end, *oend;
	u_int8_t	*name;
	u_int32_t	pkglength;
	struct	aml_name *oname;

	start = env->dp;
	pkglength = aml_parse_pkglength(env);

	AML_DEBUGPRINT("Scope(");
	name = aml_parse_namestring(env);
	aml_print_namestring(name);
	AML_DEBUGPRINT(") {\n");
	oname = env->curname;
	AML_CREATE_NAME(env->curname, env, name,);
	oend = env->end;
	env->end = end = start + pkglength;
	aml_parse_objectlist(env, indent + 1);
	aml_print_indent(indent);
	AML_DEBUGPRINT("}");
	AML_SYSASSERT(env->dp == env->end);
	env->dp = end;
	env->end = oend;
	env->curname = oname;
	env->stat = aml_stat_none;
}

static union aml_object *
aml_parse_defbuffer(struct aml_environ *env, int indent)
{
	u_int8_t	*start;
	u_int8_t	*end;
	u_int8_t	*buffer;
	u_int32_t	pkglength;
	int	size1, size2, size;
	union	aml_object *obj;

	start = env->dp;
	pkglength = aml_parse_pkglength(env);
	end = start + pkglength;

	AML_DEBUGPRINT("Buffer(");
	obj = aml_eval_name(env, aml_parse_termobj(env, indent));
	size1 = aml_objtonum(env, obj);
	size2 = end - env->dp;
	size = (size1 < size2) ? size1 : size2;
	if (size1 > 0) {
		buffer = memman_alloc_flexsize(aml_memman, size1);
		if (buffer == NULL) {
			AML_DEBUGPRINT("NO MEMORY\n");
			env->stat = aml_stat_panic;
			return (NULL);
		}
		bzero(buffer, size1);
		bcopy(env->dp, buffer, size);
	} else {
		buffer = NULL;
	}

	obj = &env->tempobject;
	obj->type = aml_t_buffer;
	obj->buffer.size = size1;
	obj->buffer.data = buffer;
	AML_DEBUGPRINT(") ");
	env->dp = end;

	return (obj);
}

static struct aml_name *
aml_parse_concat_number(struct aml_environ *env, int num1, int indent)
{
	int	num2;
	struct	aml_name *destname;
	union	aml_object *obj;

	num2 = aml_objtonum(env, aml_eval_name(env,
		aml_parse_termobj(env, indent)));
	AML_DEBUGPRINT(", ");
	destname = aml_parse_termobj(env, indent);
	AML_DEBUGPRINT(")");
	obj = &env->tempobject;
	obj->type = aml_t_buffer;
	obj->buffer.size = 2;
	obj->buffer.data = memman_alloc_flexsize(aml_memman, 2);
	if (obj->buffer.data == NULL) {
		env->stat = aml_stat_panic;
		return (NULL);
	}
	obj->buffer.data[0] = num1 & 0xff;
	obj->buffer.data[1] = num2 & 0xff;
	aml_store_to_name(env, obj, destname);
	return (&env->tempname);
}

static struct aml_name *
aml_parse_concat_buffer(struct aml_environ *env, union aml_object *obj,
    int indent)
{
	union	aml_object *tmpobj, *tmpobj2, *resobj;
	struct	aml_name *destname;

	tmpobj = aml_eval_name(env, aml_parse_termobj(env, indent));
	AML_DEBUGPRINT(", ");
	if (tmpobj->type != aml_t_buffer) {
		env->stat = aml_stat_panic;
		return (NULL);
	}
	AML_COPY_OBJECT(tmpobj2, env, tmpobj, NULL);
	destname = aml_parse_termobj(env, indent);
	AML_DEBUGPRINT(")");
	resobj = &env->tempobject;
	env->tempname.property = resobj;
	resobj->buffer.type = aml_t_buffer;
	resobj->buffer.size = tmpobj2->buffer.size + obj->buffer.size;
	if (resobj->buffer.size > 0) {
		resobj->buffer.data = memman_alloc_flexsize(aml_memman,
		    resobj->buffer.size);
		if (resobj->buffer.data == NULL) {
			env->stat = aml_stat_panic;
			return (NULL);
		}
		bcopy(obj->buffer.data, resobj->buffer.data, obj->buffer.size);
		bcopy(tmpobj2->buffer.data,
		    resobj->buffer.data + obj->buffer.size,
		    tmpobj2->buffer.size);
	} else {
		resobj->buffer.data = NULL;
	}
	aml_free_object(&tmpobj2);
	aml_store_to_name(env, resobj, destname);
	return (&env->tempname);
}

static struct aml_name *
aml_parse_concat_string(struct aml_environ *env, union aml_object *obj,
    int indent)
{
	int	len;
	union	aml_object *tmpobj, *tmpobj2, *resobj;
	struct	aml_name *destname;

	tmpobj = aml_eval_name(env, aml_parse_termobj(env, indent));
	AML_DEBUGPRINT(", ");
	if (tmpobj->type != aml_t_string) {
		env->stat = aml_stat_panic;
		return (NULL);
	}
	AML_COPY_OBJECT(tmpobj2, env, tmpobj, NULL);
	destname = aml_parse_termobj(env, indent);
	AML_DEBUGPRINT(")");
	resobj = &env->tempobject;
	env->tempname.property = resobj;
	resobj->type = aml_t_buffer;
	resobj->str.needfree = 1;
	len = strlen(obj->str.string) + strlen(tmpobj2->str.string) + 1;
	if (len > 0) {
		resobj->str.string = memman_alloc_flexsize(aml_memman, len);
		if (resobj->str.string == NULL) {
			env->stat = aml_stat_panic;
			return (NULL);
		}
		strncpy(resobj->str.string, obj->str.string, len);
		strcat(resobj->str.string, tmpobj->str.string);
	} else {
		resobj->str.string = NULL;
	}
	aml_free_object(&tmpobj2);
	aml_store_to_name(env, resobj, destname);
	return (&env->tempname);
}

static struct aml_name *
aml_parse_concatop(struct aml_environ *env, int indent)
{
	union	aml_object *obj, *tmpobj;
	struct	aml_name *aname;

	AML_DEBUGPRINT("Concat(");
	obj = aml_eval_name(env, aml_parse_termobj(env, indent));
	AML_DEBUGPRINT(", ");
	switch (obj->type) {
	case aml_t_num:
		aname = aml_parse_concat_number(env, aml_objtonum(env, obj), indent);
		break;

	case aml_t_buffer:
		/* obj may be temporal object */
		AML_COPY_OBJECT(tmpobj, env, obj, NULL);
		aname = aml_parse_concat_buffer(env, obj, indent);
		aml_free_object(&tmpobj);
		break;

	case aml_t_string:
		/* obj may be temporal object */
		AML_COPY_OBJECT(tmpobj, env, obj, NULL);
		aname = aml_parse_concat_string(env, obj, indent);
		aml_free_object(&tmpobj);
		break;

	default:
		env->stat = aml_stat_panic;
		aname = NULL;
		break;
	}

	AML_DEBUGPRINT("\n");
	return (aname);
}

static union aml_object *
aml_parse_defpackage(struct aml_environ *env, int indent)
{
	u_int8_t	numelements;
	u_int8_t	*start;
	u_int32_t	pkglength;
	int		i;
	struct	aml_environ *copy;
	struct	aml_name *tmpname;
	union	aml_object *obj, **objects;

	start = env->dp;
	pkglength = aml_parse_pkglength(env);
	numelements = aml_parse_bytedata(env);
	copy = memman_alloc(aml_memman, memid_aml_environ);
	if (copy == NULL) {
		env->stat = aml_stat_panic;
		return (NULL);
	}
	if (numelements > 0) {
		objects = memman_alloc_flexsize(aml_memman,
		    numelements * sizeof(union aml_object *));
		if (objects == NULL) {
			env->stat = aml_stat_panic;
			return (NULL);
		} else {
			bzero(objects, numelements * sizeof(union aml_object *));
		}
	} else {
		objects = NULL;
	}

	*copy = *env;
	env->dp = copy->end = start + pkglength;
	AML_DEBUGPRINT("Package() {\n");
	i = 0;
	while ((copy->dp < copy->end) && (i < numelements)) {
		aml_print_indent(indent + 1);
		tmpname = aml_parse_termobj(copy, indent + 1);

		if (tmpname != NULL) {
			objects[i] = aml_copy_object(copy, tmpname->property);
		}
		AML_DEBUGPRINT(",\n");
		i++;
	}
	aml_free_objectcontent(&copy->tempobject);

	aml_print_indent(indent);
	AML_DEBUGPRINT("}");
	obj = &env->tempobject;
	obj->type = aml_t_package;
	obj->package.elements = numelements;
	obj->package.objects = objects;

	memman_free(aml_memman, memid_aml_environ, copy);
	return (obj);
}

static void
aml_parse_defmethod(struct aml_environ *env, int indent)
{
	u_int8_t	flags;
	u_int8_t	*start;
	u_int32_t	pkglength;
	char	*name;
	struct	aml_environ *copy;
	struct	aml_method *meth;
	struct	aml_name *aname;
	union	aml_object *aobj;

	start = env->dp;
	pkglength = aml_parse_pkglength(env);
	copy = memman_alloc(aml_memman, memid_aml_environ);
	if (copy == NULL) {
		env->stat = aml_stat_panic;
		return;
	}
	AML_DEBUGPRINT("Method(");
	name = aml_parse_namestring(env);
	aml_print_namestring(name);
	AML_CREATE_NAME(aname, env, name,);
	if (aname->property != NULL) {
		env->stat = aml_stat_panic;
		AML_DEBUGPRINT("Already Defined \n");
		goto out;
	}
	AML_ALLOC_OBJECT(aobj, env, aml_t_method,);
	meth = &aobj->meth;
	aname->property = aobj;
	flags = *env->dp++;

	if (flags) {
		AML_DEBUGPRINT(", %d", flags);
	}
	AML_DEBUGPRINT(") {\n");
	*copy = *env;
	meth->argnum = flags;
	meth->from = env->dp;
	meth->to = env->dp = copy->end = start + pkglength;
	aml_print_indent(indent);
	AML_DEBUGPRINT("}");
out:
	memman_free(aml_memman, memid_aml_environ, copy);
}

static void
aml_parse_defopregion(struct aml_environ *env, int indent)
{
	u_int8_t	*name;
	struct	aml_name *aname;
	struct	aml_opregion *opregion;
	union	aml_object *obj;
	const char	*regions[] = {
		"SystemMemory",
		"SystemIO",
		"PCI_Config",
		"EmbeddedControl",
		"SMBus",
	};

	AML_DEBUGPRINT("OperationRegion(");
	/* Name */
	name = aml_parse_namestring(env);
	aml_print_namestring(name);
	AML_CREATE_NAME(aname, env, name,);
	if (aname->property != NULL) {
		env->stat = aml_stat_panic;
		AML_DEBUGPRINT("Already Defined \n");
		return;
	}
	AML_ALLOC_OBJECT(aname->property, env, aml_t_opregion,);
	opregion = &aname->property->opregion;
	opregion->space = *env->dp;
	AML_DEBUGPRINT(", %s, ", regions[*env->dp]);	/* Space */
	env->dp++;
	obj = aml_eval_name(env, aml_parse_termobj(env, indent));	/* Offset */
	opregion->offset = aml_objtonum(env, obj);
	AML_DEBUGPRINT(", ");
	obj = aml_eval_name(env, aml_parse_termobj(env, indent));	/* Length */
	opregion->length = aml_objtonum(env, obj);
	AML_DEBUGPRINT(")");
}

static const char	*accessnames[] = {
	"AnyAcc",
	"ByteAcc",
	"WordAcc",
	"DWordAcc",
	"BlockAcc",
	"SMBSendRecvAcc",
	"SMBQuickAcc"
};

static int
aml_parse_field(struct aml_environ *env, struct aml_field *template)
{
	u_int8_t	*name;
	u_int8_t	access, attribute;
	u_int32_t	width;
	struct	aml_name *aname;
	struct	aml_field *prop;

	switch (*env->dp) {
	case '\\':
	case '^':
	case 'A'...'Z':
	case '_':
	case '.':  
		name = aml_parse_namestring(env);
		width = aml_parse_pkglength(env);
		template->bitlen = width;
		aml_print_namestring(name);
		AML_CREATE_NAME(aname, env, name, NULL);
		/* Allignment */
		if (width == 16) {
			template->bitoffset += 15;
			template->bitoffset &= (~15);
		}
		if (width == 32) {
			template->bitoffset += 31;
			template->bitoffset &= (~31);
		} else if ((width & 7) == 0) {
			template->bitoffset += 7;
			template->bitoffset &= (~7);
		} else if ((width > 32) && (width & 7) != 0) {
			AML_DEBUGPRINT("??? Can I treat it?\n");
		}
		if (aname->property != NULL) {
			env->stat = aml_stat_panic;
			AML_DEBUGPRINT("Already Defined \n");
			return (NULL);
		}
		AML_ALLOC_OBJECT(aname->property, env, aml_t_field, NULL);
		prop = &aname->property->field;
		*prop = *template;
		template->bitoffset += width;
		AML_DEBUGPRINT(",\t%d", width);
		break;
	case 0x00:
		env->dp++;
		width = aml_parse_pkglength(env);
		template->bitoffset += width;
		AML_DEBUGPRINT("Offset(0x%x)", template->bitoffset);
		break;
	case 0x01:
		access = env->dp[1];
		attribute = env->dp[2];
		env->dp += 3;
		AML_DEBUGPRINT("AccessAs(%s, %d)", accessnames[access], attribute);
		template->bitoffset = attribute;
		template->flags = (template->flags | 0xf0) | access;
		break;
	}
	return (template->bitoffset);
}

static void
aml_parse_fieldlist(struct aml_environ *env, struct aml_field *template,
    int indent)
{
	u_int32_t	offset;

	offset = 0;
	while (env->dp < env->end) {
		aml_print_indent(indent);
		offset = aml_parse_field(env, template);
		if (env->dp < env->end) {
			AML_DEBUGPRINT(",\n");
		} else {
			AML_DEBUGPRINT("\n");
		}
	}
}

static void
aml_parse_deffield(struct aml_environ *env, int indent)
{
	u_int8_t	flags;
	u_int8_t	*start, *name;
	u_int32_t	pkglength;
	struct	aml_environ *copy;
	struct	aml_field fieldtemplate;
	static	const char *lockrules[] = {"NoLock", "Lock"};
	static	const char *updaterules[] = {"Preserve", "WriteAsOnes",
					     "WriteAsZeros", "*Error*"};

	start = env->dp;
	pkglength = aml_parse_pkglength(env);
	copy = memman_alloc(aml_memman, memid_aml_environ);
	if (copy == NULL) {
		env->stat = aml_stat_panic;
		return;
	}
	AML_DEBUGPRINT("Field(");
	aml_print_namestring(name = aml_parse_namestring(env));
	fieldtemplate.type = aml_t_field;
	flags = aml_parse_bytedata(env);
	fieldtemplate.flags = fieldtemplate.flags = flags;

	*copy = *env;
	env->dp = copy->end = start + pkglength;
	fieldtemplate.bitoffset = 0;
	fieldtemplate.bitlen = 0;
	fieldtemplate.f.ftype = f_t_field;
	fieldtemplate.f.fld.regname = name;
	AML_DEBUGPRINT(", %s, %s, %s) {\n",
	    accessnames[flags & 0xf],
	    lockrules[(flags >> 4) & 1],
	    updaterules[(flags >> 5) & 3]);
	aml_parse_fieldlist(copy, &fieldtemplate, indent + 1);
	aml_print_indent(indent);
	AML_DEBUGPRINT("}");
	aml_free_objectcontent(&copy->tempobject);

	AML_SYSASSERT(copy->dp == copy->end);
	memman_free(aml_memman, memid_aml_environ, copy);
}

static void
aml_parse_defindexfield(struct aml_environ *env, int indent)
{
	u_int8_t	flags;
	u_int8_t	*start, *iname, *dname;
	u_int32_t	pkglength;
	struct	aml_environ *copy;
	struct	aml_field template;
	static	const char *lockrules[] = {"NoLock", "Lock"};
	static	const char *updaterules[] = {"Preserve", "WriteAsOnes",
					     "WriteAsZeros", "*Error*"};

	start = env->dp;
	pkglength = aml_parse_pkglength(env);
	copy = memman_alloc(aml_memman, memid_aml_environ);
	if (copy == NULL) {
		env->stat = aml_stat_panic;
		return;
	}
	AML_DEBUGPRINT("IndexField(");
	aml_print_namestring(iname = aml_parse_namestring(env));	/* Name1 */
	AML_DEBUGPRINT(", ");
	aml_print_namestring(dname = aml_parse_namestring(env));	/* Name2 */
	template.type = aml_t_field;
	template.flags = flags = aml_parse_bytedata(env);
	template.bitoffset = 0;
	template.bitlen = 0;
	template.f.ftype = f_t_index;
	template.f.ifld.indexname = iname;
	template.f.ifld.dataname = dname;
	AML_DEBUGPRINT(", %s, %s, %s) {\n",
	    accessnames[flags & 0xf],
	    lockrules[(flags >> 4) & 1],
	    updaterules[(flags >> 5) & 3]);
	*copy = *env;
	env->dp = copy->end = start + pkglength;
	aml_parse_fieldlist(copy, &template, indent + 1);
	aml_print_indent(indent);
	AML_DEBUGPRINT("}");
	aml_free_objectcontent(&copy->tempobject);

	AML_SYSASSERT(copy->dp == copy->end);
	memman_free(aml_memman, memid_aml_environ, copy);
}

static void
aml_parse_defbankfield(struct aml_environ *env, int indent)
{
	u_int8_t	flags;
	u_int8_t	*start, *rname, *bname;
	u_int32_t	pkglength, bankvalue;
	struct	aml_environ *copy;
	struct	aml_field template;
	union	aml_object *obj;
	static	const char *lockrules[] = {"NoLock", "Lock"};
	static	const char *updaterules[] = {"Preserve", "WriteAsOnes",
					     "WriteAsZeros", "*Error*"};

	start = env->dp;
	pkglength = aml_parse_pkglength(env);
	copy = memman_alloc(aml_memman, memid_aml_environ);
	if (copy == NULL) {
		env->stat = aml_stat_panic;
		return;
	}
	AML_DEBUGPRINT("BankField(");
	aml_print_namestring(rname = aml_parse_namestring(env));	/* Name1 */
	AML_DEBUGPRINT(", ");
	aml_print_namestring(bname = aml_parse_namestring(env));	/* Name2 */
	AML_DEBUGPRINT(", ");
	obj = aml_eval_name(env, aml_parse_termobj(env, indent));	/* BankValue */
	bankvalue = aml_objtonum(env, obj);
	template.type = aml_t_field;
	template.flags = flags = aml_parse_bytedata(env);
	template.bitoffset = 0;
	template.bitlen = 0;
	template.f.ftype = f_t_bank;
	template.f.bfld.regname = rname;
	template.f.bfld.bankname = bname;
	template.f.bfld.bankvalue = bankvalue;
	*copy = *env;
	env->dp = copy->end = start + pkglength;
	AML_DEBUGPRINT(", %s, %s, %s) {\n",
	    accessnames[flags & 0xf],
	    lockrules[(flags >> 4) & 1],
	    updaterules[(flags >> 5) & 3]);
	aml_parse_fieldlist(copy, &template, indent + 1);
	aml_print_indent(indent);
	AML_DEBUGPRINT("}");

	aml_free_objectcontent(&copy->tempobject);
	AML_SYSASSERT(copy->dp == copy->end);
	memman_free(aml_memman, memid_aml_environ, copy);
}

static void
aml_parse_defdevice(struct aml_environ *env, int indent)
{
	u_int8_t	*start;
	u_int8_t	*name;
	u_int32_t	pkglength;
	struct	aml_environ *copy;

	start = env->dp;
	pkglength = aml_parse_pkglength(env);
	copy = memman_alloc(aml_memman, memid_aml_environ);
	if (copy == NULL) {
		env->stat = aml_stat_panic;
		return;
	}
	AML_DEBUGPRINT("Device(");
	name = aml_parse_namestring(env);
	aml_print_namestring(name);
	AML_DEBUGPRINT(") {\n");
	*copy = *env;
	AML_CREATE_NAME(copy->curname, env, name,);
	if (copy->curname->property != NULL) {
		env->stat = aml_stat_panic;
		AML_DEBUGPRINT("Already Defined \n");
		goto out;
	}
	AML_ALLOC_OBJECT(copy->curname->property, env, aml_t_device,);
	env->dp = copy->end = start + pkglength;
	aml_parse_objectlist(copy, indent + 1);
	aml_print_indent(indent);
	AML_DEBUGPRINT("}");
	aml_free_objectcontent(&copy->tempobject);

	AML_SYSASSERT(copy->dp == copy->end);
out:
	memman_free(aml_memman, memid_aml_environ, copy);
}

static void
aml_parse_defprocessor(struct aml_environ *env, int indent)
{
	u_int8_t	*start;
	u_int8_t	*name;
	u_int32_t	pkglength;
	struct	aml_environ *copy;
	struct	aml_processor *proc;
	union	aml_object *obj;

	start = env->dp;
	pkglength = aml_parse_pkglength(env);
	copy = memman_alloc(aml_memman, memid_aml_environ);
	if (copy == NULL) {
		env->stat = aml_stat_panic;
		return;
	}
	AML_ALLOC_OBJECT(obj, env, aml_t_processor,);
	proc = &obj->proc;
	AML_DEBUGPRINT("Processor(");
	name = aml_parse_namestring(env);
	aml_print_namestring(name);
	proc->id = aml_parse_bytedata(env);
	proc->addr = aml_parse_dworddata(env);
	proc->len = aml_parse_bytedata(env);
	AML_DEBUGPRINT(", %d, 0x%x, 0x%x) {\n", proc->id, proc->addr, proc->len);
	*copy = *env;
	AML_CREATE_NAME(copy->curname, env, name,);
	if (copy->curname->property != NULL) {
		env->stat = aml_stat_panic;
		AML_DEBUGPRINT("Already Defined \n");
		goto out;
	}
	copy->curname->property = obj;
	env->dp = copy->end = start + pkglength;
	aml_parse_objectlist(copy, indent + 1);
	aml_print_indent(indent);
	AML_DEBUGPRINT("}");
	aml_free_objectcontent(&copy->tempobject);

	AML_SYSASSERT(copy->dp == copy->end);
out:
	memman_free(aml_memman, memid_aml_environ, copy);
}

static void
aml_parse_defpowerres(struct aml_environ *env, int indent)
{
	u_int8_t	*start;
	u_int8_t	*name;
	u_int32_t	pkglength;
	struct	aml_environ *copy;
	struct	aml_powerres *pres;
	union	aml_object *obj;

	start = env->dp;
	pkglength = aml_parse_pkglength(env);
	copy = memman_alloc(aml_memman, memid_aml_environ);
	if (copy == NULL) {
		env->stat = aml_stat_panic;
		return;
	}
	AML_DEBUGPRINT("PowerResource(");
	AML_ALLOC_OBJECT(obj, env, aml_t_powerres,);
	name = aml_parse_namestring(env);
	aml_print_namestring(name);
	pres = &obj->pres;
	pres->level = aml_parse_bytedata(env);
	pres->order = aml_parse_worddata(env);
	AML_DEBUGPRINT(", %d, %d) {\n", pres->level, pres->order);
	*copy = *env;
	AML_CREATE_NAME(copy->curname, env, name,);
	if (copy->curname->property != NULL) {
		env->stat = aml_stat_panic;
		AML_DEBUGPRINT("Already Defined \n");
		goto out;
	}
	copy->curname->property = obj;
	env->dp = copy->end = start + pkglength;

	aml_parse_objectlist(copy, indent + 1);
	aml_print_indent(indent);
	AML_DEBUGPRINT("}");
	aml_free_objectcontent(&copy->tempobject);

	AML_SYSASSERT(copy->dp == copy->end);
out:
	memman_free(aml_memman, memid_aml_environ, copy);
}

static void
aml_parse_defthermalzone(struct aml_environ *env, int indent)
{
	u_int8_t	*start;
	u_int8_t	*name;
	u_int32_t	pkglength;
	struct	aml_environ *copy;

	start = env->dp;
	pkglength = aml_parse_pkglength(env);
	copy = memman_alloc(aml_memman, memid_aml_environ);
	if (copy == NULL) {
		env->stat = aml_stat_panic;
		return;
	}
	AML_DEBUGPRINT("ThermalZone(");
	name = aml_parse_namestring(env);
	aml_print_namestring(name);
	AML_DEBUGPRINT(") {\n");
	*copy = *env;
	AML_CREATE_NAME(copy->curname, env, name,);
	if (copy->curname->property != NULL) {
		env->stat = aml_stat_panic;
		AML_DEBUGPRINT("Already Defined \n");
		goto out;
	}
	AML_ALLOC_OBJECT(copy->curname->property, env, aml_t_therm,);
	env->dp = copy->end = start + pkglength;
	aml_parse_objectlist(copy, indent + 1);
	aml_print_indent(indent);
	AML_DEBUGPRINT("}");
	aml_free_objectcontent(&copy->tempobject);
	AML_SYSASSERT(copy->dp == copy->end);
out:
	memman_free(aml_memman, memid_aml_environ, copy);
}

static struct aml_name *
aml_parse_defelse(struct aml_environ *env, int indent, int num)
{
	u_int8_t	*start, *end, *oend;
	u_int32_t	pkglength;
	struct	aml_name *aname;

	start = env->dp;
	pkglength = aml_parse_pkglength(env);
	oend = env->end;
	env->end = end = start + pkglength;
	aname = NULL;

	AML_DEBUGPRINT("Else {\n");
	if (num == 0) {
		aname = aml_parse_objectlist(env, indent + 1);
		aml_print_indent(indent);
	}
	AML_DEBUGPRINT("}");

	env->dp = end;
	env->end = oend;
	return (aname);
}

static struct aml_name *
aml_parse_defif(struct aml_environ *env, int indent)
{
	u_int8_t	*start, *end, *oend;
	u_int32_t	pkglength;
	int	num;
	struct	aml_name *aname, *aname1;

	start = env->dp;
	pkglength = aml_parse_pkglength(env);
	aname = NULL;

	AML_DEBUGPRINT("If(");
	num = aml_objtonum(env, aml_eval_name
	    (env, aml_parse_termobj(env, indent)));
	oend = env->end;
	end = start + pkglength;
	AML_DEBUGPRINT(")");
	if (num) {
		AML_DEBUGPRINT("{\n");
		env->end = end;
		aname = aml_parse_objectlist(env, indent + 1);
		aml_print_indent(indent);
		AML_DEBUGPRINT("}");
	}
	env->dp = end;
	env->end = oend;
	if ((end < oend) && *(env->dp) == 0xa1) {
		env->dp++;
		aname1 = aml_parse_defelse(env, indent, num);
		aname = (num == 0) ? aname1 : aname;
	}
	return (aname);
}

static struct aml_name *
aml_parse_defwhile(struct aml_environ *env, int indent)
{
	u_int8_t	*start, *end, *oend;
	u_int32_t	pkglength;
	int	num;
	struct	aml_name *aname;

	start = env->dp;
	pkglength = aml_parse_pkglength(env);
	oend = env->end;
	end = start + pkglength;
	aname = NULL;
	for (;;) {
		env->dp = start;
		aml_parse_pkglength(env);
		AML_DEBUGPRINT("While(");
		num = aml_objtonum(env, aml_eval_name
		    (env, aml_parse_termobj(env, indent)));
		AML_DEBUGPRINT(")");
		if (num == 0) {
			break;
		}
		AML_DEBUGPRINT(" {\n");
		env->end = end;
		aname = aml_parse_objectlist(env, indent + 1);
		if (env->stat == aml_stat_step) {
			AML_DEBUGGER(env, env);
			continue;
		}
		if (env->stat != aml_stat_none)
			break;
		aml_print_indent(indent);
		AML_DEBUGPRINT("}");
	}
	AML_DEBUGPRINT("\n");
	env->dp = end;
	env->end = oend;
	if (env->stat == aml_stat_break) {
		env->stat = aml_stat_none;
		aname = NULL;
	}
	return (aname);
}

static void
aml_parse_defmutex(struct aml_environ *env, int indent)
{
	char	*name;
	struct	aml_name *aname;
	struct	aml_mutex *mut;

	/* MutexOp */
	AML_DEBUGPRINT("Mutex(");
	name = aml_parse_namestring(env);
	aml_print_namestring(name);
	AML_CREATE_NAME(aname, env, name,);
	if (aname->property != NULL) {
		env->stat = aml_stat_panic;
		AML_DEBUGPRINT("Already Defined \n");
		return;
	}
	AML_ALLOC_OBJECT(aname->property, env, aml_t_mutex,);
	mut = &aname->property->mutex;
	mut->level = *env->dp++;
	STAILQ_INIT(&mut->queue);
	AML_DEBUGPRINT(", %d)", mut->level);
}

static void
aml_createfield_generic(struct aml_environ *env,
    union aml_object *srcbuf, int index,
    int len, char *newname)
{
	struct	aml_bufferfield *field;
	struct	aml_name *aname;

	if (srcbuf == NULL || srcbuf->type != aml_t_buffer) {
		AML_DEBUGPRINT("Not Buffer assigned,");
		env->stat = aml_stat_panic;
		return;
	}
	AML_CREATE_NAME(aname, env, newname,);
	if (aname->property != NULL) {
		env->stat = aml_stat_panic;
		AML_DEBUGPRINT("Already Defined \n");
		return;
	}
	AML_ALLOC_OBJECT(aname->property, env, aml_t_bufferfield,);
	field = &aname->property->bfld;
	field->bitoffset = index;
	field->bitlen = len;
	field->origin = srcbuf->buffer.data;
}

static void
aml_parse_defcreatefield(struct aml_environ *env, int indent)
{
	int	index, len;
	char	*newname;
	union	aml_object *obj, *srcbuf;

	/* CreateFieldOp */
	AML_DEBUGPRINT("CreateField(");
	srcbuf = aml_eval_name(env, aml_parse_termobj(env, indent));
	if (srcbuf == &env->tempobject) {
		AML_DEBUGPRINT("NONAMED BUFFER\n");
		env->stat = aml_stat_panic;
		return;
	}
	AML_DEBUGPRINT(", ");
	obj = aml_eval_name(env, aml_parse_termobj(env, indent));
	index = aml_objtonum(env, obj);
	AML_DEBUGPRINT(", ");
	obj = aml_eval_name(env, aml_parse_termobj(env, indent));
	len = aml_objtonum(env, obj);
	AML_DEBUGPRINT(", ");
	newname = aml_parse_namestring(env);
	aml_print_namestring(newname);
	aml_createfield_generic(env, srcbuf, index, len, newname);
	AML_DEBUGPRINT(") ");
}

/*
 * Returns Named object or parser buffer. The object need not be free because
 * it returns preallocated buffer in env or Contain of named object.  If You
 * need to preserve object, create a copy and then store.  And The object
 * returned from this function is not valid after another call is
 * shared, tempolary buffer may be shared.
 */
struct aml_name *
aml_parse_termobj(struct aml_environ *env, int indent)
{
	u_int8_t	opcode;
	u_int8_t	*name;
	int	value;
	int	num1, num2;
	int	len;
	int	match1, match2, i, pkgval, start;
	int	widthindex, index;
	char	*newname;
	struct	aml_name *aname;
	struct	aml_name *destname1, *destname2;
	struct	aml_name *tmpname, *srcname;
	struct	aml_name *src;
	union	aml_object *ret;
	union	aml_object *tmpobj;
	union	aml_object anum;
	union	aml_object *objref;
	union	aml_object *srcobj;
	union	aml_object *obj;
	union	aml_object *srcbuf;
	static int	widthtbl[4] = {32, 16, 8, 1};
	const char	*opname[4] = {"CreateDWordField", "CreateWordField",
				      "CreateByteField", "CreateBitField"};

	aname = &env->tempname;
	ret = &env->tempobject;
	anum.type = aml_t_num;
	aname->property = ret;
	aml_free_objectcontent(ret);
	if (env->stat == aml_stat_panic) {
		/*
		 * If previosuly parser panic , parsing next instruction is
		 * prohibited.
		 */
		return (NULL);
	}
	aname = NULL;
	opcode = *env->dp++;
	switch (opcode) {
	case '\\':
	case '^':
	case 'A' ... 'Z':
	case '_':
	case '.':
		env->dp--;
		ret->type = aml_t_namestr;
		ret->nstr.dp = aml_parse_namestring(env);
		aml_print_namestring(ret->nstr.dp);
		aname = &env->tempname;
		break;
	case 0x0a:		/* BytePrefix */
		ret->type = aml_t_num;
		value = aml_parse_bytedata(env);
		ret->num.number = value;
		AML_DEBUGPRINT("0x%x", value);
		aname = &env->tempname;
		break;
	case 0x0b:		/* WordPrefix */
		ret->type = aml_t_num;
		value = aml_parse_worddata(env);
		ret->num.number = value;
		AML_DEBUGPRINT("0x%x", value);
		aname = &env->tempname;
		break;
	case 0x0c:		/* DWordPrefix */
		ret->type = aml_t_num;
		value = aml_parse_dworddata(env);
		ret->num.number = value;
		AML_DEBUGPRINT("0x%x", value);
		aname = &env->tempname;
		break;
	case 0x0d:		/* StringPrefix */
		ret->type = aml_t_string;
		ret->str.string = env->dp;
		len = strlen(env->dp);
		ret->str.needfree = 0;
		AML_DEBUGPRINT("\"%s\"", (const char *)ret->str.string);
		env->dp += (len + 1);
		aname = &env->tempname;
		break;
	case 0x00:		/* ZeroOp */
		ret->type = aml_t_num;
		ret->num.number = 0;
		ret->num.constant = 1;
		AML_DEBUGPRINT("Zero");
		aname = &env->tempname;
		break;
	case 0x01:		/* OneOp */
		ret->type = aml_t_num;
		ret->num.number = 1;
		ret->num.constant = 1;
		AML_DEBUGPRINT("One");
		aname = &env->tempname;
		break;
	case 0xff:		/* OnesOp */
		ret->type = aml_t_num;
		ret->num.number = 0xffffffff;
		ret->num.constant = 1;
		AML_DEBUGPRINT("Ones");
		aname = &env->tempname;
		break;
	case 0x06:		/* AliasOp */
		AML_DEBUGPRINT("Alias(");
		tmpname = aml_parse_termobj(env, indent);
		if (env->stat == aml_stat_panic) {
			return (NULL);
		}
		if (tmpname->property == NULL ||
		    tmpname->property->type != aml_t_namestr) {
			env->stat = aml_stat_panic;
			return (NULL);
		}
		/*
		 * XXX if srcname is deleted after this object, what
		 * shall I do?
		 */
		srcname = aml_search_name(env, tmpname->property->nstr.dp);
		AML_DEBUGPRINT(", ");
		name = aml_parse_namestring(env);
		aml_print_namestring(name);
		AML_CREATE_NAME(aname, env, name, NULL);
		if (aname->property != NULL) {
			env->stat = aml_stat_panic;
			AML_DEBUGPRINT("Already Defined \n");
			aml_print_curname(aname);
			return (NULL);
		}
		AML_ALLOC_OBJECT(aname->property, env, aml_t_objref, NULL);
		objref = aname->property;
		objref->objref.nameref = srcname;
		objref->objref.ref = srcname->property;
		objref->objref.offset = -1;
		objref->objref.alias = 1;	/* Yes, this is an alias */
		AML_DEBUGPRINT(")");
		/* shut the interpreter up during the namespace initializing */
		return (NULL);
	case 0x08:		/* NameOp */
		AML_DEBUGPRINT("Name(");
		name = aml_parse_namestring(env);
		aml_print_namestring(name);
		AML_CREATE_NAME(aname, env, name, NULL);
		if (env->stat == aml_stat_panic) {
			AML_DEBUGPRINT("Already Defined \n");
			aml_print_curname(aname);
			return (NULL);
		}
		AML_DEBUGPRINT(", ");
		AML_COPY_OBJECT(aname->property, env,
		    aml_eval_name(env,
			aml_parse_termobj(env, indent)),
		    NULL);
		AML_DEBUGPRINT(")");
		break;
	case 0x10:		/* ScopeOp */
		aml_parse_defscope(env, indent);
		break;
	case 0x11:		/* BufferOp */
		aname = &env->tempname;
		aname->property = aml_parse_defbuffer(env, indent);
		break;
	case 0x12:		/* PackageOp */
		aname = &env->tempname;
		aname->property = aml_parse_defpackage(env, indent);
		break;
	case 0x14:		/* MethodOp */
		aml_parse_defmethod(env, indent);
		break;
	case 0x5b:		/* ExtOpPrefix */
		opcode = *env->dp++;
		switch (opcode) {
		case 0x01:
			aml_parse_defmutex(env, indent);
			break;
		case 0x02:	/* EventOp */
			AML_DEBUGPRINT("Event(");
			name = aml_parse_namestring(env);
			aml_print_namestring(name);
			AML_CREATE_NAME(aname, env, name, NULL);
			if (aname->property != NULL) {
				env->stat = aml_stat_panic;
				AML_DEBUGPRINT("Already Defined \n");
				return (NULL);
			}
			AML_ALLOC_OBJECT(aname->property, env, aml_t_event, NULL);
			AML_DEBUGPRINT(")");
			return (NULL);
			break;
		case 0x12:	/* CondRefOfOp */
			AML_DEBUGPRINT("CondRefOf(");
			src = aml_parse_termobj(env, indent);
			AML_DEBUGPRINT(", ");
			if (src == &env->tempname || src == NULL) {
				aml_parse_termobj(env, indent);
				AML_DEBUGPRINT(")");
				anum.num.number = 0xffffffff;
				env->tempobject.num = anum.num;
				aname = &env->tempname;
				break;
			}
			AML_ALLOC_OBJECT(objref, env, aml_t_objref, NULL);
			if (src->property == NULL ||
			    src->property->type != aml_t_namestr) {
				objref->objref.nameref = src;
			} else {
				objref->objref.nameref = aml_create_local_object();
			}
			objref->objref.ref = src->property;
			objref->objref.offset = -1;	/* different from IndexOp */
			
			destname1 = aml_parse_termobj(env, indent);
			aml_store_to_name(env, objref, destname1);
			anum.num.number = 0;
			env->tempobject.num = anum.num;
			aname = &env->tempname;
			AML_DEBUGPRINT(")");
			break;
		case 0x13:
			aml_parse_defcreatefield(env, indent);
			break;
		case 0x20:	/* LoadOp *//* XXX Not Impremented */
			AML_DEBUGPRINT("Load(");
			aml_parse_termobj(env, indent);
			AML_DEBUGPRINT(", ");
			aml_parse_termobj(env, indent);
			AML_DEBUGPRINT(")");
			break;
		case 0x21:	/* StallOp */
			AML_DEBUGPRINT("Stall(");
			num1 = aml_objtonum(env, aml_eval_name(env,
			    aml_parse_termobj(env, indent)));
			AML_DEBUGPRINT(")");
			AML_STALL(num1);
			break;
		case 0x22:	/* SleepOp */
			AML_DEBUGPRINT("Sleep(");
			num1 = aml_objtonum(env, aml_eval_name(env,
			    aml_parse_termobj(env, indent)));
			AML_SLEEP(0, num1);
			AML_DEBUGPRINT(")");
			break;
		case 0x23:	/* AcquireOp *//* XXX Not yet */
			AML_DEBUGPRINT("Acquire(");
			aml_parse_termobj(env, indent);
			AML_DEBUGPRINT(", 0x%x)", aml_parse_worddata(env));
			break;
		case 0x24:	/* SignalOp *//* XXX Not yet */
			AML_DEBUGPRINT("Signal(");
			aml_parse_termobj(env, indent);
			AML_DEBUGPRINT(")");
			break;
		case 0x25:	/* WaitOp *//* XXX Not yet impremented */
			AML_DEBUGPRINT("Wait(");
			aml_parse_termobj(env, indent);
			AML_DEBUGPRINT(", ");
			aml_parse_termobj(env, indent);
			AML_DEBUGPRINT(")");
			break;
		case 0x26:	/* ResetOp *//* XXX Not yet impremented */
			AML_DEBUGPRINT("Reset(");
			aml_parse_termobj(env, indent);
			AML_DEBUGPRINT(")");
			break;
		case 0x27:	/* ReleaseOp *//* XXX Not yet impremented */
			AML_DEBUGPRINT("Release(");
			aml_parse_termobj(env, indent);
			AML_DEBUGPRINT(")");
			break;
#define NUMOP2(opname, operation) do {					\
	AML_DEBUGPRINT(opname);						\
	AML_DEBUGPRINT("(");						\
	num1 = aml_objtonum(env, aml_eval_name(env,			\
	    aml_parse_termobj(env, indent)));				\
	AML_DEBUGPRINT(", ");						\
	anum.num.number = operation (num1);				\
	destname1 = aml_parse_termobj(env, indent);			\
	AML_DEBUGPRINT(")");						\
	aml_store_to_name(env, &anum, destname1);			\
	env->tempobject.num = anum.num;					\
	env->tempname.property = &env->tempobject;			\
	aname = &env->tempname;						\
} while(0)

		case 0x28:	/* FromBCDOp */
			NUMOP2("FromBCD", frombcd);
			break;
		case 0x29:	/* ToBCDOp */
			NUMOP2("ToBCD", tobcd);
			break;
		case 0x2a:	/* UnloadOp *//* XXX Not yet impremented */
			AML_DEBUGPRINT("Unload(");
			aml_parse_termobj(env, indent);
			AML_DEBUGPRINT(")");
			break;
		case 0x30:
			env->tempobject.type = aml_t_num;
			env->tempobject.num.number = 0;
			env->tempobject.num.constant = 1;
			AML_DEBUGPRINT("Revision");
			break;
		case 0x31:
			env->tempobject.type = aml_t_debug;
			aname = &env->tempname;
			AML_DEBUGPRINT("Debug");
			break;
		case 0x32:	/* FatalOp */
			AML_DEBUGPRINT("Fatal(");
			AML_DEBUGPRINT("0x%x, ", aml_parse_bytedata(env));
			AML_DEBUGPRINT("0x%x, ", aml_parse_dworddata(env));
			aml_parse_termobj(env, indent);
			env->stat = aml_stat_panic;
			AML_DEBUGPRINT(")");
			break;
		case 0x80:	/* OpRegionOp */
			aml_parse_defopregion(env, indent);
			break;
		case 0x81:	/* FieldOp */
			aml_parse_deffield(env, indent);
			break;
		case 0x82:	/* DeviceOp */
			aml_parse_defdevice(env, indent);
			break;
		case 0x83:	/* ProcessorOp */
			aml_parse_defprocessor(env, indent);
			break;
		case 0x84:	/* PowerResOp */
			aml_parse_defpowerres(env, indent);
			break;
		case 0x85:	/* ThermalZoneOp */
			aml_parse_defthermalzone(env, indent);
			break;
		case 0x86:	/* IndexFieldOp */
			aml_parse_defindexfield(env, indent);
			break;
		case 0x87:	/* BankFieldOp */
			aml_parse_defbankfield(env, indent);
			break;
		default:
			AML_SYSERRX(1, "strange opcode 0x5b, 0x%x\n", opcode);
			AML_SYSABORT();
		}
		break;
	case 0x68 ... 0x6e:	/* ArgN */
		AML_DEBUGPRINT("Arg%d", opcode - 0x68);
		return (aml_local_stack_getArgX(NULL, opcode - 0x68));
		break;
	case 0x60 ... 0x67:
		AML_DEBUGPRINT("Local%d", opcode - 0x60);
		return (aml_local_stack_getLocalX(opcode - 0x60));
		break;
	case 0x70:		/* StoreOp */
		AML_DEBUGPRINT("Store(");
		aname = aml_create_local_object();
		AML_COPY_OBJECT(tmpobj, env,
		    aml_eval_name(env,	aml_parse_termobj(env, indent)), NULL);
		aname->property = tmpobj;
		AML_DEBUGPRINT(", ");
		destname1 = aml_parse_termobj(env, indent);
		AML_DEBUGPRINT(")");
		/* XXX
		 * temporary object may change during aml_store_to_name()
		 * operation, so we make a copy of it on stack.
		 */
		if (destname1 == &env->tempname &&
		    destname1->property == &env->tempobject) {
			destname1 = aml_create_local_object();
			AML_COPY_OBJECT(destname1->property, env,
			    &env->tempobject, NULL);
		}
		aml_store_to_name(env, tmpobj, destname1);
		if (env->stat == aml_stat_panic) {
			AML_DEBUGPRINT("StoreOp failed");
			return (NULL);
		}
		aname = aml_create_local_object();
		AML_COPY_OBJECT(tmpobj, env, destname1->property, NULL);
		aname->property = tmpobj;
		if (tmpobj == NULL) {
			printf("???");
			break;
		}
		break;
	case 0x71:		/* RefOfOp */
		AML_DEBUGPRINT("RefOf(");
		src = aml_parse_termobj(env, indent);
		AML_DEBUGPRINT(")");

		aname = aml_create_local_object();
		AML_ALLOC_OBJECT(aname->property, env, aml_t_objref, NULL);
		objref = aname->property;
		if (src->property == NULL ||
		    src->property->type != aml_t_namestr) {
			objref->objref.nameref = src;
		} else {
			objref->objref.nameref = aml_create_local_object();
		}
		objref->objref.ref = src->property;
		objref->objref.offset = -1;	/* different from IndexOp */
		break;

#define NUMOP3_2(opname, oparation, ope2) do {				\
	AML_DEBUGPRINT(opname);						\
	AML_DEBUGPRINT("(");						\
	num1 = aml_objtonum(env, aml_eval_name(env,			\
	    aml_parse_termobj(env, indent)));				\
	AML_DEBUGPRINT(", ");						\
	num2 = aml_objtonum(env, aml_eval_name(env,			\
	    aml_parse_termobj(env, indent)));				\
	AML_DEBUGPRINT(", ");						\
	anum.num.number = ope2(num1 oparation num2);			\
	destname1 = aml_parse_termobj(env, indent);			\
	AML_DEBUGPRINT(")");						\
	aml_store_to_name(env, &anum, destname1);			\
	env->tempobject.num = anum.num;					\
	env->tempname.property = &env->tempobject;			\
	aname = &env->tempname;						\
} while(0)

#define NUMOP3(opname, operation)	NUMOP3_2(opname, operation, )
#define NUMOPN3(opname, operation)	NUMOP3_2(opname, operation, ~)

	case 0x72:		/* AddOp */
		NUMOP3("Add", +);
		break;
	case 0x73:		/* ConcatOp  */
		aname = aml_parse_concatop(env, indent);
		break;
	case 0x74:		/* SubtractOp */
		NUMOP3("Subtract", -);
		break;
	case 0x75:		/* IncrementOp */
		AML_DEBUGPRINT("Increment(");
		aname = aml_parse_termobj(env, indent);
		num1 = aml_objtonum(env, aml_eval_name(env, aname));
		num1++;
		anum.num.number = num1;
		AML_DEBUGPRINT(")");
		aml_store_to_name(env, &anum, aname);
		aname = &env->tempname;
		env->tempobject.num = anum.num;
		break;
	case 0x76:		/* DecrementOp */
		AML_DEBUGPRINT("Decrement(");
		aname = aml_parse_termobj(env, indent);
		num1 = aml_objtonum(env, aml_eval_name(env, aname));
		num1--;
		anum.num.number = num1;
		AML_DEBUGPRINT(")");
		aml_store_to_name(env, &anum, aname);
		aname = &env->tempname;
		env->tempobject.num = anum.num;
		break;
	case 0x77:		/* MultiplyOp */
		NUMOP3("Multiply", *);
		break;
	case 0x78:		/* DivideOp */
		AML_DEBUGPRINT("Divide(");
		num1 = aml_objtonum(env, aml_eval_name(env,
		    aml_parse_termobj(env, indent)));
		AML_DEBUGPRINT(", ");
		num2 = aml_objtonum(env, aml_eval_name(env,
		    aml_parse_termobj(env, indent)));
		AML_DEBUGPRINT(", ");
		anum.num.number = num1 % num2;
		destname1 = aml_parse_termobj(env, indent);
		aml_store_to_name(env, &anum, destname1);
		AML_DEBUGPRINT(", ");
		anum.num.number = num1 / num2;
		destname2 = aml_parse_termobj(env, indent);
		AML_DEBUGPRINT(")");
		aml_store_to_name(env, &anum, destname2);
		env->tempobject.num = anum.num;
		aname = &env->tempname;
		break;
	case 0x79:		/* ShiftLeftOp */
		NUMOP3("ShiftLeft", <<);
		break;
	case 0x7a:		/* ShiftRightOp */
		NUMOP3("ShiftRight", >>);
		break;
	case 0x7b:		/* AndOp */
		NUMOP3("And", &);
		break;
	case 0x7c:		/* NAndOp */
		NUMOPN3("NAnd", &);
		break;
	case 0x7d:		/* OrOp */
		NUMOP3("Or", |);
		break;
	case 0x7e:		/* NOrOp */
		NUMOPN3("NOr", |);
		break;
	case 0x7f:		/* XOrOp */
		NUMOP3("XOr", ^);
		break;
	case 0x80:		/* NotOp */
		NUMOP2("Not", ~);
		break;
	case 0x81:		/* FindSetLeftBitOp */
		NUMOP2("FindSetLeftBit", findsetleftbit);
		break;
	case 0x82:		/* FindSetRightBitOp */
		NUMOP2("FindSetRightBit", findsetrightbit);
		break;
	case 0x83:		/* DerefOp */
		AML_DEBUGPRINT("DerefOf(");
		objref = aml_eval_name(env, aml_parse_termobj(env, indent));
		AML_DEBUGPRINT(")");

		if (objref->objref.ref == NULL) {
			env->tempname.property = objref->objref.ref;
			aname = &env->tempname;
			break;
		}
		switch (objref->objref.ref->type) {
		case aml_t_package:
		case aml_t_buffer:
			if (objref->objref.offset < 0) {
				env->tempname.property = objref->objref.ref;
			} else {
				objref->objref.deref = 1;
				env->tempname.property = objref;
			}
			break;
		default:
			env->tempname.property = objref->objref.ref;
			break;
		}

		aname = &env->tempname;
		break;
	case 0x86:		/* NotifyOp *//* XXX Not yet impremented */
		AML_DEBUGPRINT("Notify(");
		aml_parse_termobj(env, indent);
		AML_DEBUGPRINT(", ");
		aml_parse_termobj(env, indent);
		AML_DEBUGPRINT(")");
		break;
	case 0x87:		/* SizeOfOp */
		AML_DEBUGPRINT("SizeOf(");
		aname = aml_parse_termobj(env, indent);
		tmpobj = aml_eval_name(env, aname);

		AML_DEBUGPRINT(")");
		num1 = 0;
		switch (tmpobj->type) {
		case aml_t_buffer:
			num1 = tmpobj->buffer.size;
			break;
		case aml_t_string:
			num1 = strlen(tmpobj->str.string);
			break;
		case aml_t_package:
			num1 = tmpobj->package.elements;
			break;
		default:
			AML_DEBUGPRINT("Args of SizeOf should be "
				       "buffer/string/package only\n");
			break;
		}

		anum.num.number = num1;
		env->tempobject.num = anum.num;
		aname = &env->tempname;
		break;
	case 0x88:		/* IndexOp */
		AML_DEBUGPRINT("Index(");
		srcobj = aml_eval_name(env, aml_parse_termobj(env, indent));
		AML_DEBUGPRINT(", ");
		num1 = aml_objtonum(env, aml_eval_name(env,
		    aml_parse_termobj(env, indent)));
		AML_DEBUGPRINT(", ");
		destname1 = aml_parse_termobj(env, indent);
		AML_DEBUGPRINT(")");
		aname = aml_create_local_object();
		switch (srcobj->type) {
		case aml_t_package:
		case aml_t_buffer:
			AML_ALLOC_OBJECT(objref, env, aml_t_objref, NULL);
			aname->property = objref;
			objref->objref.ref = srcobj;
			objref->objref.offset = num1;
			objref->objref.deref = 0;
			break;
		default:
			AML_DEBUGPRINT("Arg0 of Index should be either "
				       "buffer or package\n");
			return (aname);
		}

		aml_store_to_name(env, objref, destname1);
		break;
	case 0x89:		/* MatchOp *//* XXX Not yet Impremented */
		AML_DEBUGPRINT("Match(");
		AML_COPY_OBJECT(obj, env, aml_eval_name(env,
		    aml_parse_termobj(env, indent)), NULL);
		if (obj->type != aml_t_package) {
			env->stat = aml_stat_panic;
			return (NULL);
		}
		anum.num.number = 0xffffffff;
		match1 = *env->dp;
		AML_DEBUGPRINT(", %d", *env->dp);
		env->dp++;
		num1 = aml_objtonum(env, aml_eval_name(env,
		    aml_parse_termobj(env, indent)));
		match2 = *env->dp;
		AML_DEBUGPRINT(", %d", *env->dp);
		env->dp++;
		num2 = aml_objtonum(env, aml_eval_name(env,
		    aml_parse_termobj(env, indent)));
		AML_DEBUGPRINT(", ");
		start = aml_objtonum(env, aml_eval_name(env,
		    aml_parse_termobj(env, indent)));

#define MATCHOP(opnum, arg1, arg2)	((opnum == 0) ? (1) :		\
    (opnum == 1) ? ((arg1) == (arg2))	:				\
    (opnum == 2) ? ((arg1) <= (arg2))	:				\
    (opnum == 3) ? ((arg1) <  (arg2))	:				\
    (opnum == 4) ? ((arg1) >= (arg2))	:				\
    (opnum == 5) ? ((arg1) >  (arg2))	: 0 )

		for (i = start; i < obj->package.elements; i++) {
			pkgval = aml_objtonum(env, obj->package.objects[i]);
			if (MATCHOP(match1, pkgval, num1) &&
			    MATCHOP(match2, pkgval, num2)) {
				anum.num.number = i;
				break;
			}
		}
		AML_DEBUGPRINT(")");
		aml_free_object(&obj);
		aname = &env->tempname;
		env->tempname.property = &env->tempobject;
		env->tempobject.num = anum.num;
		break;
#undef MATCHOP
	case 0x8a ... 0x8d:	/* CreateDWordFieldOp */
		widthindex = *(env->dp - 1) - 0x8a;
		AML_DEBUGPRINT("%s(", opname[widthindex]);
		srcbuf = aml_eval_name(env, aml_parse_termobj(env, indent));
		if (srcbuf == &env->tempobject) {
			AML_DEBUGPRINT("NOT NAMEDBUF\n");
			env->stat = aml_stat_panic;
			return (NULL);
		}
		AML_DEBUGPRINT(", ");
		index = aml_objtonum(env, aml_eval_name(env,
		    aml_parse_termobj(env, indent)));
		if (widthindex != 3) {
			index *= 8;
		}
		AML_DEBUGPRINT(", ");
		newname = aml_parse_namestring(env);
		aml_print_namestring(newname);
		aml_createfield_generic(env, srcbuf, index,
		    widthtbl[widthindex], newname);
		AML_DEBUGPRINT(")");
		break;
	case 0x8e:		/* ObjectTypeOp */
		AML_DEBUGPRINT("ObjectType(");
		aname = aml_parse_termobj(env, indent);
		if (aname == NULL) {
			env->tempobject.type = aml_t_num;
			env->tempobject.num.number = aml_t_null;
		} else {
			env->tempobject.type = aml_t_num;
			env->tempobject.num.number = aname->property->type;
		}
		aname = &env->tempname;
		AML_DEBUGPRINT(")");
		break;

#define CMPOP(opname,operation) do {					\
	AML_DEBUGPRINT(opname);						\
	AML_DEBUGPRINT("(");						\
	num1 = aml_objtonum(env, aml_eval_name(env,			\
	    aml_parse_termobj(env, indent)));				\
	AML_DEBUGPRINT(", ");						\
	num2 = aml_objtonum(env, aml_eval_name(env,			\
	    aml_parse_termobj(env, indent)));				\
	aname = &env->tempname;						\
	env->tempobject.type = aml_t_num;				\
	env->tempobject.num.number = (num1 operation num2) ? 0xffffffff : 0;	\
	aname->property = &env->tempobject;				\
	AML_DEBUGPRINT(")");						\
} while(0)

	case 0x90:
		CMPOP("LAnd", &&);
		break;
	case 0x91:
		CMPOP("LOr", ||);
		break;
	case 0x92:
		AML_DEBUGPRINT("LNot(");
		num1 = aml_objtonum(env, aml_eval_name(env,
		    aml_parse_termobj(env, indent)));
		aname = &env->tempname;
		env->tempobject.type = aml_t_num;
		env->tempobject.num.number = (!num1) ? 0xffffffff : 0;
		aname->property = &env->tempobject;
		AML_DEBUGPRINT(")");
		break;
	case 0x93:
		CMPOP("LEqual", ==);
		break;
	case 0x94:
		CMPOP("LGreater", >);
		break;
	case 0x95:
		CMPOP("LLess", <);
		break;
	case 0xa0:		/* IfOp */
		aname = aml_parse_defif(env, indent);
		break;
#if 0

	case 0xa1:		/* ElseOp should not be treated in Main parser
				 * But If Op */
		aml_parse_defelse(env, indent);
		break;
#endif
	case 0xa2:		/* WhileOp */
		aname = aml_parse_defwhile(env, indent);
		break;
	case 0xa3:		/* NoopOp */
		AML_DEBUGPRINT("Noop");
		break;
	case 0xa5:		/* BreakOp */
		AML_DEBUGPRINT("Break");
		env->stat = aml_stat_break;
		break;
	case 0xa4:		/* ReturnOp */
		AML_DEBUGPRINT("Return(");
		AML_COPY_OBJECT(env->tempname.property, env, aml_eval_name(env,
		    aml_parse_termobj(env, indent)), NULL);
		aname = &env->tempname;
		env->stat = aml_stat_return;
		AML_DEBUGPRINT(")");
		break;
	case 0xcc:		/* BreakPointOp */
		/* XXX Not Yet Impremented (Not need?) */
		AML_DEBUGPRINT("BreakPoint");
		break;
	default:
		AML_SYSERRX(1, "strange opcode 0x%x\n", opcode);
		AML_SYSABORT();
	}

	return (aname);
}
