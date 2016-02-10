/*-
 * Copyright (c) 2011,2012 Kai Wang
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
 */

#include "ld.h"
#include "ld_arch.h"
#include "ld_script.h"
#include "ld_exp.h"
#include "ld_layout.h"

ELFTC_VCSID("$Id: ld_exp.c 3278 2015-12-11 21:39:13Z kaiwang27 $");

/*
 * Support routines for ldscript expression.
 */

static struct ld_exp *_alloc_exp(struct ld *ld);
static int64_t _assignment(struct ld *ld, struct ld_exp *le);
static int64_t _func_addr(struct ld *ld, struct ld_exp *le);
static int64_t _func_align(struct ld *ld, struct ld_exp *le);
static int64_t _func_alignof(struct ld *ld, struct ld_exp *le);
static int64_t _func_data_segment_align(struct ld *ld, struct ld_exp *le);
static int64_t _func_data_segment_end(struct ld *ld, struct ld_exp *le);
static int64_t _func_data_segment_relro_end(struct ld *ld, struct ld_exp *le);
static int64_t _func_defined(struct ld *ld, struct ld_exp *le);
static int64_t _func_length(struct ld *ld, struct ld_exp *le);
static int64_t _func_loadaddr(struct ld *ld, struct ld_exp *le);
static int64_t _func_max(struct ld *ld, struct ld_exp *le);
static int64_t _func_min(struct ld *ld, struct ld_exp *le);
static int64_t _func_next(struct ld *ld, struct ld_exp *le);
static int64_t _func_origin(struct ld *ld, struct ld_exp *le);
static int64_t _func_segment_start(struct ld *ld, struct ld_exp *le);
static int64_t _func_sizeof(struct ld *ld, struct ld_exp *le);
static int64_t _func_sizeof_headers(struct ld *ld);
static int64_t _symbol_val(struct ld *ld, char *name);
static int64_t _symbolic_constant(struct ld *ld, const char *name);

#define	_EXP_EVAL(x) ld_exp_eval(ld, (x))
#define	_EXP_DUMP(x) ld_exp_dump(ld, (x))

void
ld_exp_free(struct ld_exp *le)
{

	if (le == NULL)
		return;

	ld_exp_free(le->le_e1);
	ld_exp_free(le->le_e2);
	ld_exp_free(le->le_e3);
	if (le->le_assign != NULL)
		ld_script_assign_free(le->le_assign);
	if (le->le_name != NULL)
		free(le->le_name);
	free(le);
}

struct ld_exp *
ld_exp_unary(struct ld *ld, enum ld_exp_op op, struct ld_exp *e1)
{
	struct ld_exp *le;

	le = _alloc_exp(ld);
	le->le_op = op;
	le->le_e1 = e1;

	return (le);
}

struct ld_exp *
ld_exp_binary(struct ld *ld, enum ld_exp_op op, struct ld_exp *e1,
    struct ld_exp *e2)
{
	struct ld_exp *le;

	le = _alloc_exp(ld);
	le->le_op = op;
	le->le_e1 = e1;
	le->le_e2 = e2;

	return (le);
}

struct ld_exp *
ld_exp_trinary(struct ld *ld, struct ld_exp *e1, struct ld_exp *e2,
    struct ld_exp *e3)
{
	struct ld_exp *le;

	le = _alloc_exp(ld);
	le->le_op = LEOP_TRINARY;
	le->le_e1 = e1;
	le->le_e2 = e2;
	le->le_e3 = e3;

	return (le);
}

struct ld_exp *
ld_exp_sizeof_headers(struct ld *ld)
{
	struct ld_exp *le;

	le = _alloc_exp(ld);
	le->le_op = LEOP_SIZEOF_HEADERS;

	return (le);
}

struct ld_exp *
ld_exp_constant(struct ld *ld, int64_t val)
{
	struct ld_exp *le;

	le = _alloc_exp(ld);
	le->le_op = LEOP_CONSTANT;
	le->le_val = val;

	return (le);
}

struct ld_exp *
ld_exp_symbolic_constant(struct ld *ld, const char *name)
{
	struct ld_exp *le;

	le = _alloc_exp(ld);
	le->le_op = LEOP_SYMBOLIC_CONSTANT;
	le->le_name = strdup(name);
	if (le->le_name == NULL)
		ld_fatal_std(ld, "calloc");

	return (le);
}

