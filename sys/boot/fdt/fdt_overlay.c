/*-
 * Copyright (c) 2015 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <libfdt.h>

#include "fdt_overlay.h"

/*
 * Get max phandle
 */
static uint32_t
fdt_max_phandle(void *fdtp)
{
	int o, depth;
	uint32_t max_phandle, phandle;

	depth = 1;
	o = fdt_path_offset(fdtp, "/");
	max_phandle = fdt_get_phandle(fdtp, o);
	for (depth = 0; (o >= 0) && (depth >= 0); o = fdt_next_node(fdtp, o, &depth)) {
		phandle = fdt_get_phandle(fdtp, o);
		if (max_phandle < phandle)
			max_phandle = phandle;
	}

	return max_phandle;
}

/*
 * Returns exact memory location specified by fixup in format
 * /path/to/node:property:offset
 */
static void *
fdt_get_fixup_location(void *fdtp, const char *fixup)
{
	char *path, *prop, *offsetp, *endp;
	int prop_offset, o, proplen;
	void  *result;

	result = 0;

	path = strdup(fixup);
	prop = strchr(path, ':');
	if (prop == NULL) {
		printf("missing property part in \"%s\"\n", fixup);
		result = NULL;
		goto out;
	}

	*prop = 0;
	prop++;

	offsetp = strchr(prop, ':');
	if (offsetp == NULL) {
		printf("missing offset part in \"%s\"\n", fixup);
		result = NULL;
		goto out;
	}

	*offsetp = 0;
	offsetp++;

	prop_offset = strtoul(offsetp, &endp, 10);
	if (*endp != '\0') {
		printf("\"%s\" is not valid number\n", offsetp);
		result = NULL;
		goto out;
	}

	o = fdt_path_offset(fdtp, path);
	if (o < 0) {
		printf("path \"%s\" not found\n", path);
		result = NULL;
		goto out;
	}

	result = fdt_getprop_w(fdtp, o, prop, &proplen);
	if (result == NULL){
		printf("property \"%s\" not found in  \"%s\" node\n", prop, path);
		result = NULL;
		goto out;
	}

	if (proplen < prop_offset + sizeof(uint32_t)) {
		printf("%s: property length is too small for fixup\n", fixup);
		result = NULL;
		goto out;
	}

	result = (char*)result + prop_offset;

out:
	free(path);
	return (result);
}

/*
 * Process one entry in __fixups__ { } node
 * @fixups is property value, array of NUL-terminated strings
 *   with fixup locations
 * @fixups_len length of the fixups array in bytes
 * @phandle is value for these locations
 */
static int
fdt_do_one_fixup(void *fdtp, const char *fixups, int fixups_len, int phandle)
{
	void *fixup_pos;
	uint32_t val;

	val = cpu_to_fdt32(phandle);

	while (fixups_len > 0) {
		fixup_pos = fdt_get_fixup_location(fdtp, fixups);
		if (fixup_pos != NULL)
			memcpy(fixup_pos, &val, sizeof(val));

		fixups_len -= strlen(fixups) + 1;
		fixups += strlen(fixups) + 1;
	}

	return (0);
}

/*
 * Increase u32 value at pos by offset
 */
static void
fdt_increase_u32(void *pos, uint32_t offset)
{
	uint32_t val;

	memcpy(&val, pos,  sizeof(val));
	val = cpu_to_fdt32(fdt32_to_cpu(val) + offset);
	memcpy(pos, &val, sizeof(val));
}

/*
 * Process local fixups
 * @fixups is property value, array of NUL-terminated strings
 *   with fixup locations
 * @fixups_len length of the fixups array in bytes
 * @offset value these locations should be increased by
 */
static int
fdt_do_local_fixup(void *fdtp, const char *fixups, int fixups_len, int offset)
{
	void *fixup_pos;

	while (fixups_len > 0) {
		fixup_pos = fdt_get_fixup_location(fdtp, fixups);
		if (fixup_pos != NULL)
			fdt_increase_u32(fixup_pos, offset);

		fixups_len -= strlen(fixups) + 1;
		fixups += strlen(fixups) + 1;
	}

	return (0);
}

