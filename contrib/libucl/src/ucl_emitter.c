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
 * @file ucl_emitter.c
 * Serialise UCL object to various of output formats
 */

static void ucl_emitter_common_elt (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj, bool first, bool print_key, bool compact);

#define UCL_EMIT_TYPE_OPS(type)		\
	static void ucl_emit_ ## type ## _elt (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj, bool first, bool print_key);	\
	static void ucl_emit_ ## type ## _start_obj (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj, bool print_key);	\
	static void ucl_emit_ ## type## _start_array (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj, bool print_key);	\
	static void ucl_emit_ ##type## _end_object (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj);	\
	static void ucl_emit_ ##type## _end_array (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj)

/*
 * JSON format operations
 */
UCL_EMIT_TYPE_OPS(json);
UCL_EMIT_TYPE_OPS(json_compact);
UCL_EMIT_TYPE_OPS(config);
UCL_EMIT_TYPE_OPS(yaml);

#define UCL_EMIT_TYPE_CONTENT(type) {	\
	.ucl_emitter_write_elt = ucl_emit_ ## type ## _elt,	\
	.ucl_emitter_start_object = ucl_emit_ ## type ##_start_obj,	\
	.ucl_emitter_start_array = ucl_emit_ ## type ##_start_array,	\
	.ucl_emitter_end_object = ucl_emit_ ## type ##_end_object,	\
	.ucl_emitter_end_array = ucl_emit_ ## type ##_end_array	\
}


const struct ucl_emitter_operations ucl_standartd_emitter_ops[] = {
	[UCL_EMIT_JSON] = UCL_EMIT_TYPE_CONTENT(json),
	[UCL_EMIT_JSON_COMPACT] = UCL_EMIT_TYPE_CONTENT(json_compact),
	[UCL_EMIT_CONFIG] = UCL_EMIT_TYPE_CONTENT(config),
	[UCL_EMIT_YAML] = UCL_EMIT_TYPE_CONTENT(yaml)
};

/*
 * Utility to check whether we need a top object
 */
#define UCL_EMIT_IDENT_TOP_OBJ(ctx, obj) ((ctx)->top != (obj) || \
		((ctx)->id == UCL_EMIT_JSON_COMPACT || (ctx)->id == UCL_EMIT_JSON))


/**
 * Add tabulation to the output buffer
 * @param buf target buffer
 * @param tabs number of tabs to add
 */
static inline void
ucl_add_tabs (const struct ucl_emitter_functions *func, unsigned int tabs,
		bool compact)
{
	if (!compact && tabs > 0) {
		func->ucl_emitter_append_character (' ', tabs * 4, func->ud);
	}
}

/**
 * Print key for the element
 * @param ctx
 * @param obj
 */
