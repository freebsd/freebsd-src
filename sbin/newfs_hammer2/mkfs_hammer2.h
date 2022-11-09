/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2022 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2011-2022 The DragonFly Project.  All rights reserved.
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

#ifndef NEWFS_HAMMER2_H_
#define NEWFS_HAMMER2_H_

#include <fs/hammer2/hammer2_disk.h>

#include <uuid.h>

#include "hammer2_subs.h"

#define HAMMER2_LABEL_NONE	0
#define HAMMER2_LABEL_BOOT	1
#define HAMMER2_LABEL_ROOT	2
#define HAMMER2_LABEL_DATA	3

#define MAXLABELS	HAMMER2_SET_COUNT

typedef struct {
	int Hammer2Version;
	uuid_t Hammer2_FSType;	/* filesystem type id for HAMMER2 */
	uuid_t Hammer2_VolFSID;	/* unique filesystem id in volu header */
	uuid_t Hammer2_SupCLID;	/* PFS cluster id in super-root inode */
	uuid_t Hammer2_SupFSID;	/* PFS unique id in super-root inode */
	uuid_t Hammer2_PfsCLID[MAXLABELS];
	uuid_t Hammer2_PfsFSID[MAXLABELS];
	hammer2_off_t BootAreaSize;
	hammer2_off_t AuxAreaSize;
	char *Label[MAXLABELS];
	int NLabels;
	int CompType; /* default LZ4 */
	int CheckType; /* default XXHASH64 */
	int DefaultLabelType;
	int DebugOpt;
} hammer2_mkfs_options_t;

void hammer2_mkfs_init(hammer2_mkfs_options_t *opt);
void hammer2_mkfs_cleanup(hammer2_mkfs_options_t *opt);

int64_t getsize(const char *str, int64_t minval, int64_t maxval, int pw);

void hammer2_mkfs(int ac, char **av, hammer2_mkfs_options_t *opt);

#endif /* !NEWFS_HAMMER2_H_ */
