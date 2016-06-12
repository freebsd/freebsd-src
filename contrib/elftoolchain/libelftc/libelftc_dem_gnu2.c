/*-
 * Copyright (c) 2008 Hyogeol Lee <hyogeollee@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <libelftc.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "_libelftc.h"

ELFTC_VCSID("$Id: libelftc_dem_gnu2.c 3447 2016-05-03 13:32:23Z emaste $");

/**
 * @file cpp_demangle_gnu2.c
 * @brief Decode function name encoding in GNU 2.
 *
 * Function name encoding in GNU 2 based on ARM style.
 */

enum encode_type {
	ENCODE_FUNC, ENCODE_OP, ENCODE_OP_CT, ENCODE_OP_DT, ENCODE_OP_USER,
	ENCODE_OP_TF, ENCODE_OP_TI, ENCODE_OP_VT
};

struct cstring {
	char	*buf;
	size_t	size;
};

struct demangle_data {
	bool	ptr, ref, cnst, array, cnst_fn, class_name;
	struct cstring array_str;
	const char *p;
	enum encode_type type;
	struct vector_str vec;
	struct vector_str arg;
};

#define SIMPLE_HASH(x,y)	(64 * x + y)
#define	CPP_DEMANGLE_GNU2_TRY	128

static void	dest_cstring(struct cstring *);
static void	dest_demangle_data(struct demangle_data *);
static bool	init_cstring(struct cstring *, size_t);
static bool	init_demangle_data(struct demangle_data *);
static bool	push_CTDT(const char *, size_t, struct vector_str *);
static bool	read_array(struct demangle_data *);
static bool	read_class(struct demangle_data *);
static bool	read_func(struct demangle_data *);
static bool	read_func_name(struct demangle_data *);
static bool	read_func_ptr(struct demangle_data *);
static bool	read_memptr(struct demangle_data *);
static bool	read_op(struct demangle_data *);
static bool	read_op_user(struct demangle_data *);
static bool	read_qual_name(struct demangle_data *);
static int	read_subst(struct demangle_data *);
static int	read_subst_iter(struct demangle_data *);
static bool	read_type(struct demangle_data *);

/**
 * @brief Decode the input string by the GNU 2 style.
 *
 * @return New allocated demangled string or NULL if failed.
 */
char *
cpp_demangle_gnu2(const char *org)
{
	struct demangle_data d;
	size_t arg_begin, arg_len;
	unsigned int try;
	char *rtn, *arg;

	if (org == NULL)
		return (NULL);

	if (init_demangle_data(&d) == false)
		return (NULL);

	try = 0;
	rtn = NULL;

	d.p = org;
	if (read_func_name(&d) == false)
		goto clean;

	switch (d.type) {
	case ENCODE_FUNC :
	case ENCODE_OP :
		break;

	case ENCODE_OP_CT :
		if (push_CTDT("::", 2, &d.vec) == false)
			goto clean;

                break;
	case ENCODE_OP_DT :
		if (push_CTDT("::~", 3, &d.vec) == false)
			goto clean;

		if (vector_str_push(&d.vec, "(void)", 6) == false)
			goto clean;

		goto flat;
	case ENCODE_OP_USER :
	case ENCODE_OP_TF :
	case ENCODE_OP_TI :
	case ENCODE_OP_VT :
		goto flat;
	}

	if (*d.p == 'F')
		++d.p;
	else if (*d.p == '\0') {
		if (d.class_name == true) {
			if (vector_str_push(&d.vec, "(void)", 6) == false)
				goto clean;

			goto flat;
		} else
			goto clean;
	}

	/* start argument types */
	if (vector_str_push(&d.vec, "(", 1) == false)
		goto clean;

	for (;;) {
		if (*d.p == 'T') {
			const int rtn_subst = read_subst(&d);

			if (rtn_subst == -1)
				goto clean;
			else if (rtn_subst == 1)
				break;

			continue;
		}

		if (*d.p == 'N') {
			const int rtn_subst_iter = read_subst_iter(&d);

			if (rtn_subst_iter == -1)
				goto clean;
			else if(rtn_subst_iter == 1)
				break;

			continue;
		}

		arg_begin = d.vec.size;

		if (read_type(&d) == false)
			goto clean;

		if (d.ptr == true) {
			if (vector_str_push(&d.vec, "*", 1) == false)
				goto clean;

			d.ptr = false;
		}

		if (d.ref == true) {
			if (vector_str_push(&d.vec, "&", 1) == false)
				goto clean;

			d.ref = false;
		}

		if (d.cnst == true) {
			if (vector_str_push(&d.vec, " const", 6) == false)
				goto clean;

			d.cnst = false;
		}

		if (d.array == true) {
			if (vector_str_push(&d.vec, d.array_str.buf,
				d.array_str.size) == false)
				goto clean;

			dest_cstring(&d.array_str);
			d.array = false;
		}

		if (*d.p == '\0')
			break;

		if ((arg = vector_str_substr(&d.vec, arg_begin, d.vec.size - 1,
			    &arg_len)) == NULL)
			goto clean;

		if (vector_str_push(&d.arg, arg, arg_len) == false)
			goto clean;

		free(arg);

		if (vector_str_push(&d.vec, ", ", 2) == false)
			goto clean;

		if (++try > CPP_DEMANGLE_GNU2_TRY)
			goto clean;
	}

	/* end argument types */
	if (vector_str_push(&d.vec, ")", 1) == false)
		goto clean;
flat:
	if (d.cnst_fn == true && vector_str_push(&d.vec, " const", 6) == false)
		goto clean;

	rtn = vector_str_get_flat(&d.vec, NULL);
clean:
	dest_demangle_data(&d);

	return (rtn);
}