/*
 * Increase node phandle by phandle_offset
 */
static void
fdt_increase_phandle(void *fdtp, int node_offset, uint32_t phandle_offset)
{
	int proplen;
	void *phandle_pos, *node_pos;

	node_pos = (char*)fdtp + node_offset;

	phandle_pos = fdt_getprop_w(fdtp, node_offset, "phandle", &proplen);
	if (phandle_pos)
		fdt_increase_u32(phandle_pos, phandle_offset);
	phandle_pos = fdt_getprop_w(fdtp, node_offset, "linux,phandle", &proplen);
	if (phandle_pos)
		fdt_increase_u32(phandle_pos, phandle_offset);
}

/*
 * Increase all phandles by offset
 */
static void
fdt_increase_phandles(void *fdtp, uint32_t offset)
{
	int o, depth;

	o = fdt_path_offset(fdtp, "/");
	for (depth = 0; (o >= 0) && (depth >= 0); o = fdt_next_node(fdtp, o, &depth)) {
		fdt_increase_phandle(fdtp, o, offset);
	}
}

/*
 * Overlay one node defined by <overlay_fdtp, overlay_o> over <main_fdtp, target_o>
 */
static void
fdt_overlay_node(void *main_fdtp, int target_o, void *overlay_fdtp, int overlay_o)
{
	int len, o, depth;
	const char *name;
	const void *val;
	int target_subnode_o;

	/* Overlay properties */
	for (o = fdt_first_property_offset(overlay_fdtp, overlay_o);
	    o >= 0; o = fdt_next_property_offset(overlay_fdtp, o)) {
		val = fdt_getprop_by_offset(overlay_fdtp, o, &name, &len);
		if (val)
			fdt_setprop(main_fdtp, target_o, name, val, len);
	}

	/* Now overlay nodes */
	o = overlay_o;
        for (depth = 0; (o >= 0) && (depth >= 0);
	    o = fdt_next_node(overlay_fdtp, o, &depth)) {
		if (depth != 1)
			continue;
		/* Check if there is node with the same name */
		name = fdt_get_name(overlay_fdtp, o, NULL);
		target_subnode_o = fdt_subnode_offset(main_fdtp, target_o, name);
		if (target_subnode_o < 0) {
			/* create new subnode and run merge recursively */
			target_subnode_o = fdt_add_subnode(main_fdtp, target_o, name);
			if (target_subnode_o < 0) {
				printf("failed to create subnode \"%s\": %d\n",
				    name, target_subnode_o);
				return;
			}
		}

		fdt_overlay_node(main_fdtp, target_subnode_o,
		    overlay_fdtp, o);
	}
}

/*
 * Apply one overlay fragment
 */
static void
fdt_apply_fragment(void *main_fdtp, void *overlay_fdtp, int fragment_o)
{
	uint32_t target;
	const char *target_path;
	const void *val;
	int target_node_o, overlay_node_o;

	target_node_o = -1;
	val = fdt_getprop(overlay_fdtp, fragment_o, "target", NULL);
	if (val) {
		memcpy(&target, val, sizeof(target));
		target = fdt32_to_cpu(target);
		target_node_o = fdt_node_offset_by_phandle(main_fdtp, target);
		if (target_node_o < 0) {
			printf("failed to find target %04x\n", target);
			return;
		}
	}

	if (target_node_o < 0) {
		target_path = fdt_getprop(overlay_fdtp, fragment_o, "target-path", NULL);
		if (target_path == NULL)
			return;

		target_node_o = fdt_path_offset(main_fdtp, target_path);
		if (target_node_o < 0) {
			printf("failed to find target-path %s\n", target_path);
			return;
		}
	}

	if (target_node_o < 0)
		return;

	overlay_node_o = fdt_subnode_offset(overlay_fdtp, fragment_o, "__overlay__");
	if (overlay_node_o < 0) {
		printf("missing __overlay__ sub-node\n");
		return;
	}

	fdt_overlay_node(main_fdtp, target_node_o, overlay_fdtp, overlay_node_o);
}

/*
 * Handle __fixups__ node in overlay DTB
 */