static void
ucl_emitter_print_key (bool print_key, struct ucl_emitter_context *ctx,
		const ucl_object_t *obj, bool compact)
{
	const struct ucl_emitter_functions *func = ctx->func;

	if (!print_key) {
		return;
	}

	if (ctx->id == UCL_EMIT_CONFIG) {
		if (obj->flags & UCL_OBJECT_NEED_KEY_ESCAPE) {
			ucl_elt_string_write_json (obj->key, obj->keylen, ctx);
		}
		else {
			func->ucl_emitter_append_len (obj->key, obj->keylen, func->ud);
		}

		if (obj->type != UCL_OBJECT && obj->type != UCL_ARRAY) {
			func->ucl_emitter_append_len (" = ", 3, func->ud);
		}
		else {
			func->ucl_emitter_append_character (' ', 1, func->ud);
		}
	}
	else {
		if (obj->keylen > 0) {
			ucl_elt_string_write_json (obj->key, obj->keylen, ctx);
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
	}
}

static void
ucl_emitter_finish_object (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj, bool compact, bool is_array)
{
	const struct ucl_emitter_functions *func = ctx->func;

	if (ctx->id == UCL_EMIT_CONFIG && obj != ctx->top) {
		if (obj->type != UCL_OBJECT && obj->type != UCL_ARRAY) {
			if (!is_array) {
				/* Objects are split by ';' */
				func->ucl_emitter_append_len (";\n", 2, func->ud);
			}
			else {
				/* Use commas for arrays */
				func->ucl_emitter_append_len (",\n", 2, func->ud);
			}
		}
		else {
			func->ucl_emitter_append_character ('\n', 1, func->ud);
		}
	}
}

/**
 * End standard ucl object
 * @param ctx emitter context
 * @param compact compact flag
 */
static void
ucl_emitter_common_end_object (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj, bool compact)
{
	const struct ucl_emitter_functions *func = ctx->func;

	if (UCL_EMIT_IDENT_TOP_OBJ(ctx, obj)) {
		ctx->ident --;
		if (compact) {
			func->ucl_emitter_append_character ('}', 1, func->ud);
		}
		else {
			if (ctx->id != UCL_EMIT_CONFIG) {
				/* newline is already added for this format */
				func->ucl_emitter_append_character ('\n', 1, func->ud);
			}
			ucl_add_tabs (func, ctx->ident, compact);
			func->ucl_emitter_append_character ('}', 1, func->ud);
		}
	}

	ucl_emitter_finish_object (ctx, obj, compact, false);
}

/**
 * End standard ucl array
 * @param ctx emitter context
 * @param compact compact flag
 */
static void
ucl_emitter_common_end_array (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj, bool compact)
{
	const struct ucl_emitter_functions *func = ctx->func;

	ctx->ident --;
	if (compact) {
		func->ucl_emitter_append_character (']', 1, func->ud);
	}
	else {
		if (ctx->id != UCL_EMIT_CONFIG) {
			/* newline is already added for this format */
			func->ucl_emitter_append_character ('\n', 1, func->ud);
		}
		ucl_add_tabs (func, ctx->ident, compact);
		func->ucl_emitter_append_character (']', 1, func->ud);
	}

	ucl_emitter_finish_object (ctx, obj, compact, true);
}

/**
 * Start emit standard UCL array
 * @param ctx emitter context
 * @param obj object to write
 * @param compact compact flag
 */
static void
ucl_emitter_common_start_array (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj, bool print_key, bool compact)
{
	const ucl_object_t *cur;
	const struct ucl_emitter_functions *func = ctx->func;
	bool first = true;

	ucl_emitter_print_key (print_key, ctx, obj, compact);

	if (compact) {
		func->ucl_emitter_append_character ('[', 1, func->ud);
	}
	else {
		func->ucl_emitter_append_len ("[\n", 2, func->ud);
	}

	ctx->ident ++;

	if (obj->type == UCL_ARRAY) {
		/* explicit array */
		cur = obj->value.av;
	}
	else {
		/* implicit array */
		cur = obj;
	}

	while (cur) {
		ucl_emitter_common_elt (ctx, cur, first, false, compact);
		first = false;
		cur = cur->next;
	}
}

/**
 * Start emit standard UCL object
 * @param ctx emitter context
 * @param obj object to write
 * @param compact compact flag
 */
static void
ucl_emitter_common_start_object (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj, bool print_key, bool compact)
{
	ucl_hash_iter_t it = NULL;
	const ucl_object_t *cur, *elt;
	const struct ucl_emitter_functions *func = ctx->func;
	bool first = true;

	ucl_emitter_print_key (print_key, ctx, obj, compact);
	/*
	 * Print <ident_level>{
	 * <ident_level + 1><object content>
	 */
	if (UCL_EMIT_IDENT_TOP_OBJ(ctx, obj)) {
		if (compact) {
			func->ucl_emitter_append_character ('{', 1, func->ud);
		}
		else {
			func->ucl_emitter_append_len ("{\n", 2, func->ud);
		}
		ctx->ident ++;
	}

	while ((cur = ucl_hash_iterate (obj->value.ov, &it))) {

		if (ctx->id == UCL_EMIT_CONFIG) {
			LL_FOREACH (cur, elt) {
				ucl_emitter_common_elt (ctx, elt, first, true, compact);
			}
		}
		else {
			/* Expand implicit arrays */
			if (cur->next != NULL) {
				if (!first) {
					if (compact) {
						func->ucl_emitter_append_character (',', 1, func->ud);
					}
					else {
						func->ucl_emitter_append_len (",\n", 2, func->ud);
					}
				}
				ucl_add_tabs (func, ctx->ident, compact);
				ucl_emitter_common_start_array (ctx, cur, true, compact);
				ucl_emitter_common_end_array (ctx, cur, compact);
			}
			else {
				ucl_emitter_common_elt (ctx, cur, first, true, compact);
			}
		}

		first = false;
	}
}

/**
 * Common choice of object emitting
 * @param ctx emitter context
 * @param obj object to print
 * @param first flag to mark the first element
 * @param print_key print key of an object
 * @param compact compact output
 */
static void
ucl_emitter_common_elt (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj, bool first, bool print_key, bool compact)
{
	const struct ucl_emitter_functions *func = ctx->func;
	bool flag;

	if (ctx->id != UCL_EMIT_CONFIG && !first) {
		if (compact) {
			func->ucl_emitter_append_character (',', 1, func->ud);
		}
		else {
			func->ucl_emitter_append_len (",\n", 2, func->ud);
		}
	}

	ucl_add_tabs (func, ctx->ident, compact);

	switch (obj->type) {
	case UCL_INT:
		ucl_emitter_print_key (print_key, ctx, obj, compact);
		func->ucl_emitter_append_int (ucl_object_toint (obj), func->ud);
		ucl_emitter_finish_object (ctx, obj, compact, !print_key);
		break;
	case UCL_FLOAT:
	case UCL_TIME:
		ucl_emitter_print_key (print_key, ctx, obj, compact);
		func->ucl_emitter_append_double (ucl_object_todouble (obj), func->ud);
		ucl_emitter_finish_object (ctx, obj, compact, !print_key);
		break;
	case UCL_BOOLEAN:
		ucl_emitter_print_key (print_key, ctx, obj, compact);
		flag = ucl_object_toboolean (obj);
		if (flag) {
			func->ucl_emitter_append_len ("true", 4, func->ud);
		}
		else {
			func->ucl_emitter_append_len ("false", 5, func->ud);
		}
		ucl_emitter_finish_object (ctx, obj, compact, !print_key);
		break;
	case UCL_STRING:
		ucl_emitter_print_key (print_key, ctx, obj, compact);
		ucl_elt_string_write_json (obj->value.sv, obj->len, ctx);
		ucl_emitter_finish_object (ctx, obj, compact, !print_key);
		break;
	case UCL_NULL:
		ucl_emitter_print_key (print_key, ctx, obj, compact);
		func->ucl_emitter_append_len ("null", 4, func->ud);
		ucl_emitter_finish_object (ctx, obj, compact, !print_key);
		break;
	case UCL_OBJECT:
		ucl_emitter_common_start_object (ctx, obj, print_key, compact);
		ucl_emitter_common_end_object (ctx, obj, compact);
		break;
	case UCL_ARRAY:
		ucl_emitter_common_start_array (ctx, obj, print_key, compact);
		ucl_emitter_common_end_array (ctx, obj, compact);
		break;
	case UCL_USERDATA:
		break;
	}
}

/*
 * Specific standard implementations of the emitter functions
 */
#define UCL_EMIT_TYPE_IMPL(type, compact)		\
	static void ucl_emit_ ## type ## _elt (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj, bool first, bool print_key) {	\
		ucl_emitter_common_elt (ctx, obj, first, print_key, (compact));	\
	}	\
	static void ucl_emit_ ## type ## _start_obj (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj, bool print_key) {	\
		ucl_emitter_common_start_object (ctx, obj, print_key, (compact));	\
	}	\
	static void ucl_emit_ ## type## _start_array (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj, bool print_key) {	\
		ucl_emitter_common_start_array (ctx, obj, print_key, (compact));	\
	}	\
	static void ucl_emit_ ##type## _end_object (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj) {	\
		ucl_emitter_common_end_object (ctx, obj, (compact));	\
	}	\
	static void ucl_emit_ ##type## _end_array (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj) {	\
		ucl_emitter_common_end_array (ctx, obj, (compact));	\
	}