/**
 * @brief Test input string is encoded by the GNU 2 style.
 *
 * @return True if input string is encoded by the GNU 2 style.
 */
bool
is_cpp_mangled_gnu2(const char *org)
{
	char *str;
	bool rtn = false;

	if (org == NULL)
		return (false);

	/* search valid text to end */
	str = strstr(org, "__");
	while (str != NULL) {
		if (*(str + 2) != '\0') {
			if (*(str + 2) == 'C' ||
			    *(str + 2) == 'F' ||
			    *(str + 2) == 'Q' ||
			    ELFTC_ISDIGIT(*(str + 2))) {
				rtn |= true;
				
				break;
			}
			
			if (*(str + 3) != '\0') {
				switch (SIMPLE_HASH(*(str + 2), *(str + 3))) {
				case SIMPLE_HASH('m', 'l') :
				case SIMPLE_HASH('d', 'v') :
				case SIMPLE_HASH('m', 'd') :
				case SIMPLE_HASH('p', 'l') :
				case SIMPLE_HASH('m', 'i') :
				case SIMPLE_HASH('l', 's') :
				case SIMPLE_HASH('r', 's') :
				case SIMPLE_HASH('e', 'q') :
				case SIMPLE_HASH('n', 'e') :
				case SIMPLE_HASH('l', 't') :
				case SIMPLE_HASH('g', 't') :
				case SIMPLE_HASH('l', 'e') :
				case SIMPLE_HASH('g', 'e') :
				case SIMPLE_HASH('a', 'd') :
				case SIMPLE_HASH('o', 'r') :
				case SIMPLE_HASH('e', 'r') :
				case SIMPLE_HASH('a', 'a') :
				case SIMPLE_HASH('o', 'o') :
				case SIMPLE_HASH('n', 't') :
				case SIMPLE_HASH('c', 'o') :
				case SIMPLE_HASH('p', 'p') :
				case SIMPLE_HASH('m', 'm') :
				case SIMPLE_HASH('a', 's') :
				case SIMPLE_HASH('r', 'f') :
				case SIMPLE_HASH('a', 'p') :
				case SIMPLE_HASH('a', 'm') :
				case SIMPLE_HASH('a', 'l') :
				case SIMPLE_HASH('a', 'r') :
				case SIMPLE_HASH('a', 'o') :
				case SIMPLE_HASH('a', 'e') :
				case SIMPLE_HASH('c', 'm') :
				case SIMPLE_HASH('r', 'm') :
				case SIMPLE_HASH('c', 'l') :
				case SIMPLE_HASH('v', 'c') :
				case SIMPLE_HASH('n', 'w') :
				case SIMPLE_HASH('d', 'l') :
				case SIMPLE_HASH('o', 'p') :
				case SIMPLE_HASH('t', 'f') :
				case SIMPLE_HASH('t', 'i') :
					rtn |= true;

					break;
				}
			}
		}

		str = strstr(str + 2, "__");
	}

	rtn |= strstr(org, "_$_") != NULL;
	rtn |= strstr(org, "_vt$") != NULL;

	return (rtn);
}