struct ld_exp *
ld_exp_symbol(struct ld *ld, const char *name)
{
	struct ld_exp *le;

	le = _alloc_exp(ld);
	le->le_op = LEOP_SYMBOL;
	le->le_name = strdup(name);
	if (le->le_name == NULL)
		ld_fatal_std(ld, "calloc");

	return (le);
}

struct ld_exp *
ld_exp_name(struct ld *ld, const char *name)
{
	struct ld_exp *le;

	le = _alloc_exp(ld);
	le->le_op = LEOP_SECTION_NAME;
	le->le_name = strdup(name);
	if (le->le_name == NULL)
		ld_fatal_std(ld, "calloc");

	return (le);
}

struct ld_exp *
ld_exp_assign(struct ld *ld, struct ld_script_assign *assign)
{
	struct ld_exp *le;

	le = _alloc_exp(ld);
	le->le_op = LEOP_ASSIGN;
	le->le_assign = assign;

	return (le);
}

int64_t
ld_exp_eval(struct ld* ld, struct ld_exp *le)
{

	assert(le != NULL);
	switch (le->le_op) {
	case LEOP_ABS:
		return (llabs(_EXP_EVAL(le->le_e1)));
	case LEOP_ADD:
		return (_EXP_EVAL(le->le_e1) + _EXP_EVAL(le->le_e2));
	case LEOP_ADDR:
		return (_func_addr(ld, le));
	case LEOP_ALIGN:
	case LEOP_BLOCK:
		return (_func_align(ld, le));
	case LEOP_ALIGNOF:
		return (_func_alignof(ld, le));
	case LEOP_AND:
		return (_EXP_EVAL(le->le_e1) & _EXP_EVAL(le->le_e2));
	case LEOP_ASSIGN:
		return (_assignment(ld, le));
	case LEOP_CONSTANT:
		return (le->le_val);
	case LEOP_DIV:
		return (_EXP_EVAL(le->le_e1) / _EXP_EVAL(le->le_e2));
	case LEOP_DSA:
		return (_func_data_segment_align(ld, le));
	case LEOP_DSE:
		return (_func_data_segment_end(ld, le));
	case LEOP_DSRE:
		return (_func_data_segment_relro_end(ld, le));
	case LEOP_DEFINED:
		return (_func_defined(ld, le));
	case LEOP_EQUAL:
		return (_EXP_EVAL(le->le_e1) == _EXP_EVAL(le->le_e2));
	case LEOP_GE:
		return (_EXP_EVAL(le->le_e1) >= _EXP_EVAL(le->le_e2));
	case LEOP_GREATER:
		return (_EXP_EVAL(le->le_e1) > _EXP_EVAL(le->le_e2));
	case LEOP_LENGTH:
		return (_func_length(ld, le));
	case LEOP_LOADADDR:
		return (_func_loadaddr(ld, le));
	case LEOP_LOGICAL_AND:
		return (_EXP_EVAL(le->le_e1) && _EXP_EVAL(le->le_e2));
	case LEOP_LOGICAL_OR:
		return (_EXP_EVAL(le->le_e1) || _EXP_EVAL(le->le_e2));
	case LEOP_LSHIFT:
		return (_EXP_EVAL(le->le_e1) << _EXP_EVAL(le->le_e2));
	case LEOP_MAX:
		return (_func_max(ld, le));
	case LEOP_MIN:
		return (_func_min(ld, le));
	case LEOP_MINUS:
		return (-(_EXP_EVAL(le->le_e1)));
	case LEOP_MOD:
		return (_EXP_EVAL(le->le_e1) % _EXP_EVAL(le->le_e2));
	case LEOP_MUL:
		return (_EXP_EVAL(le->le_e1) * _EXP_EVAL(le->le_e2));
	case LEOP_NE:
		return (_EXP_EVAL(le->le_e1) != _EXP_EVAL(le->le_e2));
	case LEOP_NEGATION:
		return (~(_EXP_EVAL(le->le_e1)));
	case LEOP_NEXT:
		return (_func_next(ld, le));
	case LEOP_NOT:
		return (!(_EXP_EVAL(le->le_e1)));
	case LEOP_OR:
		return (_EXP_EVAL(le->le_e1) | _EXP_EVAL(le->le_e2));
	case LEOP_ORIGIN:
		return (_func_origin(ld, le));
	case LEOP_RSHIFT:
		return (_EXP_EVAL(le->le_e1) >> _EXP_EVAL(le->le_e2));
	case LEOP_SEGMENT_START:
		return (_func_segment_start(ld, le));
	case LEOP_SIZEOF:
		return (_func_sizeof(ld, le));
	case LEOP_SIZEOF_HEADERS:
		return (_func_sizeof_headers(ld));
	case LEOP_SUBSTRACT:
		return (_EXP_EVAL(le->le_e1) - _EXP_EVAL(le->le_e2));
	case LEOP_SYMBOL:
		return (_symbol_val(ld, le->le_name));
	case LEOP_SYMBOLIC_CONSTANT:
		return (_symbolic_constant(ld, le->le_name));
	case LEOP_TRINARY:
		return (_EXP_EVAL(le->le_e1) ? _EXP_EVAL(le->le_e2) :
		    _EXP_EVAL(le->le_e3));
	default:
		ld_fatal(ld, "internal: unknown ldscript expression op");
	}

	return (0);
}

