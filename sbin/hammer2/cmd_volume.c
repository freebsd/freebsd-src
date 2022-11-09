/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2020 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2020 The DragonFly Project
 * All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "hammer2.h"

int
cmd_volume_list(int ac, char **av)
{
	hammer2_ioc_volume_list_t vollist;
	hammer2_ioc_volume_t *entry;
	int fd, i, j, n, w, all = 0, ecode = 0;

	if (ac == 1 && av[0] == NULL) {
		av = get_hammer2_mounts(&ac);
		all = 1;
	}
	vollist.volumes = calloc(HAMMER2_MAX_VOLUMES, sizeof(*vollist.volumes));

	for (i = 0; i < ac; ++i) {
		if (i)
			printf("\n");
		if (ac > 1 || all)
			printf("%s\n", av[i]);
		if ((fd = hammer2_ioctl_handle(av[i])) < 0) {
			ecode = 1;
			goto failed;
		}

		vollist.nvolumes = HAMMER2_MAX_VOLUMES;
		if (ioctl(fd, HAMMER2IOC_VOLUME_LIST, &vollist) < 0) {
			perror("ioctl");
			close(fd);
			ecode = 1;
			goto failed;
		}

		w = 0;
		for (j = 0; j < vollist.nvolumes; ++j) {
			entry = &vollist.volumes[j];
			n = (int)strlen(entry->path);
			if (n > w)
				w = n;
		}

		if (QuietOpt > 0) {
			for (j = 0; j < vollist.nvolumes; ++j) {
				entry = &vollist.volumes[j];
				printf("%s\n", entry->path);
			}
		} else {
			printf("version %d\n", vollist.version);
			printf("@%s\n", vollist.pfs_name);
			for (j = 0; j < vollist.nvolumes; ++j) {
				entry = &vollist.volumes[j];
				printf("volume%-2d %-*.*s %s",
				       entry->id, w, w, entry->path,
				       sizetostr(entry->size));
				if (VerboseOpt > 0)
					printf(" 0x%016jx 0x%016jx",
					       (intmax_t)entry->offset,
					       (intmax_t)entry->size);
				printf("\n");
			}
		}
		close(fd);
	}
failed:
	free(vollist.volumes);
	if (all)
		put_hammer2_mounts(ac, av);

	return (ecode);
}
