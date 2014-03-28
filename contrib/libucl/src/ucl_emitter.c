/* Copyright (c) 2013, Vsevolod Stakhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *       * Redistributions of source code must retain the above copyright
 *         notice, this list of conditions and the following disclaimer.
 *       * Redistributions in binary form must reproduce the above copyright
 *         notice, this list of conditions and the following disclaimer in the
 *         documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ucl.h"
#include "ucl_internal.h"
#include "ucl_chartable.h"
#ifdef HAVE_FLOAT_H
#include <float.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif

/**
 * @file rcl_emitter.c
 * Serialise UCL object to various of output formats
 */


static void ucl_obj_write_json (ucl_object_t *obj,
		struct ucl_emitter_functions *func,
		unsigned int tabs,
		bool start_tabs,
		bool compact);
static void ucl_elt_write_json (ucl_object_t *obj,
		struct ucl_emitter_functions *func,
		unsigned int tabs,
		bool start_tabs,
		bool compact);
static void ucl_elt_write_config (ucl_object_t *obj,
		struct ucl_emitter_functions *func,
		unsigned int tabs,
		bool start_tabs,
		bool is_top,
		bool expand_array);
static void ucl_elt_write_yaml (ucl_object_t *obj,
		struct ucl_emitter_functions *func,
		unsigned int tabs,
		bool start_tabs,
		bool compact,
		bool expand_array);
static void ucl_elt_array_write_yaml (ucl_object_t *obj,
		struct ucl_emitter_functions *func,
		unsigned int tabs,
		bool start_tabs,
		bool is_top);

/**
 * Add tabulation to the output buffer
 * @param buf target buffer
 * @param tabs number of tabs to add
 */
static inline void
ucl_add_tabs (struct ucl_emitter_functions *func, unsigned int tabs, bool compact)
{
	if (!compact) {
		func->ucl_emitter_append_character (' ', tabs * 4, func->ud);
	}
}

/**
 * Serialise string
 * @param str string to emit
 * @param buf target buffer
 */
static void
ucl_elt_string_write_json (const char *str, size_t size,
		struct ucl_emitter_functions *func)
{
	const char *p = str, *c = str;
	size_t len = 0;

	func->ucl_emitter_append_character ('"', 1, func->ud);
	while (size) {
		if (ucl_test_character (*p, UCL_CHARACTER_JSON_UNSAFE)) {
			if (len > 0) {
				func->ucl_emitter_append_len (c, len, func->ud);
			}
			switch (*p) {
			case '\n':
				func->ucl_emitter_append_len ("\\n", 2, func->ud);
				break;
			case '\r':
				func->ucl_emitter_append_len ("\\r", 2, func->ud);
				break;
			case '\b':
				func->ucl_emitter_append_len ("\\b", 2, func->ud);
				break;
			case '\t':
				func->ucl_emitter_append_len ("\\t", 2, func->ud);
				break;
			case '\f':
				func->ucl_emitter_append_len ("\\f", 2, func->ud);
				break;
			case '\\':
				func->ucl_emitter_append_len ("\\\\", 2, func->ud);
				break;
			case '"':
				func->ucl_emitter_append_len ("\\\"", 2, func->ud);
				break;
			}
			len = 0;
			c = ++p;
		}
		else {
			p ++;
			len ++;
		}
		size --;
	}
	if (len > 0) {
		func->ucl_emitter_append_len (c, len, func->ud);
	}
	func->ucl_emitter_append_character ('"', 1, func->ud);
}

/**
 * Write a single object to the buffer
 * @param obj object to write
 * @param buf target buffer
 */