static void
dest_cstring(struct cstring *s)
{

	if (s == NULL)
		return;

	free(s->buf);
	s->buf = NULL;
	s->size = 0;
}

static void
dest_demangle_data(struct demangle_data *d)
{

	if (d != NULL) {
		vector_str_dest(&d->arg);
		vector_str_dest(&d->vec);

		dest_cstring(&d->array_str);
	}
}

static bool
init_cstring(struct cstring *s, size_t len)
{

	if (s == NULL || len <= 1)
		return (false);

	if ((s->buf = malloc(sizeof(char) * len)) == NULL)
		return (false);

	s->size = len - 1;

	return (true);
}

static bool
init_demangle_data(struct demangle_data *d)
{

	if (d == NULL)
		return (false);

	d->ptr = false;
	d->ref = false;
	d->cnst = false;
	d->array = false;
	d->cnst_fn = false;
	d->class_name = false;

	d->array_str.buf = NULL;
	d->array_str.size = 0;

	d->type = ENCODE_FUNC;

	if (vector_str_init(&d->vec) == false)
		return (false);

	if (vector_str_init(&d->arg) == false) {
		vector_str_dest(&d->vec);

		return (false);
	}

	return (true);
}

static bool
push_CTDT(const char *s, size_t l, struct vector_str *v)
{

	if (s == NULL || l == 0 || v == NULL)
		return (false);

	if (vector_str_push(v, s, l) == false)
		return (false);

	assert(v->size > 1);

	return (vector_str_push(v, v->container[v->size - 2],
		strlen(v->container[v->size - 2])));
}

static bool
read_array(struct demangle_data *d)
{
	size_t len;
	const char *end;

	if (d == NULL || d->p == NULL)
		return (false);

	end = d->p;
	assert(end != NULL);

	for (;;) {
		if (*end == '\0')
			return (false);

		if (ELFTC_ISDIGIT(*end) == 0)
			break;

		++end;
	}

	if (*end != '_')
		return (false);

	len = end - d->p;
	assert(len > 0);

	dest_cstring(&d->array_str);
	if (init_cstring(&d->array_str, len + 3) == false)
		return (false);

	strncpy(d->array_str.buf + 1, d->p, len);
	*d->array_str.buf = '[';
	*(d->array_str.buf + len + 1) = ']';

	d->array = true;
	d->p = end + 1;

	return (true);
}

static bool
read_class(struct demangle_data *d)
{
	size_t len;
	char *str;

	if (d == NULL)
		return (false);

	len = strtol(d->p, &str, 10);
	if (len == 0 && (errno == EINVAL || errno == ERANGE))
		return (false);

	assert(len > 0);
	assert(str != NULL);

	if (vector_str_push(&d->vec, str, len) == false)
		return (false);

	d->p = str + len;

	d->class_name = true;

	return (true);
}

static bool
read_func(struct demangle_data *d)
{
	size_t len;
	const char *name;
	char *delim;

	if (d == NULL)
		return (false);

	assert(d->p != NULL && "d->p (org str) is NULL");
	if ((delim = strstr(d->p, "__")) == NULL)
		return (false);

	len = delim - d->p;
	assert(len != 0);

	name = d->p;

	d->p = delim + 2;

	if (*d->p == 'C') {
		++d->p;

		d->cnst_fn = true;
	}

	if (*d->p == 'Q' && ELFTC_ISDIGIT(*(d->p + 1))) {
		++d->p;

		if (read_qual_name(d) == false)
			return (false);
	} else if (ELFTC_ISDIGIT(*d->p)) {
		if (read_class(d) == false)
			return (false);

		if (vector_str_push(&d->vec, "::", 2) == false)
			return (false);
	}

	return (vector_str_push(&d->vec, name, len));
}

