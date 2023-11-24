/*-
 * Copyright (c) 2016 Andriy Gapon <avg@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <kenv.h>
#include <unistd.h>

#include <libzfsbootenv.h>

#ifndef ZFS_MAXNAMELEN
#define	ZFS_MAXNAMELEN	256
#endif

static int
add_pair(const char *name, const char *nvlist, const char *key,
    const char *type, const char *value)
{
	void *data, *nv;
	size_t size;
	int rv;
	char *end;

	rv = lzbe_nvlist_get(name, nvlist, &nv);
	if (rv != 0)
		return (rv);

	data = NULL;
	rv = EINVAL;
	if (strcmp(type, "DATA_TYPE_STRING") == 0) {
		data = __DECONST(void *, value);
		size = strlen(data) + 1;
		rv = lzbe_add_pair(nv, key, type, data, size);
	} else if (strcmp(type, "DATA_TYPE_UINT64") == 0) {
		uint64_t v;

		v = strtoull(value, &end, 0);
		if (errno != 0 || *end != '\0')
			goto done;
		size = sizeof (v);
		rv = lzbe_add_pair(nv, key, type, &v, size);
	} else if (strcmp(type, "DATA_TYPE_INT64") == 0) {
		int64_t v;

		v = strtoll(value, &end, 0);
		if (errno != 0 || *end != '\0')
			goto done;
		size = sizeof (v);
		rv = lzbe_add_pair(nv, key, type, &v, size);
	} else if (strcmp(type, "DATA_TYPE_UINT32") == 0) {
		uint32_t v;

		v = strtoul(value, &end, 0);
		if (errno != 0 || *end != '\0')
			goto done;
		size = sizeof (v);
		rv = lzbe_add_pair(nv, key, type, &v, size);
	} else if (strcmp(type, "DATA_TYPE_INT32") == 0) {
		int32_t v;

		v = strtol(value, &end, 0);
		if (errno != 0 || *end != '\0')
			goto done;
		size = sizeof (v);
		rv = lzbe_add_pair(nv, key, type, &v, size);
	} else if (strcmp(type, "DATA_TYPE_UINT16") == 0) {
		uint16_t v;

		v = strtoul(value, &end, 0);
		if (errno != 0 || *end != '\0')
			goto done;
		size = sizeof (v);
		rv = lzbe_add_pair(nv, key, type, &v, size);
	} else if (strcmp(type, "DATA_TYPE_INT16") == 0) {
		int16_t v;

		v = strtol(value, &end, 0);
		if (errno != 0 || *end != '\0')
			goto done;
		size = sizeof (v);
		rv = lzbe_add_pair(nv, key, type, &v, size);
	} else if (strcmp(type, "DATA_TYPE_UINT8") == 0) {
		uint8_t v;

		v = strtoul(value, &end, 0);
		if (errno != 0 || *end != '\0')
			goto done;
		size = sizeof (v);
		rv = lzbe_add_pair(nv, key, type, &v, size);
	} else if (strcmp(type, "DATA_TYPE_INT8") == 0) {
		int8_t v;

		v = strtol(value, &end, 0);
		if (errno != 0 || *end != '\0')
			goto done;
		size = sizeof (v);
		rv = lzbe_add_pair(nv, key, type, &v, size);
	} else if (strcmp(type, "DATA_TYPE_BYTE") == 0) {
		uint8_t v;

		v = strtoul(value, &end, 0);
		if (errno != 0 || *end != '\0')
			goto done;
		size = sizeof (v);
		rv = lzbe_add_pair(nv, key, type, &v, size);
	} else if (strcmp(type, "DATA_TYPE_BOOLEAN_VALUE") == 0) {
		int32_t v;

		v = strtol(value, &end, 0);
		if (errno != 0 || *end != '\0') {
			if (strcasecmp(value, "YES") == 0)
				v = 1;
			else if (strcasecmp(value, "NO") == 0)
				v = 0;
			if (strcasecmp(value, "true") == 0)
				v = 1;
			else if (strcasecmp(value, "false") == 0)
				v = 0;
			else goto done;
		}
		size = sizeof (v);
		rv = lzbe_add_pair(nv, key, type, &v, size);
	}

	if (rv == 0)
		rv = lzbe_nvlist_set(name, nvlist, nv);

done:
	lzbe_nvlist_free(nv);
	return (rv);
}

static int
delete_pair(const char *name, const char *nvlist, const char *key)
{
	void *nv;
	int rv;

	rv = lzbe_nvlist_get(name, nvlist, &nv);
	if (rv == 0) {
		rv = lzbe_remove_pair(nv, key);
	}
	if (rv == 0)
		rv = lzbe_nvlist_set(name, nvlist, nv);

	lzbe_nvlist_free(nv);
	return (rv);
}

/*
 * Usage: zfsbootcfg [-z pool] [-d key] [-k key -t type -v value] [-p]
 *	zfsbootcfg [-z pool] -n nvlist [-d key] [-k key -t type -v value] [-p]
 *
 * if nvlist is set, we will update nvlist in bootenv.
 * if nvlist is not set, we update pairs in bootenv.
 */
int
main(int argc, char * const *argv)
{
	char buf[ZFS_MAXNAMELEN], *name;
	const char *key, *value, *type, *nvlist;
	int rv;
	bool print, delete;

	nvlist = NULL;
	name = NULL;
	key = NULL;
	type = NULL;
	value = NULL;
	print = delete = false;
	while ((rv = getopt(argc, argv, "d:k:n:pt:v:z:")) != -1) {
		switch (rv) {
		case 'd':
			delete = true;
			key = optarg;
			break;
		case 'k':
			key = optarg;
			break;
		case 'n':
			nvlist = optarg;
			break;
		case 'p':
			print = true;
			break;
		case 't':
			type = optarg;
			break;
		case 'v':
			value = optarg;
			break;
		case 'z':
			name = optarg;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 1)
		value = argv[0];

	if (argc > 1) {
		fprintf(stderr, "usage: zfsbootcfg <boot.config(5) options>\n");
		return (1);
	}

	if (name == NULL) {
		rv = kenv(KENV_GET, "vfs.root.mountfrom", buf, sizeof(buf));
		if (rv <= 0) {
			perror("can't get vfs.root.mountfrom");
			return (1);
		}

		if (strncmp(buf, "zfs:", 4) == 0) {
			name = strchr(buf + 4, '/');
			if (name != NULL)
				*name = '\0';
			name = buf + 4;
		} else {
			perror("not a zfs root");
			return (1);
		}
	}

	rv = 0;
	if (key != NULL || value != NULL) {
		if (type == NULL)
			type = "DATA_TYPE_STRING";

		if (delete)
			rv = delete_pair(name, nvlist, key);
		else if (key == NULL || strcmp(key, "command") == 0)
			rv = lzbe_set_boot_device(name, lzbe_add, value);
		else
			rv = add_pair(name, nvlist, key, type, value);

		if (rv == 0)
			printf("zfs bootenv is successfully written\n");
		else
			printf("error: %d\n", rv);
	} else if (!print) {
		char *ptr;

		if (lzbe_get_boot_device(name, &ptr) == 0) {
			printf("zfs:%s:\n", ptr);
			free(ptr);
		}
	}

	if (print) {
		rv = lzbe_bootenv_print(name, nvlist, stdout);
	}

	return (rv);
}
