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
 * Provides cli command 'nvostre'
 */

#include "stand.h"
#include "nvstore.h"
#include "bootstrap.h"

COMMAND_SET(nvstore, "nvstore", "manage non-volatile data", command_nvstore);

static void
nvstore_usage(const char *me)
{
	printf("Usage:\t%s -l\n", me);
	printf("\t%s store -l\n", me);
	printf("\t%s store [-t type] key value\n", me);
	printf("\t%s store -g key\n", me);
	printf("\t%s store -d key\n", me);
}

/*
 * Usage: nvstore -l		# list stores
 *	nvstore store -l	# list data in store
 *	nvstore store [-t type] key value
 *	nvstore store -g key	# get value
 *	nvstore store -d key	# delete key
 */
static int
command_nvstore(int argc, char *argv[])
{
	int c;
	bool list, get, delete;
	nvstore_t *st;
	char *me, *name, *type;

	me = argv[0];
	optind = 1;
	optreset = 1;

	list = false;
	while ((c = getopt(argc, argv, "l")) != -1) {
		switch (c) {
		case 'l':
			list = true;
			break;
		case '?':
		default:
			return (CMD_ERROR);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		if (list) {
			if (STAILQ_EMPTY(&stores)) {
				printf("No configured nvstores\n");
				return (CMD_OK);
			}
			printf("List of configured nvstores:\n");
			STAILQ_FOREACH(st, &stores, nvs_next) {
				printf("\t%s\n", st->nvs_name);
			}
			return (CMD_OK);
		}
		nvstore_usage(me);
		return (CMD_ERROR);
	}

	if (argc == 0 || (argc != 0 && list)) {
		nvstore_usage(me);
		return (CMD_ERROR);
	}

	st = nvstore_get_store(argv[0]);
	if (st == NULL) {
		nvstore_usage(me);
		return (CMD_ERROR);
	}

	optind = 1;
	optreset = 1;
	name = NULL;
	type = NULL;
	get = delete = false;

	while ((c = getopt(argc, argv, "d:g:lt:")) != -1) {
		switch (c) {
		case 'd':
			if (list || get) {
				nvstore_usage(me);
				return (CMD_ERROR);
			}
			name = optarg;
			delete = true;
			break;
		case 'g':
			if (delete || list) {
				nvstore_usage(me);
				return (CMD_ERROR);
			}
			name = optarg;
			get = true;
			break;
		case 'l':
			if (delete || get) {
				nvstore_usage(me);
				return (CMD_ERROR);
			}
			list = true;
			break;
		case 't':
			type = optarg;
			break;
		case '?':
		default:
			return (CMD_ERROR);
		}
	}

	argc -= optind;
	argv += optind;

	if (list) {
		(void) nvstore_print(st);
		return (CMD_OK);
	}

	if (delete && name != NULL) {
		(void) nvstore_unset_var(st, name);
		return (CMD_OK);
	}

	if (get && name != NULL) {
		char *ptr = NULL;

		if (nvstore_get_var(st, name, (void **)&ptr) == 0)
			printf("%s = %s\n", name, ptr);
		return (CMD_OK);
	}

	if (argc == 2) {
		c = nvstore_set_var_from_string(st, type, argv[0], argv[1]);
		if (c != 0) {
			printf("error: %s\n", strerror(c));
			return (CMD_ERROR);
		}
		return (CMD_OK);
	}

	nvstore_usage(me);
	return (CMD_OK);
}