static int
fdt_overlay_do_fixups(void *main_fdtp, void *overlay_fdtp)
{
	int main_symbols_o, symbol_o, overlay_fixups_o;
	int fixup_prop_o;
	int len;
	const char *fixups, *name;
	const char *symbol_path;
	uint32_t phandle;

	main_symbols_o = fdt_path_offset(main_fdtp, "/__symbols__");
	overlay_fixups_o = fdt_path_offset(overlay_fdtp, "/__fixups__");

	if (main_symbols_o < 0)
		return (-1);
	if (overlay_fixups_o < 0)
		return (-1);

	for (fixup_prop_o = fdt_first_property_offset(overlay_fdtp, overlay_fixups_o);
	    fixup_prop_o >= 0;
	    fixup_prop_o = fdt_next_property_offset(overlay_fdtp, fixup_prop_o)) {
		fixups = fdt_getprop_by_offset(overlay_fdtp, fixup_prop_o, &name, &len);
		symbol_path = fdt_getprop(main_fdtp, main_symbols_o, name, NULL);
		if (symbol_path == NULL) {
			printf("couldn't find \"%s\" symbol in main dtb\n", name);
			return (-1);
		}
		symbol_o = fdt_path_offset(main_fdtp, symbol_path);
		if (symbol_o < 0) {
			printf("couldn't find \"%s\" path in main dtb\n", symbol_path);
			return (-1);
		}
		phandle = fdt_get_phandle(main_fdtp, symbol_o);
		if (fdt_do_one_fixup(overlay_fdtp, fixups, len, phandle) < 0)
			return (-1);
	}

	return (0);
}

/*
 * Handle __local_fixups__ node in overlay DTB
 */
static int
fdt_overlay_do_local_fixups(void *main_fdtp, void *overlay_fdtp)
{
	int overlay_local_fixups_o;
	int len;
	const char *fixups;
	uint32_t phandle_offset;

	overlay_local_fixups_o = fdt_path_offset(overlay_fdtp, "/__local_fixups__");

	if (overlay_local_fixups_o < 0)
		return (-1);

	phandle_offset = fdt_max_phandle(main_fdtp);
	fdt_increase_phandles(overlay_fdtp, phandle_offset);
	fixups = fdt_getprop_w(overlay_fdtp, overlay_local_fixups_o, "fixup", &len);
	if (fixups) {
		if (fdt_do_local_fixup(overlay_fdtp, fixups, len, phandle_offset) < 0)
			return (-1);
	}

	return (0);
}

/*
 * Apply all fragments to main DTB
 */
static int
fdt_overlay_apply_fragments(void *main_fdtp, void *overlay_fdtp)
{
	int o, depth;

	o = fdt_path_offset(overlay_fdtp, "/");
	for (depth = 0; (o >= 0) && (depth >= 0); o = fdt_next_node(overlay_fdtp, o, &depth)) {
		if (depth != 1)
			continue;

		fdt_apply_fragment(main_fdtp, overlay_fdtp, o);
	}

	return (0);
}

int
fdt_overlay_apply(void *main_fdtp, void *overlay_fdtp, size_t overlay_length)
{
	void *overlay_copy;
	int rv;

	rv = 0;

	/* We modify overlay in-place, so we need writable copy */
	overlay_copy = malloc(overlay_length);
	if (overlay_copy == NULL) {
		printf("failed to allocate memory for overlay copy\n");
		return (-1);
	}

	memcpy(overlay_copy, overlay_fdtp, overlay_length);

	if (fdt_overlay_do_fixups(main_fdtp, overlay_copy) < 0) {
		printf("failed to perform fixups in overlay\n");
		rv = -1;
		goto out;
	}

	if (fdt_overlay_do_local_fixups(main_fdtp, overlay_copy) < 0) {
		printf("failed to perform local fixups in overlay\n");
		rv = -1;
		goto out;
	}

	if (fdt_overlay_apply_fragments(main_fdtp, overlay_copy) < 0) {
		printf("failed to apply fragments\n");
		rv = -1;
	}

out:
	free(overlay_copy);

	return (rv);
}
