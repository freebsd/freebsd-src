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

#ifndef HAMMER2_HAMMER2_H_
#define HAMMER2_HAMMER2_H_

/*
 * Rollup headers for hammer2 utility
 */
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/disk.h>
#include <dirent.h>

#include <fs/hammer2/hammer2_disk.h>
#include <fs/hammer2/hammer2_mount.h>
#include <fs/hammer2/hammer2_ioctl.h>
#include <fs/hammer2/hammer2_xxhash.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <uuid.h>
#include <assert.h>

#include "hammer2_subs.h"

/* user-specifiable check modes only */
#define HAMMER2_CHECK_STRINGS		{ "none", "disabled", "crc32", \
					  "xxhash64", "sha192" }
#define HAMMER2_CHECK_STRINGS_COUNT	5

#define HAMMER2_COMP_STRINGS		{ "none", "autozero", "lz4", "zlib" }
#define HAMMER2_COMP_STRINGS_COUNT	4

extern int VerboseOpt;
extern int QuietOpt;

/*
 * Hammer2 command APIs
 */
int cmd_pfs_getid(const char *sel_path, const char *name, int privateid);
int cmd_pfs_list(int ac, char **av);
int cmd_info(int ac, const char **av);
int cmd_mountall(int ac, const char **av);
int cmd_stat(int ac, const char **av);
int cmd_dumpchain(const char *path, u_int flags);
int cmd_show(const char *devpath, int which);
int cmd_volume_list(int ac, char **av);

void print_inode(const char *path);

#endif /* !HAMMER2_HAMMER2_H_ */