static void
ucl_elt_obj_write_json (ucl_object_t *obj, struct ucl_emitter_functions *func,
		unsigned int tabs, bool start_tabs, bool compact)
{
	ucl_object_t *cur;
	ucl_hash_iter_t it = NULL;

	if (start_tabs) {
		ucl_add_tabs (func, tabs, compact);
	}
	if (compact) {
		func->ucl_emitter_append_character ('{', 1, func->ud);
	}
	else {
		func->ucl_emitter_append_len ("{\n", 2, func->ud);
	}
	while ((cur = ucl_hash_iterate (obj->value.ov, &it))) {
		ucl_add_tabs (func, tabs + 1, compact);
		if (cur->keylen > 0) {
			ucl_elt_string_write_json (cur->key, cur->keylen, func);
		}
		else {
			func->ucl_emitter_append_len ("null", 4, func->ud);
		}
		if (compact) {
			func->ucl_emitter_append_character (':', 1, func->ud);
		}
		else {
			func->ucl_emitter_append_len (": ", 2, func->ud);
		}
		ucl_obj_write_json (cur, func, tabs + 1, false, compact);
		if (ucl_hash_iter_has_next (it)) {
			if (compact) {
				func->ucl_emitter_append_character (',', 1, func->ud);
			}
			else {
				func->ucl_emitter_append_len (",\n", 2, func->ud);
			}
		}
		else if (!compact) {
			func->ucl_emitter_append_character ('\n', 1, func->ud);
		}
	}
	ucl_add_tabs (func, tabs, compact);
	func->ucl_emitter_append_character ('}', 1, func->ud);
}

/**
 * Write a single array to the buffer
 * @param obj array to write
 * @param buf target buffer
 */
static void
ucl_elt_array_write_json (ucl_object_t *obj, struct ucl_emitter_functions *func,
		unsigned int tabs, bool start_tabs, bool compact)
{
	ucl_object_t *cur = obj;

	if (start_tabs) {
		ucl_add_tabs (func, tabs, compact);
	}
	if (compact) {
		func->ucl_emitter_append_character ('[', 1, func->ud);
	}
	else {
		func->ucl_emitter_append_len ("[\n", 2, func->ud);
	}
	while (cur) {
		ucl_elt_write_json (cur, func, tabs + 1, true, compact);
		if (cur->next != NULL) {
			if (compact) {
				func->ucl_emitter_append_character (',', 1, func->ud);
			}
			else {
				func->ucl_emitter_append_len (",\n", 2, func->ud);
			}
		}
		else if (!compact) {
			func->ucl_emitter_append_character ('\n', 1, func->ud);
		}
		cur = cur->next;
	}
	ucl_add_tabs (func, tabs, compact);
	func->ucl_emitter_append_character (']', 1, func->ud);
}

/**
 * Emit a single element
 * @param obj object
 * @param buf buffer
 */
static void
ucl_elt_write_json (ucl_object_t *obj, struct ucl_emitter_functions *func,
		unsigned int tabs, bool start_tabs, bool compact)
{
	bool flag;

	switch (obj->type) {
	case UCL_INT:
		if (start_tabs) {
			ucl_add_tabs (func, tabs, compact);
		}
		func->ucl_emitter_append_int (ucl_object_toint (obj), func->ud);
		break;
	case UCL_FLOAT:
	case UCL_TIME:
		if (start_tabs) {
			ucl_add_tabs (func, tabs, compact);
		}
		func->ucl_emitter_append_double (ucl_object_todouble (obj), func->ud);
		break;
	case UCL_BOOLEAN:
		if (start_tabs) {
			ucl_add_tabs (func, tabs, compact);
		}
		flag = ucl_object_toboolean (obj);
		if (flag) {
			func->ucl_emitter_append_len ("true", 4, func->ud);
		}
		else {
			func->ucl_emitter_append_len ("false", 5, func->ud);
		}
		break;
	case UCL_STRING:
		if (start_tabs) {
			ucl_add_tabs (func, tabs, compact);
		}
		ucl_elt_string_write_json (obj->value.sv, obj->len, func);
		break;
	case UCL_NULL:
		if (start_tabs) {
			ucl_add_tabs (func, tabs, compact);
		}
		func->ucl_emitter_append_len ("null", 4, func->ud);
		break;
	case UCL_OBJECT:
		ucl_elt_obj_write_json (obj, func, tabs, start_tabs, compact);
		break;
	case UCL_ARRAY:
		ucl_elt_array_write_json (obj->value.av, func, tabs, start_tabs, compact);
		break;
	case UCL_USERDATA:
		break;
	}
}

