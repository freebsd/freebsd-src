/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2022 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2011-2012 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 * by Venkatesh Srinivas <vsrinivas@dragonflybsd.org>
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

struct pfs_entry {
	TAILQ_ENTRY(pfs_entry) entry;
	char name[NAME_MAX+1];
	char s[NAME_MAX+1];
};

int
cmd_pfs_list(int ac, char **av)
{
	hammer2_ioc_pfs_t pfs;
	int ecode = 0;
	int fd;
	int i;
	int all = 0;
	char *pfs_id_str = NULL;
	const char *type_str;
	TAILQ_HEAD(, pfs_entry) head;
	struct pfs_entry *p, *e;

	if (ac == 1 && av[0] == NULL) {
		av = get_hammer2_mounts(&ac);
		all = 1;
	}

	for (i = 0; i < ac; ++i) {
		if ((fd = hammer2_ioctl_handle(av[i])) < 0)
			return(1);
		bzero(&pfs, sizeof(pfs));
		TAILQ_INIT(&head);
		if (i)
			printf("\n");

		while ((pfs.name_key = pfs.name_next) != (hammer2_key_t)-1) {
			if (ioctl(fd, HAMMER2IOC_PFS_GET, &pfs) < 0) {
				perror("ioctl");
				ecode = 1;
				break;
			}
			hammer2_uuid_to_str(&pfs.pfs_clid, &pfs_id_str);
			if (pfs.pfs_type == HAMMER2_PFSTYPE_MASTER) {
				if (pfs.pfs_subtype == HAMMER2_PFSSUBTYPE_NONE)
					type_str = "MASTER";
				else
					type_str = hammer2_pfssubtype_to_str(
						pfs.pfs_subtype);
			} else {
				type_str = hammer2_pfstype_to_str(pfs.pfs_type);
			}
			e = calloc(1, sizeof(*e));
			snprintf(e->name, sizeof(e->name), "%s", pfs.name);
			snprintf(e->s, sizeof(e->s), "%-11s %s",
				type_str, pfs_id_str);
			free(pfs_id_str);
			pfs_id_str = NULL;

			p = TAILQ_FIRST(&head);
			while (p) {
				if (strcmp(e->name, p->name) <= 0) {
					TAILQ_INSERT_BEFORE(p, e, entry);
					break;
				}
				p = TAILQ_NEXT(p, entry);
			}
			if (!p)
				TAILQ_INSERT_TAIL(&head, e, entry);
		}
		close(fd);

		printf("Type        "
		       "ClusterId (pfs_clid)                 "
		       "Label on %s\n", av[i]);
		while ((p = TAILQ_FIRST(&head)) != NULL) {
			printf("%s %s\n", p->s, p->name);
			TAILQ_REMOVE(&head, p, entry);
			free(p);
		}
	}

	if (all)
		put_hammer2_mounts(ac, av);

	return (ecode);
}

int
cmd_pfs_getid(const char *sel_path, const char *name, int privateid)
{
	hammer2_ioc_pfs_t pfs;
	int ecode = 0;
	int fd;
	char *pfs_id_str = NULL;

	if ((fd = hammer2_ioctl_handle(sel_path)) < 0)
		return(1);
	bzero(&pfs, sizeof(pfs));

	snprintf(pfs.name, sizeof(pfs.name), "%s", name);
	if (ioctl(fd, HAMMER2IOC_PFS_LOOKUP, &pfs) < 0) {
		perror("ioctl");
		ecode = 1;
	} else {
		if (privateid)
			hammer2_uuid_to_str(&pfs.pfs_fsid, &pfs_id_str);
		else
			hammer2_uuid_to_str(&pfs.pfs_clid, &pfs_id_str);
		printf("%s\n", pfs_id_str);
		free(pfs_id_str);
		pfs_id_str = NULL;
	}
	close(fd);
	return (ecode);
}