static bool
read_func_name(struct demangle_data *d)
{
	size_t len;
	bool rtn;
	char *op_name;

	if (d == NULL)
		return (false);

	rtn = false;
	op_name = NULL;

	assert(d->p != NULL && "d->p (org str) is NULL");

	if (*d->p == '_' && *(d->p + 1) == '_') {
		d->p += 2;

		/* CTOR */
		if (*d->p == 'Q' && ELFTC_ISDIGIT(*(d->p + 1))) {
			++d->p;
			d->type = ENCODE_OP_CT;

			if (read_qual_name(d) == false)
				return (false);

			return (vector_str_pop(&d->vec));
		} else if (ELFTC_ISDIGIT(*d->p)) {
			d->type = ENCODE_OP_CT;

			return (read_class(d));
		}

		d->type = ENCODE_OP;
		if (read_op(d) == false) {
			/* not good condition, start function name with '__' */
			d->type = ENCODE_FUNC;

			if (vector_str_push(&d->vec, "__", 2) == false)
				return (false);
			
			return (read_func(d));
		}

		if (d->type == ENCODE_OP_USER ||
		    d->type == ENCODE_OP_TF ||
		    d->type == ENCODE_OP_TI)
			return (true);

		/* skip "__" */
		d->p += 2;

		if (*d->p == 'C') {
			++d->p;

			d->cnst_fn = true;
		}

		/* assume delimiter is removed */
		if (*d->p == 'Q' && ELFTC_ISDIGIT(*(d->p + 1))) {
			++d->p;

			assert(d->vec.size > 0);

			len = strlen(d->vec.container[d->vec.size - 1]);
			if ((op_name = malloc(sizeof(char) * (len + 1)))
			    == NULL)
				return (false);

			snprintf(op_name, len + 1, "%s",
			    d->vec.container[d->vec.size - 1]);
			vector_str_pop(&d->vec);

			if (read_qual_name(d) == false)
				goto clean;

			if (vector_str_push(&d->vec, "::", 2) == false)
				goto clean;

			if (vector_str_push(&d->vec, op_name, len) == false)
				goto clean;

			rtn = true;
		} else if (ELFTC_ISDIGIT(*d->p)) {
			assert(d->vec.size > 0);

			len = strlen(d->vec.container[d->vec.size - 1]);
			if ((op_name = malloc(sizeof(char) * (len + 1)))
			    == NULL)
				return (false);

			snprintf(op_name, len + 1, "%s",
			    d->vec.container[d->vec.size - 1]);
			vector_str_pop(&d->vec);

			if (read_class(d) == false)
				goto clean;

			if (vector_str_push(&d->vec, "::", 2) == false)
				goto clean;

			if (vector_str_push(&d->vec, op_name, len) == false)
				goto clean;

			rtn = true;
		}
	} else if (memcmp(d->p, "_$_", 3) == 0) {
		/* DTOR */
		d->p += 3;
		d->type = ENCODE_OP_DT;

		if (*d->p == 'Q' && ELFTC_ISDIGIT(*(d->p + 1))) {
			++d->p;

			if (read_qual_name(d) == false)
				return (false);

			return (vector_str_pop(&d->vec));
		} else if (ELFTC_ISDIGIT(*d->p))
			return (read_class(d));

		return (false);
	} else if (memcmp(d->p, "_vt$", 4) == 0) {
		/* vtable */
		d->p += 4;
		d->type = ENCODE_OP_VT;

		if (*d->p == 'Q' && ELFTC_ISDIGIT(*(d->p + 1))) {
			++d->p;

			if (read_qual_name(d) == false)
				return (false);

			if (vector_str_pop(&d->vec) == false)
				return (false);
		} else if (ELFTC_ISDIGIT(*d->p)) {
			if (read_class(d) == false)
				return (false);
		}

		return (vector_str_push(&d->vec, " virtual table", 14));
	} else
		return (read_func(d));
clean:
	free(op_name);

	return (rtn);
}