/**
 * Write a single object to the buffer
 * @param obj object
 * @param buf target buffer
 */
static void
ucl_obj_write_json (ucl_object_t *obj, struct ucl_emitter_functions *func,
		unsigned int tabs, bool start_tabs, bool compact)
{
	ucl_object_t *cur;
	bool is_array = (obj->next != NULL);

	if (is_array) {
		/* This is an array actually */
		if (start_tabs) {
			ucl_add_tabs (func, tabs, compact);
		}

		if (compact) {
			func->ucl_emitter_append_character ('[', 1, func->ud);
		}
		else {
			func->ucl_emitter_append_len ("[\n", 2, func->ud);
		}
		cur = obj;
		while (cur != NULL) {
			ucl_elt_write_json (cur, func, tabs + 1, true, compact);
			if (cur->next) {
				func->ucl_emitter_append_character (',', 1, func->ud);
			}
			if (!compact) {
				func->ucl_emitter_append_character ('\n', 1, func->ud);
			}
			cur = cur->next;
		}
		ucl_add_tabs (func, tabs, compact);
		func->ucl_emitter_append_character (']', 1, func->ud);
	}
	else {
		ucl_elt_write_json (obj, func, tabs, start_tabs, compact);
	}

}

/**
 * Emit an object to json
 * @param obj object
 * @return json output (should be freed after using)
 */
static void
ucl_object_emit_json (ucl_object_t *obj, bool compact, struct ucl_emitter_functions *func)
{
	ucl_obj_write_json (obj, func, 0, false, compact);
}

/**
 * Write a single object to the buffer
 * @param obj object to write
 * @param buf target buffer
 */
static void
ucl_elt_obj_write_config (ucl_object_t *obj, struct ucl_emitter_functions *func,
		unsigned int tabs, bool start_tabs, bool is_top)
{
	ucl_object_t *cur, *cur_obj;
	ucl_hash_iter_t it = NULL;

	if (start_tabs) {
		ucl_add_tabs (func, tabs, is_top);
	}
	if (!is_top) {
		func->ucl_emitter_append_len ("{\n", 2, func->ud);
	}

	while ((cur = ucl_hash_iterate (obj->value.ov, &it))) {
		LL_FOREACH (cur, cur_obj) {
			ucl_add_tabs (func, tabs + 1, is_top);
			if (cur_obj->flags & UCL_OBJECT_NEED_KEY_ESCAPE) {
				ucl_elt_string_write_json (cur_obj->key, cur_obj->keylen, func);
			}
			else {
				func->ucl_emitter_append_len (cur_obj->key, cur_obj->keylen, func->ud);
			}
			if (cur_obj->type != UCL_OBJECT && cur_obj->type != UCL_ARRAY) {
				func->ucl_emitter_append_len (" = ", 3, func->ud);
			}
			else {
				func->ucl_emitter_append_character (' ', 1, func->ud);
			}
			ucl_elt_write_config (cur_obj, func,
					is_top ? tabs : tabs + 1,
					false, false, false);
			if (cur_obj->type != UCL_OBJECT && cur_obj->type != UCL_ARRAY) {
				func->ucl_emitter_append_len (";\n", 2, func->ud);
			}
			else {
				func->ucl_emitter_append_character ('\n', 1, func->ud);
			}
		}
	}

	ucl_add_tabs (func, tabs, is_top);
	if (!is_top) {
		func->ucl_emitter_append_character ('}', 1, func->ud);
	}
}

/**
 * Write a single array to the buffer
 * @param obj array to write
 * @param buf target buffer
 */
static void
ucl_elt_array_write_config (ucl_object_t *obj, struct ucl_emitter_functions *func,
		unsigned int tabs, bool start_tabs, bool is_top)
{
	ucl_object_t *cur = obj;

	if (start_tabs) {
		ucl_add_tabs (func, tabs, false);
	}

	func->ucl_emitter_append_len ("[\n", 2, func->ud);
	while (cur) {
		ucl_elt_write_config (cur, func, tabs + 1, true, false, false);
		func->ucl_emitter_append_len (",\n", 2, func->ud);
		cur = cur->next;
	}
	ucl_add_tabs (func, tabs, false);
	func->ucl_emitter_append_character (']', 1, func->ud);
}

