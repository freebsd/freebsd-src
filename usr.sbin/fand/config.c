/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Stéphane Rochoy <stephane.rochoy@stormshield.eu>
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
#include <err.h>
#include <figpar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fand.h"

static int store_lo_duty(struct figpar_config *option, uint32_t line,
                         char *directive, char *value);
static int    store_temp(struct figpar_config *option, uint32_t line,
                         char *directive, char *value);
static int    store_duty(struct figpar_config *option, uint32_t line,
                         char *directive, char *value);

static struct figpar_config fand_config[] = {
	/* TYPE                DIRECTIVE          DEFAULT    HANDLER */
	{FIGPAR_TYPE_UINT,     "lo_duty",         {0},       &store_lo_duty},
	{FIGPAR_TYPE_DATA1,    "step.*.temp",     {0},       &store_temp},
	{FIGPAR_TYPE_UINT,     "step.*.duty",     {0},       &store_duty},
	{0, NULL, {0}, NULL}
};

static char *
handle_index(char *directive, const char *prefix, size_t *index)
{
	unsigned long  ul;
	char          *next;

	if (strncmp(directive, prefix, strlen(prefix)))
		return (directive);
	ul = strtoul(directive + strlen(prefix), &next, 0);
	*index = ul;
	return (next);
}

static int
store_lo_duty(struct figpar_config *option, uint32_t line __unused,
              char *directive __unused, char *value)
{
	unsigned long ul;

	if (option == NULL) {
		warnx("%s:%d:%s: Missing callback parameter", __FILE__,
		      __LINE__, __func__);
		return (-1); /* Abort processing */
	}

	ul               = strtoul(value, NULL, 0);
	cprofile.lo_duty = ul;
	return (0);
}

static int
store_temp(struct figpar_config *option, uint32_t line, char *directive,
           char *value)
{
	float  f;
	size_t s;

	if (option == NULL) {
		warnx("%s:%d:%s: Missing callback parameter", __FILE__,
		      __LINE__, __func__);
		return (-1); /* Abort processing */
	}

	handle_index(directive, "step.", &s);
	if (s == (size_t)-1) {
		warnx("Missing step index at line %u", line);
		return (-1);
	}

	f = strtof(value, NULL);

	/* Ensure temperatures are strictly increasing */
	if (s > 0 && f <= cprofile.steps[s - 1].temp) {
		warnx("step %zu temperature (%.2f) must be strictly greater"
		    " than step %zu temperature (%.2f)", s, f, s - 1,
		    cprofile.steps[s - 1].temp);
		return (-1);
	}

	cprofile.steps[s].temp = f;

	/* Update step count */
	if (s >= cprofile.step_count) {
		if (s != cprofile.step_count) {
			warnx("Invalid step index at line %u;"
			    " want %zu, got %zu", line, cprofile.step_count, s);
			return (-1);
		}
		cprofile.step_count++;
	}
	return (0);
}

static int
store_duty(struct figpar_config *option, uint32_t line, char *directive,
           char *value)
{
	size_t         s;
	unsigned long  ul;

	if (option == NULL) {
		warnx("%s:%d:%s: Missing callback parameter", __FILE__,
		      __LINE__, __func__);
		return (-1); /* Abort processing */
	}

	handle_index(directive, "step.", &s);
	if (s == (size_t)-1) {
		warnx("Missing step index at line %u", line);
		return (-1);
	}

	ul = strtoul(value, NULL, 0);
	cprofile.steps[s].duty = ul;

	/* Update step count */
	if (s >= cprofile.step_count) {
		if (s != cprofile.step_count) {
			warnx("Invalid step index at line %u;"
			    " want %zu, got %zu", line, cprofile.step_count, s);
			return (-1);
		}
		cprofile.step_count++;
	}

	return (0);
}

int
parse_fand_config(const char *path)
{
	return (parse_config(fand_config, path, NULL,
	                     FIGPAR_BREAK_ON_EQUALS | FIGPAR_REQUIRE_EQUALS));
}