/* Read function ptr type */
static bool
read_func_ptr(struct demangle_data *d)
{
	struct demangle_data fptr;
	size_t arg_len, rtn_len;
	char *arg_type, *rtn_type;
	int lim;

	if (d == NULL)
		return (false);

	if (init_demangle_data(&fptr) == false)
		return (false);

	fptr.p = d->p + 1;
	lim = 0;
	arg_type = NULL;
	rtn_type = NULL;

	for (;;) {
		if (read_type(&fptr) == false) {
			dest_demangle_data(&fptr);

			return (false);
		}

		if (fptr.ptr == true) {
			if (vector_str_push(&fptr.vec, "*", 1) == false) {
				dest_demangle_data(&fptr);

				return (false);
			}

			fptr.ptr = false;
		}

		if (fptr.ref == true) {
			if (vector_str_push(&fptr.vec, "&", 1) == false) {
				dest_demangle_data(&fptr);

				return (false);
			}

			fptr.ref = false;
		}

		if (fptr.cnst == true) {
			if (vector_str_push(&fptr.vec, " const", 6) == false) {
				dest_demangle_data(&fptr);

				return (false);
			}

			fptr.cnst = false;
		}

		if (*fptr.p == '_')
			break;

		if (vector_str_push(&fptr.vec, ", ", 2) == false) {
			dest_demangle_data(&fptr);

			return (false);
		}

		if (++lim > CPP_DEMANGLE_GNU2_TRY) {

			dest_demangle_data(&fptr);

			return (false);
		}
	}

	arg_type = vector_str_get_flat(&fptr.vec, &arg_len);
	/* skip '_' */
	d->p = fptr.p + 1;

	dest_demangle_data(&fptr);

	if (init_demangle_data(&fptr) == false) {
		free(arg_type);

		return (false);
	}

	fptr.p = d->p;
	lim = 0;

	if (read_type(&fptr) == false) {
		free(arg_type);
		dest_demangle_data(&fptr);

		return (false);
	}

	rtn_type = vector_str_get_flat(&fptr.vec, &rtn_len);
	d->p = fptr.p;


	dest_demangle_data(&fptr);

	if (vector_str_push(&d->vec, rtn_type, rtn_len) == false) {
		free(rtn_type);
		free(arg_type);

		return (false);
	}

	free(rtn_type);

	if (vector_str_push(&d->vec, " (*)(", 5) == false) {
		free(arg_type);

		return (false);
	}

	if (vector_str_push(&d->vec, arg_type, arg_len) == false) {
		free(arg_type);

		return (false);
	}

	free(arg_type);

	return (vector_str_push(&d->vec, ")", 1));
}

static bool
read_memptr(struct demangle_data *d)
{
	struct demangle_data mptr;
	size_t len;
	bool rtn;
	char *mptr_str;

	if (d == NULL || d->p == NULL)
		return (false);

	if (init_demangle_data(&mptr) == false)
		return (false);

	rtn = false;
	mptr_str = NULL;

	mptr.p = d->p;
	if (*mptr.p == 'Q') {
		++mptr.p;

		if (read_qual_name(&mptr) == false)
			goto clean;
	} else if (read_class(&mptr) == false)
			goto clean;

	d->p = mptr.p;

	if ((mptr_str = vector_str_get_flat(&mptr.vec, &len)) == NULL)
		goto clean;

	if (vector_str_push(&d->vec, mptr_str, len) == false)
		goto clean;

	if (vector_str_push(&d->vec, "::*", 3) == false)
		goto clean;

	rtn = true;
clean:
	free(mptr_str);
	dest_demangle_data(&mptr);

	return (rtn);
}

