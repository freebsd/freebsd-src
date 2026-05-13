/*
 * bytecode.c
 * variable expansion bytecode evaluator
 *
 * Copyright (c) 2026 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>

/*
 * !doc
 *
 * libpkgconf `bytecode` module
 * ============================
 *
 * The libpkgconf `bytecode` module contains the functions related to
 * evaluating variable expansion bytecode.
 */

#define PKGCONF_EVAL_MAX_OUTPUT (PKGCONF_BUFSIZE - 1)
#define PKGCONF_EVAL_MAX_ITERATIONS (512)

static bool
pkgconf_bytecode_eval_append_slice(pkgconf_bytecode_eval_ctx_t *ctx,
	pkgconf_buffer_t *out,
	const char *p,
	size_t n)
{
	size_t cur = pkgconf_buffer_len(out);

	if (cur >= PKGCONF_EVAL_MAX_OUTPUT)
		return false;

	if (n > PKGCONF_EVAL_MAX_OUTPUT - cur)
	{
		pkgconf_warn(ctx->client,
			"warning: truncating very long variable to 64KB\n");

		n = PKGCONF_EVAL_MAX_OUTPUT - cur;
		pkgconf_buffer_append_slice(out, p, n);
		return false;
	}

	pkgconf_buffer_append_slice(out, p, n);
	return true;
}

static bool
pkgconf_bytecode_eval_append(pkgconf_bytecode_eval_ctx_t *ctx,
	pkgconf_buffer_t *out,
	const char *s)
{
	if (s == NULL || *s == '\0')
		return true;

	return pkgconf_bytecode_eval_append_slice(ctx, out, s, strlen(s));
}

static bool
pkgconf_bytecode_eval_internal(pkgconf_bytecode_eval_ctx_t *ctx,
	const pkgconf_bytecode_t *bc,
	pkgconf_buffer_t *out,
	bool *saw_sysroot);

static pkgconf_variable_t *
pkgconf_bytecode_eval_scan(const pkgconf_list_t *vars,
	const char *name, size_t nlen,
	unsigned int require_flags,
	unsigned int forbid_flags)
{
	const pkgconf_node_t *node;

	PKGCONF_FOREACH_LIST_ENTRY(vars->head, node)
	{
		pkgconf_variable_t *v = node->data;

		if ((v->flags & require_flags) != require_flags)
			continue;

		if ((v->flags & forbid_flags) != 0)
			continue;

		if (pkgconf_str_eq_slice(v->key, name, nlen))
			return v;
	}

	return NULL;
}

pkgconf_variable_t *
pkgconf_bytecode_eval_lookup_var(pkgconf_bytecode_eval_ctx_t *ctx,
	const char *name, size_t nlen)
{
	pkgconf_variable_t *v;

	if ((v = pkgconf_bytecode_eval_scan(&ctx->client->global_vars, name, nlen, PKGCONF_VARIABLEF_OVERRIDE, 0)) != NULL)
		return v;

	if (ctx->vars != NULL && (v = pkgconf_bytecode_eval_scan(ctx->vars, name, nlen, 0, 0)) != NULL)
		return v;

	if ((v = pkgconf_bytecode_eval_scan(&ctx->client->global_vars, name, nlen, 0, PKGCONF_VARIABLEF_OVERRIDE)) != NULL)
		return v;

	return NULL;
}

static bool
pkgconf_bytecode_eval_var(pkgconf_bytecode_eval_ctx_t *ctx,
	const char *name, size_t nlen,
	pkgconf_buffer_t *out,
	bool *saw_sysroot)
{
	pkgconf_variable_t *v;

	v = pkgconf_bytecode_eval_lookup_var(ctx, name, nlen);
	if (v == NULL)
		return true;

	if (v->expanding)
		return false;

	v->expanding = true;

	bool inner_saw = false;
	bool ok = pkgconf_bytecode_eval_internal(ctx, &v->bc, out, &inner_saw);

	v->expanding = false;

	if (!ok)
		return false;

	if (saw_sysroot != NULL)
		*saw_sysroot |= inner_saw;

	return true;
}

static bool
pkgconf_bytecode_eval_internal(pkgconf_bytecode_eval_ctx_t *ctx,
	const pkgconf_bytecode_t *bc,
	pkgconf_buffer_t *out,
	bool *saw_sysroot)
{
	(void) ctx;

	if (bc == NULL || out == NULL)
		return false;

	const uint8_t *p = bc->base;
	const uint8_t *end = bc->base + bc->len;

	if (++ctx->expansions > PKGCONF_EVAL_MAX_ITERATIONS)
	{
		pkgconf_warn(ctx->client,
			"warning: bytecode program exceeds iteration limit (" SIZE_FMT_SPECIFIER ")\n",
			ctx->expansions - 1);
		return false;
	}

	while (p < end)
	{
		const pkgconf_bytecode_op_t *op =
			(const pkgconf_bytecode_op_t *)p;

		if ((const uint8_t *)op + sizeof(*op) > end)
			return false;

		if ((const uint8_t *)op + sizeof(*op) + op->size > end)
			return false;

		switch (op->tag)
		{
		case PKGCONF_BYTECODE_OP_TEXT:
			/* this only fails due to truncation */
			if (!pkgconf_bytecode_eval_append_slice(ctx, out, op->data, op->size))
				return false;
			break;

		case PKGCONF_BYTECODE_OP_VAR:
			if (!pkgconf_bytecode_eval_var(ctx, op->data, op->size, out, saw_sysroot))
				return false;
			break;

		case PKGCONF_BYTECODE_OP_SYSROOT:
			if (saw_sysroot != NULL)
				*saw_sysroot = true;
			if (!pkgconf_bytecode_eval_append(ctx, out, pkgconf_buffer_str_or_empty(&ctx->sysroot)))
				return false;
			break;

		default:
			/* reserved/unimplemented */
			return false;
		}

		p = (const uint8_t *)pkgconf_bytecode_op_next(op);
	}

	return true;
}

