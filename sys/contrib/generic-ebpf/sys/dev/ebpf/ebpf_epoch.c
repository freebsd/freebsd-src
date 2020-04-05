/*-
 * SPDX-License-Identifier: Apache License 2.0
 *
 * Copyright 2019 Yutaro Hayakawa
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * This file contains user space implmentation of epoch interfaces. It
 * will not be used for platform which has native epoch or epoch-like
 * memory reclamation mechanism like RCU.
 */

#include "ebpf_epoch.h"

static ck_epoch_t ebpf_epoch;
static pthread_key_t ebpf_epoch_key;

static void
ebpf_epoch_record_dtor(void *specific)
{
	ck_epoch_record_t *record = specific;
	ck_epoch_unregister(record);
}

int
ebpf_epoch_init(void)
{
	ck_epoch_init(&ebpf_epoch);
	return pthread_key_create(&ebpf_epoch_key,
			ebpf_epoch_record_dtor);
}

int
ebpf_epoch_deinit(void)
{
	return pthread_key_delete(ebpf_epoch_key);
}

static int
ebpf_epoch_get_record(ck_epoch_record_t **record)
{
	int error;
	*record = pthread_getspecific(ebpf_epoch_key);

	if (*record == NULL) {
		*record = ebpf_malloc(sizeof(ck_epoch_record_t));
		if (*record == NULL)
			return -1;

		ck_epoch_register(&ebpf_epoch, *record);

		error = pthread_setspecific(ebpf_epoch_key, *record);
		if (error != 0) {
			ebpf_free(*record);
			return -1;
		}
	}

	return 0;
}

void
ebpf_epoch_enter(void)
{
	int error;
	ck_epoch_record_t *record;

	error = ebpf_epoch_get_record(&record);
	ebpf_assert(error == 0);

	ck_epoch_begin(record, NULL);
}

void
ebpf_epoch_exit(void)
{
	ck_epoch_record_t *record;

	record = pthread_getspecific(ebpf_epoch_key);
	ebpf_assert(record != NULL);

	ck_epoch_end(record, NULL);
}

void
ebpf_epoch_call(ebpf_epoch_context *ctx,
		void (*callback)(ebpf_epoch_context *))
{
	int error;
	ck_epoch_record_t *record;

	error = ebpf_epoch_get_record(&record);
	ebpf_assert(error == 0);

	ck_epoch_call(record, ctx, callback);
}

void
ebpf_epoch_wait(void)
{
	int error;
	ck_epoch_record_t *record;

	error = ebpf_epoch_get_record(&record);
	ebpf_assert(error == 0);

	ck_epoch_synchronize(record);
}
