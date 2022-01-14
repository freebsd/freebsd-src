/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 John H. Baldwin <jhb@FreeBSD.org>
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
 * $FreeBSD$
 */

#ifndef __CONFIG_H__
#define	__CONFIG_H__

#include <sys/nv.h>

/*-
 * Manages a configuration database backed by an nv(9) list.
 *
 * The database only stores string values.  Callers should parse
 * values into other types if needed.  String values can reference
 * other configuration variables using a '%(name)' syntax.  In this
 * case, the name must be the the full path of the configuration
 * variable.  The % character can be escaped with a preceding \ to
 * avoid expansion.  Any \ characters must be escaped.
 *
 * Configuration variables are stored in a tree.  The full path of a
 * variable is specified as a dot-separated name similar to sysctl(8)
 * OIDs.
 */ 

/*
 * Fetches the value of a configuration variable.  If the "raw" value
 * contains references to other configuration variables, this function
 * expands those references and returns a pointer to the parsed
 * string.  The string's storage is only stable until the next call to
 * this function.
 *
 * If no node is found, returns NULL.
 *
 * If 'parent' is NULL, 'name' is assumed to be a top-level variable.
 */
const char *get_config_value_node(const nvlist_t *parent, const char *name);

/*
 * Similar to get_config_value_node but expects a full path to the
 * leaf node.
 */
const char *get_config_value(const char *path);

/* Initializes the tree to an empty state. */
void	init_config(void);

/*
 * Creates an existing configuration node via a dot-separated OID
 * path.  Will fail if the path names an existing leaf configuration
 * variable.  If the node already exists, this returns a pointer to
 * the existing node.
 */
nvlist_t *create_config_node(const char *path);

/*
 * Looks for an existing configuration node via a dot-separated OID
 * path.  Will fail if the path names an existing leaf configuration
 * variable.
 */
nvlist_t *find_config_node(const char *path);

/*
 * Similar to the above, but treats the path relative to an existing
 * 'parent' node rather than as an absolute path.
 */
nvlist_t *create_relative_config_node(nvlist_t *parent, const char *path);
nvlist_t *find_relative_config_node(nvlist_t *parent, const char *path);

/*
 * Adds or replaces the value of the specified variable.
 *
 * If 'parent' is NULL, 'name' is assumed to be a top-level variable.
 */
void	set_config_value_node(nvlist_t *parent, const char *name,
    const char *value);

/*
 * Similar to set_config_value_node but only sets value if it's unset yet.
 */
void 	set_config_value_node_if_unset(nvlist_t *const parent,
    const char *const name, const char *const value);

/*
 * Similar to set_config_value_node but expects a full path to the
 * leaf node.
 */
void	set_config_value(const char *path, const char *value);

/*
 * Similar to set_config_value but only sets the value if it's unset yet.
 */
void 	set_config_value_if_unset(const char *const path,
    const char *const value);

/* Convenience wrappers for boolean variables. */
bool	get_config_bool(const char *path);
bool	get_config_bool_node(const nvlist_t *parent, const char *name);
bool	get_config_bool_default(const char *path, bool def);
bool	get_config_bool_node_default(const nvlist_t *parent, const char *name,
    bool def);
void	set_config_bool(const char *path, bool value);
void	set_config_bool_node(nvlist_t *parent, const char *name, bool value);

void	dump_config(void);

#endif /* !__CONFIG_H__ */