static void
pkgconf_bytecode_eval_ctx_init(pkgconf_bytecode_eval_ctx_t *ctx,
	const pkgconf_client_t *client,
	const pkgconf_list_t *vars)
{
	memset(ctx, 0, sizeof(*ctx));

	ctx->client = client;
	ctx->vars = vars;

	const char *raw = pkgconf_client_get_sysroot_dir(client);

	/* disabled sysroot cases */
	if (raw == NULL || *raw == '\0')
		return;

	if (raw[0] == '.' && raw[1] == '\0')
		return;

	if (raw[0] == '/' && raw[1] == '\0')
		return;

	pkgconf_buffer_append(&ctx->sysroot, raw);

	while (pkgconf_buffer_len(&ctx->sysroot) > 1 && ctx->sysroot.end[-1] == '/')
		pkgconf_buffer_trim_byte(&ctx->sysroot);

	/* if normalization yields "/", disable by making buffer empty */
	if (pkgconf_buffer_len(&ctx->sysroot) == 1 && ctx->sysroot.base[0] == '/')
		pkgconf_buffer_trim_byte(&ctx->sysroot);
}

bool
pkgconf_bytecode_eval(const pkgconf_client_t *client,
	const pkgconf_list_t *vars,
	const pkgconf_bytecode_t *bc,
	pkgconf_buffer_t *out,
	bool *saw_sysroot)
{
	bool ret;

	if (client == NULL || bc == NULL || out == NULL)
		return false;

	pkgconf_bytecode_eval_ctx_t ctx;
	pkgconf_bytecode_eval_ctx_init(&ctx, client, vars);

	if (saw_sysroot != NULL)
		*saw_sysroot = false;

	ret = pkgconf_bytecode_eval_internal(&ctx, bc, out, saw_sysroot);

	pkgconf_buffer_finalize(&ctx.sysroot);

	return ret;
}

void
pkgconf_bytecode_emit(pkgconf_buffer_t *buf,
	enum pkgconf_bytecode_op tag,
	const void *data,
	uint32_t size)
{
	pkgconf_bytecode_op_t op = {
		.tag = tag,
		.size = size,
	};

	pkgconf_buffer_append_slice(buf, (const char *) &op, sizeof(op));

	if (size != 0)
		pkgconf_buffer_append_slice(buf, (const char *) data, (size_t) size);
}

void
pkgconf_bytecode_emit_text(pkgconf_buffer_t *buf, const char *p, size_t n)
{
	if (p == NULL || n == 0)
		return;

	pkgconf_bytecode_emit(buf, PKGCONF_BYTECODE_OP_TEXT, p, (uint32_t) n);
}

void
pkgconf_bytecode_emit_var(pkgconf_buffer_t *buf, const char *name, size_t nlen)
{
	if (name == NULL || nlen == 0)
		return;

	pkgconf_bytecode_emit(buf, PKGCONF_BYTECODE_OP_VAR, name, (uint32_t) nlen);
}

void
pkgconf_bytecode_emit_sysroot(pkgconf_buffer_t *buf)
{
	pkgconf_bytecode_emit(buf, PKGCONF_BYTECODE_OP_SYSROOT, NULL, 0);
}

void
pkgconf_bytecode_from_buffer(pkgconf_bytecode_t *bc, const pkgconf_buffer_t *buf)
{
	bc->base = (const uint8_t *)buf->base;
	bc->len = (size_t)(buf->end - buf->base);
}

