/* $FreeBSD$ */

/*-
 * Copyright (c) 2011 Hans Petter Selasky. All rights reserved.
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysexits.h>
#include <err.h>
#include <string.h>

#include <sys/queue.h>

#include "bus_sections.h"

#define	MAX_STRING	64

struct format_info;
typedef TAILQ_HEAD(,format_info) format_info_head_t;
typedef TAILQ_ENTRY(format_info) format_info_entry_t;

static format_info_head_t format_head = TAILQ_HEAD_INITIALIZER(format_head);

struct format_info {
	format_info_entry_t entry;
	format_info_head_t fields;
	char	name[MAX_STRING];
	uint16_t bit_offset;
	uint16_t bit_size;
};

static struct format_info *
format_info_new(char *pstr, uint16_t bo, uint16_t bs)
{
	struct format_info *pfi;

	pfi = malloc(sizeof(*pfi));
	if (pfi == NULL)
		errx(EX_SOFTWARE, "Out of memory.");

	memset(pfi, 0, sizeof(*pfi));

	TAILQ_INIT(&pfi->fields);

	strlcpy(pfi->name, pstr, sizeof(pfi->name));
	pfi->bit_offset = bo;
	pfi->bit_size = bs;
	return (pfi);
}

static const struct format_info *
format_get_section(const char *section)
{
	const struct format_info *psub;
	static const struct format_info *psub_last;
	static const char *psub_cache;

	if (psub_cache && strcmp(psub_cache, section) == 0)
		return (psub_last);

	TAILQ_FOREACH(psub, &format_head, entry) {
		if (strcmp(section, psub->name) == 0) {
			psub_cache = section;
			psub_last = psub;
			return (psub);
		}
	}
	warnx("Section '%s' not found", section);
	psub_cache = section;
	psub_last = psub;
	return (NULL);
}

uint16_t
format_get_section_size(const char *section)
{
	const struct format_info *pfi;

	pfi = format_get_section(section);
	if (pfi == NULL)
		return (0);

	return ((pfi->bit_offset + 7) / 8);
}


uint8_t
format_get_field(const char *section, const char *field,
    const uint8_t *ptr, uint16_t size)
{
	const struct format_info *pfi;
	const struct format_info *psub;
	uint16_t rem;
	uint16_t off;
	uint16_t sz;

	pfi = format_get_section(section);
	if (pfi == NULL)
		return (0);

	/* skip until we find the fields */
	while (pfi && TAILQ_FIRST(&pfi->fields) == NULL)
		pfi = TAILQ_NEXT(pfi, entry);

	if (pfi == NULL)
		return (0);

	TAILQ_FOREACH(psub, &pfi->fields, entry) {
		if (strcmp(field, psub->name) == 0) {

			/* range check */
			if (((psub->bit_offset + psub->bit_size) / 8) > size)
				return (0);

			/* compute byte offset */
			rem = psub->bit_offset & 7;
			off = psub->bit_offset / 8;
			sz = psub->bit_size;

			/* extract bit-field */
			return ((ptr[off] >> rem) & ((1 << sz) - 1));
		}
	}
	warnx("Field '%s' not found in '%s'", field, pfi->name);
	return (0);
}

void
format_parse_entries(const uint8_t *ptr, uint32_t len)
{
	static const char *command_list = "012345678:";
	const char *cmd;
	struct format_info *pfi;
	struct format_info *pfi_last = NULL;
	char linebuf[3][MAX_STRING];
	uint32_t off = 0;
	uint16_t bit_offset = 0;
	uint8_t state = 0;
	uint8_t cmd_index;
	int c;

	/*
	 * The format we are parsing:
	 * <string>{string,string}<next_string>{...}
	 */
	while (len--) {
		c = *(ptr++);

		/* skip some characters */
		if (c == 0 || c == '\n' || c == '\r' || c == ' ' || c == '\t')
			continue;

		/* accumulate non-field delimiters */
		if (strchr("{,}", c) == NULL) {
			if (off < (MAX_STRING - 1)) {
				linebuf[state][off] = c;
				off++;
			}
			continue;
		}
		/* parse keyword */
		linebuf[state][off] = 0;
		off = 0;
		state++;
		if (state == 3) {
			/* check for command in command list */
			cmd = strchr(command_list, linebuf[2][0]);
			if (cmd != NULL)
				cmd_index = cmd - command_list;
			else
				cmd_index = 255;

			/*
			 * Check for new field, format is:
			 *
			 * <field_name>{bit_offset_xor, bit_size}
			 */
			if (cmd_index < 9 && pfi_last != NULL) {
				pfi = format_info_new(linebuf[0], bit_offset ^
				    atoi(linebuf[1]), cmd_index);
				TAILQ_INSERT_TAIL(&pfi_last->fields, pfi, entry);
				bit_offset += cmd_index;
			}
			/*
			 * Check for new section, format is:
			 *
			 * <section_name>{section_bit_size, :}
			 */
			if (cmd_index == 9) {
				pfi_last = format_info_new(linebuf[0],
				    atoi(linebuf[1]), cmd_index);
				TAILQ_INSERT_TAIL(&format_head, pfi_last, entry);
				bit_offset = 0;
			}
			state = 0;
			continue;
		}
	}
}
