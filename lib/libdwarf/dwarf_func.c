/*-
 * Copyright (c) 2008-2009, 2011, Juniper Networks, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	JNPR: dwarf_func.c 336441 2009-10-17 09:19:54Z deo
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdlib.h>
#include <string.h>
#include <libdwarf.h>
#include <_libdwarf.h>

static void
dwarf_add_function(Dwarf_Debug dbg, Dwarf_Func func)
{

	STAILQ_INSERT_TAIL(&dbg->dbg_func, func, func_next);
}

int
dwarf_function_get_addr_range(Dwarf_Func f, Dwarf_Addr *low_pc,
    Dwarf_Addr *high_pc)
{

	*low_pc = f->func_low_pc;
	*high_pc = f->func_high_pc;
	return 0;
}

int
dwarf_inlined_function_get_addr_range(Dwarf_Inlined_Func f, Dwarf_Addr *low_pc,
    Dwarf_Addr *high_pc)
{

	*low_pc = f->ifunc_low_pc;
	*high_pc = f->ifunc_high_pc;
	return 0;
}

int
dwarf_function_is_inlined(Dwarf_Func f)
{

	if (f->func_is_inlined == DW_INL_inlined ||
	    f->func_is_inlined == DW_INL_declared_inlined)
		return 1;
	else
		return 0;
}

Dwarf_Func
dwarf_find_function_by_name(Dwarf_Debug dbg, const char *name)
{
	/* XXX: replace with a fast version */

	Dwarf_Func func;
	STAILQ_FOREACH(func, &dbg->dbg_func, func_next) {
		if (strcmp(name, func->func_name) == 0)
			return func;
	}
	return NULL;
}

Dwarf_Func
dwarf_find_function_by_offset(Dwarf_Debug dbg, Dwarf_Off off)
{

	Dwarf_Func func;
	Dwarf_Die die;
	/* printf("look for %llx\n", off); */
	STAILQ_FOREACH(func, &dbg->dbg_func, func_next) {
		die = func->func_die;
		if ((off_t)die->die_offset == off) {
			return func;
		}
	}
	return NULL;
}

void
dwarf_build_function_table(Dwarf_Debug dbg)
{
	Dwarf_CU cu;
	Dwarf_AttrValue av;
	Dwarf_Die die, origin_die;
	Dwarf_Func func, origin_func;
	Dwarf_Inlined_Func ifunc;
	unsigned long long offset;
	const char *name;
	Dwarf_Error error;

	/*
	 * find out all the functions
	 */
	STAILQ_FOREACH(cu, &dbg->dbg_cu, cu_next) {
		STAILQ_FOREACH(die, &cu->cu_die, die_next) {
			if (die->die_a->a_tag == DW_TAG_subprogram) {
				/*
				 * Some function has multiple entries, i.e.
				 * if a function is inlined, it has many
				 * abstract/concrete instances, the abstract
				 * instances are with DW_TAG_subprogram.
				 */
				dwarf_attrval_string(die, DW_AT_name, &name,
				    &error);
				func = dwarf_find_function_by_name(dbg, name);
				if (func == NULL) {
					func = malloc(
					    sizeof(struct _Dwarf_Func));
					DWARF_ASSERT(func);

					func->func_die = die;
					func->func_name = name;
					STAILQ_INIT(
					    &func->func_inlined_instances);

					dwarf_add_function(dbg, func);
					STAILQ_FOREACH(av, &die->die_attrval,
					    av_next) {
						switch (av->av_attrib) {
						case DW_AT_low_pc:
							func->func_low_pc =
							    av->u[0].u64;
							break;
						case DW_AT_high_pc:
							func->func_high_pc =
							    av->u[0].u64;
							break;
						case DW_AT_inline:
							func->func_is_inlined =
							    av->u[0].u64;
							break;
						}
					}
				}
			}
		}
	}

	/*
	 * Now check the concrete inlined instances.
	 */
	STAILQ_FOREACH(cu, &dbg->dbg_cu, cu_next) {
		STAILQ_FOREACH(die, &cu->cu_die, die_next) {
			if (die->die_a->a_tag == DW_TAG_inlined_subroutine) {
				ifunc = malloc(
				    sizeof(struct _Dwarf_Inlined_Func));
				DWARF_ASSERT(ifunc);
				STAILQ_FOREACH(av, &die->die_attrval, av_next) {
					switch (av->av_attrib) {
					case DW_AT_abstract_origin:
						offset = av->u[0].u64 +
						    die->die_cu->cu_offset;
						origin_die = dwarf_die_find(
							die, offset);
						DWARF_ASSERT(origin_die != 0);

						/*
						 * the abstract origin must
						 * have been merged with
						 * another die
						 */
						dwarf_attrval_string(
						    origin_die, DW_AT_name,
						    &name, &error);
						origin_func =
						    dwarf_find_function_by_name
						    (dbg, name);
						DWARF_ASSERT(origin_func != 0);

						STAILQ_INSERT_TAIL(
						    &origin_func->
						    func_inlined_instances,
						    ifunc, ifunc_next);

						break;
					case DW_AT_low_pc:
						ifunc->ifunc_low_pc =
						    av->u[0].u64;
						break;
					case DW_AT_high_pc:
						ifunc->ifunc_high_pc =
						    av->u[0].u64;
						break;
					}
				}
			}
		}
	}
}

void
dwarf_function_iterate_inlined_instance(Dwarf_Func func,
    Dwarf_Inlined_Callback f, void *data)
{
	Dwarf_Inlined_Func ifunc;

	if (!dwarf_function_is_inlined(func))
		return;
	STAILQ_FOREACH(ifunc, &func->func_inlined_instances, ifunc_next) {
		f(ifunc, data);
	}
}