void
pkgconf_bytecode_compile(pkgconf_buffer_t *out, const char *value)
{
	const char *p, *text_start;

	if (out == NULL || value == NULL)
		return;

	p = value;
	text_start = value;

	for (; *p != '\0'; p++)
	{
		const char *name, *q;

		if (*p != '$')
			continue;

		/* $$ escapes to a literal $ */
		if (p[1] == '$')
		{
			if (p > text_start)
				pkgconf_bytecode_emit_text(out, text_start, (size_t)(p - text_start));

			pkgconf_bytecode_emit_text(out, "$", 1);

			p++;
			text_start = p + 1;
			continue;
		}

		if (p[1] != '{')
			continue;

		if (p > text_start)
			pkgconf_bytecode_emit_text(out, text_start, (size_t)(p - text_start));

		name = p + 2;
		q = name;

		for (; *q != '\0' && *q != '}'; q++)
			;

		/* make sure a variable expansion ends with } */
		if (*q != '}')
		{
			text_start = p;
			continue;
		}

		/* if this is not a valid variable, emit it as text */
		size_t nlen = (size_t)(q - name);
		if (nlen == 0 || nlen >= PKGCONF_ITEM_SIZE)
		{
			pkgconf_bytecode_emit_text(out, p, (size_t)((q + 1) - p));
			p = q;
			text_start = p + 1;
			continue;
		}

		/* we need to special-case ${pc_sysrootdir} and emit OP_SYSROOT instead... */
		if (nlen == strlen("pc_sysrootdir") && !memcmp(name, "pc_sysrootdir", nlen))
			pkgconf_bytecode_emit_sysroot(out);
		else
			pkgconf_bytecode_emit_var(out, name, nlen);

		p = q;
		text_start = p + 1;
	}

	if (p > text_start)
		pkgconf_bytecode_emit_text(out, text_start, (size_t)(p - text_start));
}

bool
pkgconf_bytecode_eval_str_to_buf(const pkgconf_client_t *client, const pkgconf_list_t *vars, const char *input, bool *saw_sysroot, pkgconf_buffer_t *out)
{
	pkgconf_buffer_t bcbuf = PKGCONF_BUFFER_INITIALIZER;
	pkgconf_bytecode_t bc;
	bool ret = false;

	pkgconf_bytecode_compile(&bcbuf, input);
	pkgconf_bytecode_from_buffer(&bc, &bcbuf);

	ret = pkgconf_bytecode_eval(client, vars, &bc, out, saw_sysroot);

	pkgconf_buffer_finalize(&bcbuf);

	return ret;
}

char *
pkgconf_bytecode_eval_str(const pkgconf_client_t *client, const pkgconf_list_t *vars, const char *input, bool *saw_sysroot)
{
	pkgconf_buffer_t out = PKGCONF_BUFFER_INITIALIZER;

	if (!pkgconf_bytecode_eval_str_to_buf(client, vars, input, saw_sysroot, &out))
	{
		if (pkgconf_buffer_len(&out) > 0)
			return pkgconf_buffer_freeze(&out);

		pkgconf_buffer_finalize(&out);
		return NULL;
	}

	if (pkgconf_buffer_len(&out) == 0)
	{
		pkgconf_buffer_finalize(&out);
		return strdup("");
	}

	return pkgconf_buffer_freeze(&out);
}

bool
pkgconf_bytecode_references_var(const pkgconf_buffer_t *buf, const char *key)
{
	const uint8_t *p, *end;
	size_t klen;

	if (buf == NULL || key == NULL)
		return false;

	klen = strlen(key);
	p = (uint8_t *) buf->base;
	end = (uint8_t *) buf->end;

	while (p < end)
	{
		const pkgconf_bytecode_op_t *op = (const pkgconf_bytecode_op_t *)p;

		if (p + sizeof(*op) > end)
			return false;

		if (p + sizeof(*op) + op->size > end)
			return false;

		if (op->tag == PKGCONF_BYTECODE_OP_VAR)
		{
			if (op->size == (uint32_t) klen && pkgconf_str_eq_slice(op->data, key, klen))
				return true;
		}

		p += sizeof(*op) + op->size;
	}

	return false;
}

static bool
pkgconf_bytecode_op_is_selfref(const pkgconf_bytecode_op_t *op, const char *key)
{
	const size_t klen = strlen(key);

	if (op->tag != PKGCONF_BYTECODE_OP_VAR)
		return false;

	if (op->size != (uint32_t) klen)
		return false;

	return pkgconf_str_eq_slice(op->data, key, klen);
}

static void
pkgconf_bytecode_append_stream(pkgconf_buffer_t *dst, const pkgconf_buffer_t *bcbuf)
{
	if (dst == NULL || bcbuf == NULL || pkgconf_buffer_str(bcbuf) == NULL)
		return;

	pkgconf_buffer_append_slice(dst, pkgconf_buffer_str(bcbuf), pkgconf_buffer_len(bcbuf));
}

bool
pkgconf_bytecode_rewrite_selfrefs(pkgconf_buffer_t *out, const pkgconf_buffer_t *rhs, const char *key, const pkgconf_buffer_t *prev)
{
	const uint8_t *p = (uint8_t *) rhs->base;
	const uint8_t *end = (uint8_t *) rhs->end;

	while (p < end)
	{
		const pkgconf_bytecode_op_t *op = (const pkgconf_bytecode_op_t *)p;

		if (p + sizeof(*op) > end)
			return false;

		if (p + sizeof(*op) + op->size > end)
			return false;

		if (pkgconf_bytecode_op_is_selfref(op, key))
			pkgconf_bytecode_append_stream(out, prev);
		else
			pkgconf_buffer_append_slice(out, (const char *) op, sizeof(*op) + op->size);

		p += sizeof(*op) + op->size;
	}

	return true;
}