void
ld_exp_dump(struct ld *ld, struct ld_exp *le)
{

	assert(le != NULL);

	if (le->le_par)
		printf("(");

	switch (le->le_op) {
	case LEOP_ABS:
		printf("ABS(");
		_EXP_DUMP(le->le_e1);
		printf(")");
		break;
	case LEOP_ADD:
		_EXP_DUMP(le->le_e1);
		printf(" + ");
		_EXP_DUMP(le->le_e2);
		break;
	case LEOP_ADDR:
		printf("ADDR(");
		_EXP_DUMP(le->le_e1);
		printf(")");
		break;
	case LEOP_ALIGN:
	case LEOP_BLOCK:
		printf("ALIGN(");
		_EXP_DUMP(le->le_e1);
		if (le->le_e2 != NULL) {
			printf(", ");
			_EXP_DUMP(le->le_e2);
		}
		printf(")");
		break;
	case LEOP_ALIGNOF:
		printf("ALIGNOF(");
		_EXP_DUMP(le->le_e1);
		printf(")");
		break;
	case LEOP_AND:
		_EXP_DUMP(le->le_e1);
		printf(" & ");
		_EXP_DUMP(le->le_e2);
		break;
	case LEOP_ASSIGN:
		printf("0x%jx", (uintmax_t)  le->le_assign->lda_res);
		break;
	case LEOP_CONSTANT:
		printf("0x%jx", (uintmax_t) le->le_val);
		break;
	case LEOP_DIV:
		_EXP_DUMP(le->le_e1);
		printf(" / ");
		_EXP_DUMP(le->le_e2);
		break;
	case LEOP_DSA:
		printf("DATA_SEGMENT_ALIGN(");
		_EXP_DUMP(le->le_e1);
		printf(", ");
		_EXP_DUMP(le->le_e2);
		printf(")");
		break;
	case LEOP_DSE:
		printf("DATA_SEGMENT_END(");
		_EXP_DUMP(le->le_e1);
		printf(")");
		break;
	case LEOP_DSRE:
		printf("DATA_SEGMENT_RELRO_END(");
		_EXP_DUMP(le->le_e1);
		printf(", ");
		_EXP_DUMP(le->le_e2);
		printf(")");
		break;
	case LEOP_DEFINED:
		printf("DEFINED(");
		_EXP_DUMP(le->le_e1);
		printf(")");
		break;
	case LEOP_EQUAL:
		_EXP_DUMP(le->le_e1);
		printf(" == ");
		_EXP_DUMP(le->le_e2);
		break;
	case LEOP_GE:
		_EXP_DUMP(le->le_e1);
		printf(" >= ");
		_EXP_DUMP(le->le_e2);
		break;
	case LEOP_GREATER:
		_EXP_DUMP(le->le_e1);
		printf(" > ");
		_EXP_DUMP(le->le_e2);
		break;
	case LEOP_LENGTH:
		printf("LENGTH(");
		_EXP_DUMP(le->le_e1);
		printf(")");
		break;
	case LEOP_LOADADDR:
		printf("LOADADDR(");
		_EXP_DUMP(le->le_e1);
		printf(")");
		break;
	case LEOP_LOGICAL_AND:
		_EXP_DUMP(le->le_e1);
		printf(" && ");
		_EXP_DUMP(le->le_e2);
		break;
	case LEOP_LOGICAL_OR:
		_EXP_DUMP(le->le_e1);
		printf(" || ");
		_EXP_DUMP(le->le_e2);
		break;
	case LEOP_LSHIFT:
		_EXP_DUMP(le->le_e1);
		printf(" << ");
		_EXP_DUMP(le->le_e2);
		break;
	case LEOP_MAX:
		printf("MAX(");
		_EXP_DUMP(le->le_e1);
		printf(", ");
		_EXP_DUMP(le->le_e2);
		printf(")");
		break;
	case LEOP_MIN:
		printf("MIN(");
		_EXP_DUMP(le->le_e1);
		printf(", ");
		_EXP_DUMP(le->le_e2);
		printf(")");
		break;
	case LEOP_MINUS:
		printf("-");
		_EXP_DUMP(le->le_e1);
		break;
	case LEOP_MOD:
		_EXP_DUMP(le->le_e1);
		printf(" %% ");
		_EXP_DUMP(le->le_e2);
		break;
	case LEOP_MUL:
		_EXP_DUMP(le->le_e1);
		printf(" * ");
		_EXP_DUMP(le->le_e2);
		break;
	case LEOP_NE:
		_EXP_DUMP(le->le_e1);
		printf(" != ");
		_EXP_DUMP(le->le_e2);
		break;
	case LEOP_NEGATION:
		printf("~");
		_EXP_DUMP(le->le_e1);
		break;
	case LEOP_NEXT:
		printf("NEXT(");
		_EXP_DUMP(le->le_e1);
		printf(")");
		break;
	case LEOP_NOT:
		printf("!");
		_EXP_DUMP(le->le_e1);
		break;
	case LEOP_OR:
		_EXP_DUMP(le->le_e1);
		printf(" | ");
		_EXP_DUMP(le->le_e2);
		break;
	case LEOP_ORIGIN:
		printf("ORIGIN(");
		_EXP_DUMP(le->le_e1);
		printf(")");
		break;
	case LEOP_RSHIFT:
		_EXP_DUMP(le->le_e1);
		printf(" >> ");
		_EXP_DUMP(le->le_e2);
		break;
	case LEOP_SEGMENT_START:
		printf("SEGMENT_START(");
		_EXP_DUMP(le->le_e1);
		printf(", ");
		_EXP_DUMP(le->le_e2);
		printf(")");
		break;
	case LEOP_SIZEOF:
		printf("SIZEOF(");
		_EXP_DUMP(le->le_e1);
		printf(")");
		break;
	case LEOP_SIZEOF_HEADERS:
		printf("SIZEOF_HEADERS");
		break;
	case LEOP_SUBSTRACT:
		_EXP_DUMP(le->le_e1);
		printf(" - ");
		_EXP_DUMP(le->le_e2);
		break;
	case LEOP_SYMBOL:
		printf("%s", le->le_name);
		break;
	case LEOP_SYMBOLIC_CONSTANT:
		printf("0x%jx",
		    (uintmax_t) _symbolic_constant(ld, le->le_name));
		break;
	case LEOP_TRINARY:
		_EXP_DUMP(le->le_e1);
		printf(" ? ");
		_EXP_DUMP(le->le_e2);
		printf(" : ");
		_EXP_DUMP(le->le_e3);
		break;
	default:
		ld_fatal(ld, "internal: unknown ldscript expression op");
	}

	if (le->le_par)
		printf(")");
}