UCL_EMIT_TYPE_IMPL(json, false);
UCL_EMIT_TYPE_IMPL(json_compact, true);
UCL_EMIT_TYPE_IMPL(config, false);
UCL_EMIT_TYPE_IMPL(yaml, false);

unsigned char *
ucl_object_emit (const ucl_object_t *obj, enum ucl_emitter emit_type)
{
	unsigned char *res = NULL;
	struct ucl_emitter_functions *func;
	if (obj == NULL) {
		return NULL;
	}

	func = ucl_object_emit_memory_funcs ((void **)&res);

	if (func != NULL) {
		ucl_object_emit_full (obj, emit_type, func);
		ucl_object_emit_funcs_free (func);
	}

	return res;
}

bool
ucl_object_emit_full (const ucl_object_t *obj, enum ucl_emitter emit_type,
		struct ucl_emitter_functions *emitter)
{
	const struct ucl_emitter_context *ctx;
	struct ucl_emitter_context my_ctx;
	bool res = false;

	ctx = ucl_emit_get_standard_context (emit_type);
	if (ctx != NULL) {
		memcpy (&my_ctx, ctx, sizeof (my_ctx));
		my_ctx.func = emitter;
		my_ctx.ident = 0;
		my_ctx.top = obj;

		my_ctx.ops->ucl_emitter_write_elt (&my_ctx, obj, true, false);
		res = true;
	}

	return res;
}
