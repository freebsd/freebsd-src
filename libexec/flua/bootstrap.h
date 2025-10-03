/*-
 * Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef FLUA_BOOTSTRAP_H
#define	FLUA_BOOTSTRAP_H

#ifdef BOOTSTRAPPING
#include <sys/linker_set.h>

#include <lauxlib.h>

SET_DECLARE(flua_module_set, const luaL_Reg);
#define	FLUA_MODULE_DEF(ident, modname, openfn)			\
	static const luaL_Reg ident = {	modname, openfn };	\
	DATA_SET(flua_module_set, ident)

#define	FLUA_MODULE_NAMED(mod, name)	\
	FLUA_MODULE_DEF(module_ ## mod, name, luaopen_ ## mod)
#define	FLUA_MODULE(mod)		\
	FLUA_MODULE_DEF(module_ ## mod, #mod, luaopen_ ## mod)
#else	/* !BOOTSTRAPPING */
#define	FLUA_MODULE_DEF(ident, modname, openfn)
#define	FLUA_MODULE_NAMED(mod, name)
#define	FLUA_MODULE(modname)
#endif	/* BOOTSTRAPPING */

#endif	/* FLUA_BOOTSTRAP_H */