/**
 * Emit a single element
 * @param obj object
 * @param buf buffer
 */
static void
ucl_elt_write_config (ucl_object_t *obj, struct ucl_emitter_functions *func,
		unsigned int tabs, bool start_tabs, bool is_top, bool expand_array)
{
	bool flag;

	if (expand_array && obj->next != NULL) {
		ucl_elt_array_write_config (obj, func, tabs, start_tabs, is_top);
	}
	else {
		switch (obj->type) {
		case UCL_INT:
			if (start_tabs) {
				ucl_add_tabs (func, tabs, false);
			}
			func->ucl_emitter_append_int (ucl_object_toint (obj), func->ud);
			break;
		case UCL_FLOAT:
		case UCL_TIME:
			if (start_tabs) {
				ucl_add_tabs (func, tabs, false);
			}
			func->ucl_emitter_append_double (ucl_object_todouble (obj), func->ud);
			break;
		case UCL_BOOLEAN:
			if (start_tabs) {
				ucl_add_tabs (func, tabs, false);
			}
			flag = ucl_object_toboolean (obj);
			if (flag) {
				func->ucl_emitter_append_len ("true", 4, func->ud);
			}
			else {
				func->ucl_emitter_append_len ("false", 5, func->ud);
			}
			break;
		case UCL_STRING:
			if (start_tabs) {
				ucl_add_tabs (func, tabs, false);
			}
			ucl_elt_string_write_json (obj->value.sv, obj->len, func);
			break;
		case UCL_NULL:
			if (start_tabs) {
				ucl_add_tabs (func, tabs, false);
			}
			func->ucl_emitter_append_len ("null", 4, func->ud);
			break;
		case UCL_OBJECT:
			ucl_elt_obj_write_config (obj, func, tabs, start_tabs, is_top);
			break;
		case UCL_ARRAY:
			ucl_elt_array_write_config (obj->value.av, func, tabs, start_tabs, is_top);
			break;
		case UCL_USERDATA:
			break;
		}
	}
}

/**
 * Emit an object to rcl
 * @param obj object
 * @return rcl output (should be freed after using)
 */
static void
ucl_object_emit_config (ucl_object_t *obj, struct ucl_emitter_functions *func)
{
	ucl_elt_write_config (obj, func, 0, false, true, true);
}


static void
ucl_obj_write_yaml (ucl_object_t *obj, struct ucl_emitter_functions *func,
		unsigned int tabs, bool start_tabs)
{
	bool is_array = (obj->next != NULL);

	if (is_array) {
		ucl_elt_array_write_yaml (obj, func, tabs, start_tabs, false);
	}
	else {
		ucl_elt_write_yaml (obj, func, tabs, start_tabs, false, true);
	}
}

/**
 * Write a single object to the buffer
 * @param obj object to write
 * @param buf target buffer
 */
static void
ucl_elt_obj_write_yaml (ucl_object_t *obj, struct ucl_emitter_functions *func,
		unsigned int tabs, bool start_tabs, bool is_top)
{
	ucl_object_t *cur;
	ucl_hash_iter_t it = NULL;

	if (start_tabs) {
		ucl_add_tabs (func, tabs, is_top);
	}
	if (!is_top) {
		func->ucl_emitter_append_len ("{\n", 2, func->ud);
	}

	while ((cur = ucl_hash_iterate (obj->value.ov, &it))) {
		ucl_add_tabs (func, tabs + 1, is_top);
		if (cur->keylen > 0) {
			ucl_elt_string_write_json (cur->key, cur->keylen, func);
		}
		else {
			func->ucl_emitter_append_len ("null", 4, func->ud);
		}
		func->ucl_emitter_append_len (": ", 2, func->ud);
		ucl_obj_write_yaml (cur, func, is_top ? tabs : tabs + 1, false);
		if (ucl_hash_iter_has_next(it)) {
			if (!is_top) {
				func->ucl_emitter_append_len (",\n", 2, func->ud);
			}
			else {
				func->ucl_emitter_append_character ('\n', 1, func->ud);
			}
		}
		else {
			func->ucl_emitter_append_character ('\n', 1, func->ud);
		}
	}

	ucl_add_tabs (func, tabs, is_top);
	if (!is_top) {
		func->ucl_emitter_append_character ('}', 1, func->ud);
	}
}

