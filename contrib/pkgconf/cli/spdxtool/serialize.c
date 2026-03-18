/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *​ Copyright (c) 2025 The FreeBSD Foundation
 *​
 *​ Portions of this software were developed by
 * Tuukka Pasanen <tuukka.pasanen@ilmi.fi> under sponsorship from
 * the FreeBSD Foundation
 */

#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "core.h"
#include "serialize.h"

static inline void
serialize_add_indent(pkgconf_buffer_t *buffer, unsigned int level)
{
	for (; level; level--)
		pkgconf_buffer_append(buffer, "    ");
}

static inline void
serialize_next_line(pkgconf_buffer_t *buffer, bool more)
{
	if (more)
		pkgconf_buffer_push_byte(buffer, ',');

	pkgconf_buffer_push_byte(buffer, '\n');
}

static inline void
serialize_begin_object(pkgconf_buffer_t *buffer, char ch, unsigned int level)
{
	serialize_add_indent(buffer, level);
	pkgconf_buffer_push_byte(buffer, ch);
	serialize_next_line(buffer, false);
}

static inline void
serialize_end_object(pkgconf_buffer_t *buffer, char ch, unsigned int level, bool more)
{
	serialize_add_indent(buffer, level);
	pkgconf_buffer_push_byte(buffer, ch);
	serialize_next_line(buffer, more);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_serialize_parm_and_string(pkgconf_buffer_t *buffer, char *parm, char *string, unsigned int level, bool more)
 *
 *    Add paramter, string and optional comma to buffer
 *
 *    :param pkgconf_buffer_t *buffer: Buffer to add.
 *    :param char *param: Parameter name
 *    :param char *string: String to add
 *    :param unsigned int level: Indent level
 *    :param bool more: true if more fields are expected, else false
 *    :return: nothing
 */
void
spdxtool_serialize_parm_and_string(pkgconf_buffer_t *buffer, char *parm, char *string, unsigned int level, bool more)
{
	serialize_add_indent(buffer, level);
	pkgconf_buffer_append_fmt(buffer, "\"%s\": \"%s\"", parm, string);
	serialize_next_line(buffer, more);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_serialize_parm_and_char(pkgconf_buffer_t *buffer, char *parm, char ch, unsigned int level, bool more)
 *
 *    Add paramter, char and optional comma to buffer
 *
 *    :param pkgconf_buffer_t *buffer: Buffer to add.
 *    :param char *param: Parameter name
 *    :param char ch: Char to add
 *    :param unsigned int level: Indent level
 *    :param bool more: true if more fields are expected, else false
 *    :return: nothing
 */
void
spdxtool_serialize_parm_and_char(pkgconf_buffer_t *buffer, char *parm, char ch, unsigned int level, bool more)
{
	serialize_add_indent(buffer, level);
	pkgconf_buffer_append_fmt(buffer, "\"%s\": %c", parm, ch);
	serialize_next_line(buffer, more);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_serialize_parm_and_int(pkgconf_buffer_t *buffer, char *parm, int integer, unsigned int level, bool more)
 *
 *    Add paramter, integer and optional comma to buffer
 *
 *    :param pkgconf_buffer_t *buffer: Buffer to add.
 *    :param char *param: Parameter name
 *    :param int integer: Int to add
 *    :param unsigned int level: Indent level
 *    :param bool more: true if more fields are expected, else false
 *    :return: nothing
 */
void
spdxtool_serialize_parm_and_int(pkgconf_buffer_t *buffer, char *parm, int integer, unsigned int level, bool more)
{
	serialize_add_indent(buffer, level);
	pkgconf_buffer_append_fmt(buffer, "\"%s\": %d", parm, integer);
	serialize_next_line(buffer, more);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_serialize_string(pkgconf_buffer_t *buffer, char *string, unsigned int level, bool more)
 *
 *    Add just string.
 *
 *    :param pkgconf_buffer_t *buffer: Buffer to add.
 *    :param char *param: Parameter name
 *    :param char *ch: String to add
 *    :param unsigned int: level Indent level
 *    :param bool more: true if more fields are expected, else false
 *    :return: nothing
 */
void
spdxtool_serialize_string(pkgconf_buffer_t *buffer, char *string, unsigned int level, bool more)
{
	serialize_add_indent(buffer, level);
	pkgconf_buffer_push_byte(buffer, '"');
	pkgconf_buffer_append(buffer, string);
	pkgconf_buffer_push_byte(buffer, '"');
	serialize_next_line(buffer, more);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_serialize_obj_start(pkgconf_buffer_t *buffer, unsigned int level)
 *
 *    Start JSON object to buffer
 *
 *    :param pkgconf_buffer_t *buffer: Buffer to add.
 *    :param unsigned int level: Indent level
 *    :return: nothing
 */
void
spdxtool_serialize_obj_start(pkgconf_buffer_t *buffer, unsigned int level)
{
	serialize_begin_object(buffer, '{', level);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_serialize_obj_end(pkgconf_buffer_t *buffer, unsigned int level, bool more)
 *
 *    End JSON object to buffer
 *
 *    :param pkgconf_buffer_t *buffer: Buffer to add.
 *    :param unsigned int level: Level which is added
 *    :param bool more: true if more fields are expected, else false
 *    :return: nothing
 */
void
spdxtool_serialize_obj_end(pkgconf_buffer_t *buffer, unsigned int level, bool more)
{
	serialize_end_object(buffer, '}', level, more);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_serialize_obj_start(pkgconf_buffer_t *buffer, unsigned int level)
 *
 *    Start JSON array to buffer
 *
 *    :param pkgconf_buffer_t *buffer: Buffer to add.
 *    :param unsigned int level: Level which is added
 *    :return: nothing
 */
void
spdxtool_serialize_array_start(pkgconf_buffer_t *buffer, unsigned int level)
{
	serialize_begin_object(buffer, '[', level);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_serialize_array_end(pkgconf_buffer_t *buffer, unsigned int level, bool more)
 *
 *    End JSON array to buffer
 *
 *    :param pkgconf_buffer_t *buffer: Buffer to add.
 *    :param unsigned int level: Level which is added
 *    :param bool more: true if more fields are expected, else false
 *    :return: nothing
 */
void
spdxtool_serialize_array_end(pkgconf_buffer_t *buffer, unsigned int level, bool more)
{
	serialize_end_object(buffer, ']', level, more);
}