static struct ld_exp *
_alloc_exp(struct ld *ld)
{
	struct ld_exp *le;

	if ((le = calloc(1, sizeof(*le))) == NULL)
		ld_fatal_std(ld, "calloc");

	return (le);
}

static int64_t
_assignment(struct ld *ld, struct ld_exp *le)
{
	struct ld_exp *var;

	assert(le->le_assign != NULL);
	ld_script_process_assign(ld, le->le_assign);
	var = le->le_assign->lda_var;
	return (ld_script_variable_value(ld, var->le_name));
}

static int64_t
_func_addr(struct ld *ld, struct ld_exp *le)
{

	/* TODO */
	(void) ld; (void) le;
	return (0);
}

static int64_t
_func_align(struct ld *ld, struct ld_exp *le)
{
	struct ld_state *ls;

	ls = &ld->ld_state;
	if (le->le_e2 != NULL)
		return (roundup(_EXP_EVAL(le->le_e1), _EXP_EVAL(le->le_e2)));
	else
		return (roundup(ls->ls_loc_counter, _EXP_EVAL(le->le_e1)));
}

static int64_t
_func_alignof(struct ld *ld, struct ld_exp *le)
{

	/* TODO */
	(void) ld; (void) le;
	return (0);
}

static int64_t
_func_data_segment_align(struct ld *ld, struct ld_exp *le)
{
	struct ld_state *ls; 
	uint64_t maxpagesize;
	/* uint64_t commonpagesize; */

	/*
	 * TODO: test if align to common page size use less number
	 * of pages.
	 */
	ls = &ld->ld_state;
	maxpagesize = _EXP_EVAL(le->le_e1);
	/* commonpagesize = _EXP_EVAL(le->le_e2); */

	return (roundup(ls->ls_loc_counter, maxpagesize) +
	    (ls->ls_loc_counter & (maxpagesize - 1)));
}