/**
 * Write a single array to the buffer
 * @param obj array to write
 * @param buf target buffer
 */
static void
ucl_elt_array_write_yaml (ucl_object_t *obj, struct ucl_emitter_functions *func,
		unsigned int tabs, bool start_tabs, bool is_top)
{
	ucl_object_t *cur = obj;

	if (start_tabs) {
		ucl_add_tabs (func, tabs, false);
	}

	func->ucl_emitter_append_len ("[\n", 2, func->ud);
	while (cur) {
		ucl_elt_write_yaml (cur, func, tabs + 1, true, false, false);
		func->ucl_emitter_append_len (",\n", 2, func->ud);
		cur = cur->next;
	}
	ucl_add_tabs (func, tabs, false);
	func->ucl_emitter_append_character (']', 1, func->ud);
}

/**
 * Emit a single element
 * @param obj object
 * @param buf buffer
 */
static void
ucl_elt_write_yaml (ucl_object_t *obj, struct ucl_emitter_functions *func,
		unsigned int tabs, bool start_tabs, bool is_top, bool expand_array)
{
	bool flag;

	if (expand_array && obj->next != NULL ) {
		ucl_elt_array_write_yaml (obj, func, tabs, start_tabs, is_top);
	}
	else {
		switch (obj->type) {
		case UCL_INT:
			if (start_tabs) {
				ucl_add_tabs (func, tabs, false);
			}
			func->ucl_emitter_append_int (ucl_object_toint (obj), func->ud);
			break;
		case UCL_FLOAT:
		case UCL_TIME:
			if (start_tabs) {
				ucl_add_tabs (func, tabs, false);
			}
			func->ucl_emitter_append_double (ucl_object_todouble (obj), func->ud);
			break;
		case UCL_BOOLEAN:
			if (start_tabs) {
				ucl_add_tabs (func, tabs, false);
			}
			flag = ucl_object_toboolean (obj);
			if (flag) {
				func->ucl_emitter_append_len ("true", 4, func->ud);
			}
			else {
				func->ucl_emitter_append_len ("false", 5, func->ud);
			}
			break;
		case UCL_STRING:
			if (start_tabs) {
				ucl_add_tabs (func, tabs, false);
			}
			ucl_elt_string_write_json (obj->value.sv, obj->len, func);
			break;
		case UCL_NULL:
			if (start_tabs) {
				ucl_add_tabs (func, tabs, false);
			}
			func->ucl_emitter_append_len ("null", 4, func->ud);
			break;
		case UCL_OBJECT:
			ucl_elt_obj_write_yaml (obj, func, tabs, start_tabs, is_top);
			break;
		case UCL_ARRAY:
			ucl_elt_array_write_yaml (obj->value.av, func, tabs, start_tabs, is_top);
			break;
		case UCL_USERDATA:
			break;
		}
	}
}

/**
 * Emit an object to rcl
 * @param obj object
 * @return rcl output (should be freed after using)
 */
static void
ucl_object_emit_yaml (ucl_object_t *obj, struct ucl_emitter_functions *func)
{
	ucl_elt_write_yaml (obj, func, 0, false, true, true);
}

/*
 * Generic utstring output
 */
static int
ucl_utstring_append_character (unsigned char c, size_t len, void *ud)
{
	UT_string *buf = ud;

	if (len == 1) {
		utstring_append_c (buf, c);
	}
	else {
		utstring_reserve (buf, len);
		memset (&buf->d[buf->i], c, len);
		buf->i += len;
		buf->d[buf->i] = '\0';
	}

	return 0;
}