static bool
read_op(struct demangle_data *d)
{

	if (d == NULL)
		return (false);

	assert(d->p != NULL && "d->p (org str) is NULL");

	switch (SIMPLE_HASH(*(d->p), *(d->p+1))) {
	case SIMPLE_HASH('m', 'l') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator*", 9));
	case SIMPLE_HASH('d', 'v') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator/", 9));
	case SIMPLE_HASH('m', 'd') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator%", 9));
	case SIMPLE_HASH('p', 'l') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator+", 9));
	case SIMPLE_HASH('m', 'i') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator-", 9));
	case SIMPLE_HASH('l', 's') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator<<", 10));
	case SIMPLE_HASH('r', 's') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator>>", 10));
	case SIMPLE_HASH('e', 'q') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator==", 10));
	case SIMPLE_HASH('n', 'e') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator!=", 10));
	case SIMPLE_HASH('l', 't') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator<", 9));
	case SIMPLE_HASH('g', 't') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator>", 9));
	case SIMPLE_HASH('l', 'e') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator<=", 10));
	case SIMPLE_HASH('g', 'e') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator>=", 10));
	case SIMPLE_HASH('a', 'd') :
		d->p += 2;
		if (*d->p == 'v') {
			++d->p;
			return (vector_str_push(&d->vec, "operator/=",
				10));
		} else
			return (vector_str_push(&d->vec, "operator&", 9));
	case SIMPLE_HASH('o', 'r') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator|", 9));
	case SIMPLE_HASH('e', 'r') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator^", 9));
	case SIMPLE_HASH('a', 'a') :
		d->p += 2;
		if (*d->p == 'd') {
			++d->p;
			return (vector_str_push(&d->vec, "operator&=",
				10));
		} else
			return (vector_str_push(&d->vec, "operator&&",
				10));
	case SIMPLE_HASH('o', 'o') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator||", 10));
	case SIMPLE_HASH('n', 't') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator!", 9));
	case SIMPLE_HASH('c', 'o') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator~", 9));
	case SIMPLE_HASH('p', 'p') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator++", 10));
	case SIMPLE_HASH('m', 'm') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator--", 10));
	case SIMPLE_HASH('a', 's') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator=", 9));
	case SIMPLE_HASH('r', 'f') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator->", 10));
	case SIMPLE_HASH('a', 'p') :
		/* apl */
		if (*(d->p + 2) != 'l')
			return (false);

		d->p += 3;
		return (vector_str_push(&d->vec, "operator+=", 10));
	case SIMPLE_HASH('a', 'm') :
		d->p += 2;
		if (*d->p == 'i') {
			++d->p;
			return (vector_str_push(&d->vec, "operator-=",
				10));
		} else if (*d->p == 'u') {
			++d->p;
			return (vector_str_push(&d->vec, "operator*=",
				10));
		} else if (*d->p == 'd') {
			++d->p;
			return (vector_str_push(&d->vec, "operator%=",
				10));
		}

		return (false);
	case SIMPLE_HASH('a', 'l') :
		/* als */
		if (*(d->p + 2) != 's')
			return (false);

		d->p += 3;
		return (vector_str_push(&d->vec, "operator<<=", 11));
	case SIMPLE_HASH('a', 'r') :
		/* ars */
		if (*(d->p + 2) != 's')
			return (false);

		d->p += 3;
		return (vector_str_push(&d->vec, "operator>>=", 11));
	case SIMPLE_HASH('a', 'o') :
		/* aor */
		if (*(d->p + 2) != 'r')
			return (false);

		d->p += 3;
		return (vector_str_push(&d->vec, "operator|=", 10));
	case SIMPLE_HASH('a', 'e') :
		/* aer */
		if (*(d->p + 2) != 'r')
			return (false);

		d->p += 3;
		return (vector_str_push(&d->vec, "operator^=", 10));
	case SIMPLE_HASH('c', 'm') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator,", 9));
	case SIMPLE_HASH('r', 'm') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator->*", 11));
	case SIMPLE_HASH('c', 'l') :
		d->p += 2;
		return (vector_str_push(&d->vec, "()", 2));
	case SIMPLE_HASH('v', 'c') :
		d->p += 2;
		return (vector_str_push(&d->vec, "[]", 2));
	case SIMPLE_HASH('n', 'w') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator new()", 14));
	case SIMPLE_HASH('d', 'l') :
		d->p += 2;
		return (vector_str_push(&d->vec, "operator delete()",
			17));
	case SIMPLE_HASH('o', 'p') :
		/* __op<TO_TYPE>__<FROM_TYPE> */
		d->p += 2;

		d->type = ENCODE_OP_USER;

		return (read_op_user(d));
	case SIMPLE_HASH('t', 'f') :
		d->p += 2;
		d->type = ENCODE_OP_TF;

		if (read_type(d) == false)
			return (false);

		return (vector_str_push(&d->vec, " type_info function", 19));
	case SIMPLE_HASH('t', 'i') :
		d->p += 2;
		d->type = ENCODE_OP_TI;

		if (read_type(d) == false)
			return (false);

		return (vector_str_push(&d->vec, " type_info node", 15));
	default :
		return (false);
	};
}

