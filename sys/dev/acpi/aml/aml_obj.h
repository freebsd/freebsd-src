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
 *	$Id: aml_obj.h,v 1.15 2000/08/09 14:47:43 iwasaki Exp $
 *	$FreeBSD$
 */

#ifndef _AML_OBJ_H_
#define _AML_OBJ_H_

#include <sys/queue.h>

struct aml_environ;
enum aml_objtype {
	aml_t_namestr = -3,
	aml_t_regfield,
	aml_t_objref,
	aml_t_null = 0,
	aml_t_num,
	aml_t_string,
	aml_t_buffer,
	aml_t_package,
	aml_t_device,
	aml_t_field,
	aml_t_event,
	aml_t_method,
	aml_t_mutex,
	aml_t_opregion,
	aml_t_powerres,
	aml_t_processor,
	aml_t_therm,
	aml_t_bufferfield,
	aml_t_ddbhandle,
	aml_t_debug
};

struct	aml_namestr {
	enum	aml_objtype type;	/* =aml_t_namestr */
	u_int8_t	*dp;
};

struct	aml_opregion {
	enum	aml_objtype type;
	int	space;
	int	offset;
	int	length;
};

struct	aml_num {
	enum	aml_objtype type;	/* =aml_t_num */
	int	number;
	int	constant;
};

struct	aml_package {
	enum	aml_objtype type;
	int	elements;
	union	aml_object **objects;
};

struct	aml_string {
	enum	aml_objtype type;	/* =aml_t_string */
	int	needfree;
	u_int8_t	*string;
};

struct	aml_buffer {
	enum	aml_objtype type;	/* =aml_t_buffer */
	int	size;
	u_int8_t	*data;		/* This should be free when 
					 * this object is free.
					 */
};

enum	fieldtype {
	f_t_field,
	f_t_index,
	f_t_bank
};

struct	nfieldd {
	enum	fieldtype ftype;	/* f_t_field */
	u_int8_t	*regname;	/* Namestring */
};

struct	ifieldd {
	enum	fieldtype ftype;	/* f_t_index */
	u_int8_t	*indexname;
	u_int8_t	*dataname;
};

struct	bfieldd {
	enum	fieldtype ftype;	/* f_t_bank */
	u_int8_t	*regname;
	u_int8_t	*bankname;
	u_int32_t	bankvalue;
};

struct	aml_field {
	enum	aml_objtype type;
	u_int32_t	flags;
	int	bitoffset;		/* Not Byte offset but bitoffset */
	int	bitlen;
	union {
		enum	fieldtype ftype;
		struct	nfieldd fld;
		struct	ifieldd ifld;
		struct	bfieldd bfld;
	}     f;
};

struct	aml_bufferfield {
	enum	aml_objtype type;	/* aml_t_bufferfield */
	int	bitoffset;
	int	bitlen;
	u_int8_t	*origin;	/* This should not be free
					 * when this object is free
					 * (Within Buffer object)
					 */
};

struct	aml_method {
	enum	aml_objtype type;
	int	argnum;		/* Not argnum but argnum|frag */
	u_int8_t	*from;
	u_int8_t	*to;
};

struct aml_powerres {
	enum	aml_objtype type;
	int	level;
	int	order;
};

struct	aml_processor {
	enum	aml_objtype type;
	int	id;
	int	addr;
	int	len;
};

struct	aml_mutex_queue {
	STAILQ_ENTRY(aml_mutex_queue) entry;
};

struct	aml_mutex {
	enum	aml_objtype type;
	int	level;
	volatile	void *cookie;	/* In kernel, struct proc? */
	STAILQ_HEAD(, aml_mutex_queue) queue;
};

struct	aml_objref {
	enum	aml_objtype type;
	struct	aml_name *nameref;
	union	aml_object *ref;
	int	offset;		/* of aml_buffer.data or aml_package.objects. */
	/* if negative value, not ready to dereference for element access. */
	unsigned	deref;	/* indicates whether dereffenced or not */
	unsigned	alias;	/* true if this is an alias object reference */
};

struct	aml_regfield {
	enum	aml_objtype type;
	int	space;
	u_int32_t	flags;
	int	offset;
	int	bitoffset;
	int	bitlen;
};

struct	aml_event {
	enum	aml_objtype type;	/* aml_t_event */
	int	inuse;
};

union	aml_object {
	enum	aml_objtype type;
	struct	aml_num num;
	struct	aml_processor proc;
	struct	aml_powerres pres;
	struct	aml_opregion opregion;
	struct	aml_method meth;
	struct	aml_field field;
	struct	aml_mutex mutex;
	struct	aml_namestr nstr;
	struct	aml_buffer buffer;
	struct	aml_bufferfield bfld;
	struct	aml_package package;
	struct	aml_string str;
	struct	aml_objref objref;
	struct	aml_event event;
	struct	aml_regfield regfield;
};

union aml_object	*aml_copy_object(struct aml_environ *,
					  union aml_object *);
union aml_object	*aml_alloc_object(enum aml_objtype,
					   union aml_object *);
void			 aml_free_objectcontent(union aml_object *);
void			 aml_free_object(union aml_object **);
void			 aml_realloc_object(union aml_object *, int);

#endif /* !_AML_OBJ_H_ */