static int64_t
_func_data_segment_end(struct ld *ld, struct ld_exp *le)
{

	return (_EXP_EVAL(le->le_e1));
}

static int64_t
_func_data_segment_relro_end(struct ld *ld, struct ld_exp *le)
{

	/* TODO */
	(void) ld; (void) le;
	return (0);
}

static int64_t
_func_defined(struct ld *ld, struct ld_exp *le)
{

	/* TODO */
	(void) ld; (void) le;
	return (0);
}

static int64_t
_func_length(struct ld *ld, struct ld_exp *le)
{

	/* TODO */
	(void) ld; (void) le;
	return (0);
}

static int64_t
_func_loadaddr(struct ld *ld, struct ld_exp *le)
{

	/* TODO */
	(void) ld; (void) le;
	return (0);
}

static int64_t
_func_max(struct ld *ld, struct ld_exp *le)
{
	uint64_t val1, val2;

	val1 = _EXP_EVAL(le->le_e1);
	val2 = _EXP_EVAL(le->le_e2);

	return (val1 > val2 ? val1 : val2);
}

static int64_t
_func_min(struct ld *ld, struct ld_exp *le)
{
	uint64_t val1, val2;

	val1 = _EXP_EVAL(le->le_e1);
	val2 = _EXP_EVAL(le->le_e2);

	return (val1 > val2 ? val2 : val1);
}

static int64_t
_func_next(struct ld *ld, struct ld_exp *le)
{

	/* TODO */
	(void) ld; (void) le;
	return (0);
}

static int64_t
_func_origin(struct ld *ld, struct ld_exp *le)
{

	/* TODO */
	(void) ld; (void) le;
	return (0);
}

static int64_t
_func_segment_start(struct ld *ld, struct ld_exp *le)
{

	/* TODO */
	(void) ld; (void) le;
	return (0);
}

static int64_t
_func_sizeof(struct ld *ld, struct ld_exp *le)
{

	/* TODO */
	(void) ld; (void) le;
	return (0);
}

static int64_t
_func_sizeof_headers(struct ld *ld)
{

	return (ld_layout_calc_header_size(ld));
}

static int64_t
_symbol_val(struct ld *ld, char *name)
{

	return (ld_script_variable_value(ld, name));
}

static int64_t
_symbolic_constant(struct ld *ld, const char *name)
{

	if (ld->ld_arch == NULL)
		return (0);

	if (strcmp(name, "COMMONPAGESIZE") == 0)
		return (ld->ld_arch->get_common_page_size(ld));
	else if (strcmp(name, "MAXPAGESIZE") == 0)
		return (ld->ld_arch->get_max_page_size(ld));

	return (0);
}
