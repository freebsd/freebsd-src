/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 The FreeBSD Foundation
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libiscsiutil.h"

struct keys *
keys_new(void)
{
	struct keys *keys;

	keys = calloc(1, sizeof(*keys));
	if (keys == NULL)
		log_err(1, "calloc");

	return (keys);
}

void
keys_delete(struct keys *keys)
{

	for (int i = 0; i < KEYS_MAX; i++) {
		free(keys->keys_names[i]);
		free(keys->keys_values[i]);
	}
	free(keys);
}

void
keys_load(struct keys *keys, const char *data, size_t len)
{
	int i;
	char *keys_data, *name, *pair, *value;
	size_t pair_len;

	if (len == 0)
		return;

	if (data[len - 1] != '\0')
		log_errx(1, "protocol error: key not NULL-terminated\n");

	keys_data = malloc(len);
	if (keys_data == NULL)
		log_err(1, "malloc");
	memcpy(keys_data, data, len);

	/*
	 * XXX: Review this carefully.
	 */
	pair = keys_data;
	for (i = 0;; i++) {
		if (i >= KEYS_MAX)
			log_errx(1, "too many keys received");

		pair_len = strlen(pair);

		value = pair;
		name = strsep(&value, "=");
		if (name == NULL || value == NULL)
			log_errx(1, "malformed keys");
		keys->keys_names[i] = checked_strdup(name);
		keys->keys_values[i] = checked_strdup(value);
		log_debugx("key received: \"%s=%s\"",
		    keys->keys_names[i], keys->keys_values[i]);

		pair += pair_len + 1; /* +1 to skip the terminating '\0'. */
		if (pair == keys_data + len)
			break;
		assert(pair < keys_data + len);
	}
	free(keys_data);
}

void
keys_save(struct keys *keys, char **datap, size_t *lenp)
{
	FILE *fp;
	char *data;
	size_t len;
	int i;

	fp = open_memstream(&data, &len);
	if (fp == NULL)
		log_err(1, "open_memstream");
	for (i = 0; i < KEYS_MAX; i++) {
		if (keys->keys_names[i] == NULL)
			break;

		fprintf(fp, "%s=%s", keys->keys_names[i], keys->keys_values[i]);

		/* Append a '\0' after each key pair. */
		fputc('\0', fp);
	}
	if (fclose(fp) != 0)
		log_err(1, "fclose");

	if (len == 0) {
		free(data);
		data = NULL;
	}

	*datap = data;
	*lenp = len;
}

const char *
keys_find(struct keys *keys, const char *name)
{
	int i;

	/*
	 * Note that we don't handle duplicated key names here,
	 * as they are not supposed to happen in requests, and if they do,
	 * it's an initiator error.
	 */
	for (i = 0; i < KEYS_MAX; i++) {
		if (keys->keys_names[i] == NULL)
			return (NULL);
		if (strcmp(keys->keys_names[i], name) == 0)
			return (keys->keys_values[i]);
	}
	return (NULL);
}

void
keys_add(struct keys *keys, const char *name, const char *value)
{
	int i;

	log_debugx("key to send: \"%s=%s\"", name, value);

	/*
	 * Note that we don't check for duplicates here, as they are perfectly
	 * fine in responses, e.g. the "TargetName" keys in discovery session
	 * response.
	 */
	for (i = 0; i < KEYS_MAX; i++) {
		if (keys->keys_names[i] == NULL) {
			keys->keys_names[i] = checked_strdup(name);
			keys->keys_values[i] = checked_strdup(value);
			return;
		}
	}
	log_errx(1, "too many keys");
}

void
keys_add_int(struct keys *keys, const char *name, int value)
{
	char *str;
	int ret;

	ret = asprintf(&str, "%d", value);
	if (ret <= 0)
		log_err(1, "asprintf");

	keys_add(keys, name, str);
	free(str);
}