static bool
read_op_user(struct demangle_data *d)
{
	struct demangle_data from, to;
	size_t from_len, to_len;
	bool rtn;
	char *from_str, *to_str;

	if (d == NULL)
		return (false);

	if (init_demangle_data(&from) == false)
		return (false);

	rtn = false;
	from_str = NULL;
	to_str = NULL;
	if (init_demangle_data(&to) == false)
		goto clean;

	to.p = d->p;
	if (*to.p == 'Q') {
		++to.p;

		if (read_qual_name(&to) == false)
			goto clean;

		/* pop last '::' */
		if (vector_str_pop(&to.vec) == false)
			goto clean;
	} else {
		if (read_class(&to) == false)
			goto clean;

		/* skip '__' */
		to.p += 2;
	}

	if ((to_str = vector_str_get_flat(&to.vec, &to_len)) == NULL)
		goto clean;

	from.p = to.p;
	if (*from.p == 'Q') {
		++from.p;

		if (read_qual_name(&from) == false)
			goto clean;

		/* pop last '::' */
		if (vector_str_pop(&from.vec) == false)
			goto clean;
	} else if (read_class(&from) == false)
			goto clean;

	if ((from_str = vector_str_get_flat(&from.vec, &from_len)) == NULL)
		goto clean;

	if (vector_str_push(&d->vec, from_str, from_len) == false)
		goto clean;

	if (vector_str_push(&d->vec, "::operator ", 11) == false)
		goto clean;

	if (vector_str_push(&d->vec, to_str, to_len) == false)
		goto clean;

	rtn = vector_str_push(&d->vec, "()", 2);
clean:
	free(to_str);
	free(from_str);
	dest_demangle_data(&to);
	dest_demangle_data(&from);

	return (rtn);
}

/* single digit + class names */
static bool
read_qual_name(struct demangle_data *d)
{
	int i;
	char num;

	if (d == NULL)
		return (false);

	assert(d->p != NULL && "d->p (org str) is NULL");
	assert(*d->p > 48 && *d->p < 58 && "*d->p not in ASCII numeric range");

	num = *d->p - 48;

	assert(num > 0);

	++d->p;
	for (i = 0; i < num ; ++i) {
		if (read_class(d) == false)
			return (false);

		if (vector_str_push(&d->vec, "::", 2) == false)
			return (false);
	}

	if (*d->p != '\0')
		d->p = d->p + 2;

	return (true);
}

/* Return -1 at fail, 0 at success, and 1 at end */
static int
read_subst(struct demangle_data *d)
{
	size_t idx;
	char *str;

	if (d == NULL)
		return (-1);

	idx = strtol(d->p + 1, &str, 10);
	if (idx == 0 && (errno == EINVAL || errno == ERANGE))
		return (-1);

	assert(idx > 0);
	assert(str != NULL);

	d->p = str;

	if (vector_str_push(&d->vec, d->arg.container[idx - 1],
		strlen(d->arg.container[idx - 1])) == false)
		return (-1);

	if (vector_str_push(&d->arg, d->arg.container[idx - 1],
		strlen(d->arg.container[idx - 1])) == false)
		return (-1);

	if (*d->p == '\0')
		return (1);

	return (0);
}