static int
ucl_utstring_append_len (const unsigned char *str, size_t len, void *ud)
{
	UT_string *buf = ud;

	utstring_append_len (buf, str, len);

	return 0;
}

static int
ucl_utstring_append_int (int64_t val, void *ud)
{
	UT_string *buf = ud;

	utstring_printf (buf, "%jd", (intmax_t)val);
	return 0;
}

static int
ucl_utstring_append_double (double val, void *ud)
{
	UT_string *buf = ud;
	const double delta = 0.0000001;

	if (val == (double)(int)val) {
		utstring_printf (buf, "%.1lf", val);
	}
	else if (fabs (val - (double)(int)val) < delta) {
		/* Write at maximum precision */
		utstring_printf (buf, "%.*lg", DBL_DIG, val);
	}
	else {
		utstring_printf (buf, "%lf", val);
	}

	return 0;
}


unsigned char *
ucl_object_emit (ucl_object_t *obj, enum ucl_emitter emit_type)
{
	UT_string *buf = NULL;
	unsigned char *res = NULL;
	struct ucl_emitter_functions func = {
		.ucl_emitter_append_character = ucl_utstring_append_character,
		.ucl_emitter_append_len = ucl_utstring_append_len,
		.ucl_emitter_append_int = ucl_utstring_append_int,
		.ucl_emitter_append_double = ucl_utstring_append_double
	};

	if (obj == NULL) {
		return NULL;
	}

	utstring_new (buf);
	func.ud = buf;

	if (buf != NULL) {
		if (emit_type == UCL_EMIT_JSON) {
			ucl_object_emit_json (obj, false, &func);
		}
		else if (emit_type == UCL_EMIT_JSON_COMPACT) {
			ucl_object_emit_json (obj, true, &func);
		}
		else if (emit_type == UCL_EMIT_YAML) {
			ucl_object_emit_yaml (obj, &func);
		}
		else {
			ucl_object_emit_config (obj, &func);
		}

		res = utstring_body (buf);
		free (buf);
	}

	return res;
}

bool
ucl_object_emit_full (ucl_object_t *obj, enum ucl_emitter emit_type,
		struct ucl_emitter_functions *emitter)
{
	if (emit_type == UCL_EMIT_JSON) {
		ucl_object_emit_json (obj, false, emitter);
	}
	else if (emit_type == UCL_EMIT_JSON_COMPACT) {
		ucl_object_emit_json (obj, true, emitter);
	}
	else if (emit_type == UCL_EMIT_YAML) {
		ucl_object_emit_yaml (obj, emitter);
	}
	else {
		ucl_object_emit_config (obj, emitter);
	}

	/* XXX: need some error checks here */
	return true;
}


unsigned char *
ucl_object_emit_single_json (ucl_object_t *obj)
{
	UT_string *buf = NULL;
	unsigned char *res = NULL;

	if (obj == NULL) {
		return NULL;
	}

	utstring_new (buf);

	if (buf != NULL) {
		switch (obj->type) {
		case UCL_OBJECT:
			ucl_utstring_append_len ("object", 6, buf);
			break;
		case UCL_ARRAY:
			ucl_utstring_append_len ("array", 5, buf);
			break;
		case UCL_INT:
			ucl_utstring_append_int (obj->value.iv, buf);
			break;
		case UCL_FLOAT:
		case UCL_TIME:
			ucl_utstring_append_double (obj->value.dv, buf);
			break;
		case UCL_NULL:
			ucl_utstring_append_len ("null", 4, buf);
			break;
		case UCL_BOOLEAN:
			if (obj->value.iv) {
				ucl_utstring_append_len ("true", 4, buf);
			}
			else {
				ucl_utstring_append_len ("false", 5, buf);
			}
			break;
		case UCL_STRING:
			ucl_utstring_append_len (obj->value.sv, obj->len, buf);
			break;
		case UCL_USERDATA:
			ucl_utstring_append_len ("userdata", 8, buf);
			break;
		}
		res = utstring_body (buf);
		free (buf);
	}

	return res;
}
