/*-
 * Copyright 2020 Toomas Soome <tsoome@me.com>
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

/*
 * Big Theory Statement.
 *
 * nvstore is abstraction layer to implement data read/write to different
 * types of non-volatile storage.
 *
 * User interfaces:
 * Provide mapping via environment: setenv/unsetenv/putenv. Access via
 * environment functions/commands is available once nvstore has
 * attached the backend and stored textual data is mapped to environment.
 *
 * nvstore_init(): attach new backend and create the environment mapping.
 * nvstore_fini: detach backend and unmap the related environment.
 *
 * The disk based storage, such as UFS file or ZFS bootenv label area, is
 * only accessible after root file system is set. Root file system change
 * will switch the back end storage.
 */

#include <sys/cdefs.h>
#include <stdbool.h>
#include <sys/queue.h>
#include "stand.h"
#include "nvstore.h"

nvstore_list_t stores = STAILQ_HEAD_INITIALIZER(stores);

void *
nvstore_get_store(const char *name)
{
	nvstore_t *st;

	st = NULL;

	STAILQ_FOREACH(st, &stores, nvs_next) {
		if (strcmp(name, st->nvs_name) == 0)
			break;
	}

	return (st);
}

int
nvstore_init(const char *name, nvs_callbacks_t *cb, void *data)
{
	nvstore_t *st;

	st = nvstore_get_store(name);
	if (st != NULL)
		return (EEXIST);

	if ((st = malloc(sizeof (*st))) == NULL)
		return (ENOMEM);

	if ((st->nvs_name = strdup(name)) == NULL) {
		free(st);
		return (ENOMEM);
	}

	st->nvs_data = data;
	st->nvs_cb = cb;

	STAILQ_INSERT_TAIL(&stores, st, nvs_next);
	return (0);
}

int
nvstore_fini(const char *name)
{
	nvstore_t *st;

	st = nvstore_get_store(name);
	if (st == NULL)
		return (ENOENT);

	STAILQ_REMOVE(&stores, st, nvstore, nvs_next);

	free(st->nvs_name);
	free(st->nvs_data);
	free(st);
	return (0);
}

int
nvstore_print(void *ptr)
{
	nvstore_t *st = ptr;

	return (st->nvs_cb->nvs_iterate(st->nvs_data, st->nvs_cb->nvs_print));
}

int
nvstore_get_var(void *ptr, const char *name, void **data)
{
	nvstore_t *st = ptr;

	return (st->nvs_cb->nvs_getter(st->nvs_data, name, data));
}

int
nvstore_set_var(void *ptr, int type, const char *name,
    void *data, size_t size)
{
	nvstore_t *st = ptr;

	return (st->nvs_cb->nvs_setter(st->nvs_data, type, name, data, size));
}

int
nvstore_set_var_from_string(void *ptr, const char *type, const char *name,
    const char *data)
{
	nvstore_t *st = ptr;

	return (st->nvs_cb->nvs_setter_str(st->nvs_data, type, name, data));
}

int
nvstore_unset_var(void *ptr, const char *name)
{
	nvstore_t *st = ptr;

	return (st->nvs_cb->nvs_unset(st->nvs_data, name));
}