static int
read_subst_iter(struct demangle_data *d)
{
	int i;
	size_t idx;
	char repeat;
	char *str;

	if (d == NULL)
		return (-1);

	++d->p;
	assert(*d->p > 48 && *d->p < 58 && "*d->p not in ASCII numeric range");

	repeat = *d->p - 48;

	assert(repeat > 1);

	++d->p;

	idx = strtol(d->p, &str, 10);
	if (idx == 0 && (errno == EINVAL || errno == ERANGE))
		return (-1);

	assert(idx > 0);
	assert(str != NULL);

	d->p = str;

	for (i = 0; i < repeat ; ++i) {
		if (vector_str_push(&d->vec, d->arg.container[idx - 1],
			strlen(d->arg.container[idx - 1])) == false)
			return (-1);

		if (vector_str_push(&d->arg, d->arg.container[idx - 1],
			strlen(d->arg.container[idx - 1])) == false)
			return (-1);

		if (i != repeat - 1 &&
		    vector_str_push(&d->vec, ", ", 2) == false)
			return (-1);
	}

	if (*d->p == '\0')
		return (1);

	return (0);
}

static bool
read_type(struct demangle_data *d)
{

	if (d == NULL)
		return (false);

	assert(d->p != NULL && "d->p (org str) is NULL");

	while (*d->p == 'U' || *d->p == 'C' || *d->p == 'V' || *d->p == 'S' ||
	       *d->p == 'P' || *d->p == 'R' || *d->p == 'A' || *d->p == 'F' ||
	       *d->p == 'M') {
		switch (*d->p) {
		case 'U' :
			++d->p;

			if (vector_str_push(&d->vec, "unsigned ", 9) == false)
				return (false);

			break;
		case 'C' :
			++d->p;

			if (*d->p == 'P')
				d->cnst = true;
			else {
				if (vector_str_push(&d->vec, "const ", 6) ==
				    false)
					return (false);
			}

			break;
		case 'V' :
			++d->p;

			if (vector_str_push(&d->vec, "volatile ", 9) == false)
				return (false);

			break;
		case 'S' :
			++d->p;

			if (vector_str_push(&d->vec, "signed ", 7) == false)
				return (false);

			break;
		case 'P' :
			++d->p;

			if (*d->p == 'F')
				return (read_func_ptr(d));
			else
				d->ptr = true;

			break;
		case 'R' :
			++d->p;

			d->ref = true;

			break;
		case 'F' :
			break;
		case 'A' :
			++d->p;

			if (read_array(d) == false)
				return (false);

			break;
		case 'M' :
			++d->p;

			if (read_memptr(d) == false)
				return (false);

			break;
		default :
			break;
		}
	}

	if (ELFTC_ISDIGIT(*d->p))
		return (read_class(d));

	switch (*d->p) {
	case 'Q' :
		++d->p;

		return (read_qual_name(d));
	case 'v' :
		++d->p;

		return (vector_str_push(&d->vec, "void", 4));
	case 'b':
		++d->p;

		return(vector_str_push(&d->vec, "bool", 4));
	case 'c' :
		++d->p;

		return (vector_str_push(&d->vec, "char", 4));
	case 's' :
		++d->p;

		return (vector_str_push(&d->vec, "short", 5));
	case 'i' :
		++d->p;

		return (vector_str_push(&d->vec, "int", 3));
	case 'l' :
		++d->p;

		return (vector_str_push(&d->vec, "long", 4));
	case 'f' :
		++d->p;

		return (vector_str_push(&d->vec, "float", 5));
	case 'd':
		++d->p;

		return (vector_str_push(&d->vec, "double", 6));
	case 'r':
		++d->p;

		return (vector_str_push(&d->vec, "long double", 11));
	case 'e':
		++d->p;

		return (vector_str_push(&d->vec, "...", 3));
	case 'w':
		++d->p;

		return (vector_str_push(&d->vec, "wchar_t", 7));
	case 'x':
		++d->p;

		return (vector_str_push(&d->vec, "long long", 9));
	default:
		return (false);
	};

	/* NOTREACHED */
	return (false);
}
