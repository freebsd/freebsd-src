/*-
 * Copyright (c) 2008, 2009 Yahoo!, Inc.
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
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <err.h>
#include <libutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mfiutil.h"

MFI_TABLE(top, volume);

const char *
mfi_ldstate(enum mfi_ld_state state)
{
	static char buf[16];

	switch (state) {
	case MFI_LD_STATE_OFFLINE:
		return ("OFFLINE");
	case MFI_LD_STATE_PARTIALLY_DEGRADED:
		return ("PARTIALLY DEGRADED");
	case MFI_LD_STATE_DEGRADED:
		return ("DEGRADED");
	case MFI_LD_STATE_OPTIMAL:
		return ("OPTIMAL");
	default:
		sprintf(buf, "LSTATE 0x%02x", state);
		return (buf);
	}
}

void
mbox_store_ldref(uint8_t *mbox, union mfi_ld_ref *ref)
{

	mbox[0] = ref->v.target_id;
	mbox[1] = ref->v.reserved;
	mbox[2] = ref->v.seq & 0xff;
	mbox[3] = ref->v.seq >> 8;
}

int
mfi_ld_get_list(int fd, struct mfi_ld_list *list, uint8_t *statusp)
{

	return (mfi_dcmd_command(fd, MFI_DCMD_LD_GET_LIST, list,
		sizeof(struct mfi_ld_list), NULL, 0, statusp));
}

int
mfi_ld_get_info(int fd, uint8_t target_id, struct mfi_ld_info *info,
    uint8_t *statusp)
{
	uint8_t mbox[1];

	mbox[0] = target_id;
	return (mfi_dcmd_command(fd, MFI_DCMD_LD_GET_INFO, info,
	    sizeof(struct mfi_ld_info), mbox, 1, statusp));
}

static int
mfi_ld_get_props(int fd, uint8_t target_id, struct mfi_ld_props *props)
{
	uint8_t mbox[1];

	mbox[0] = target_id;
	return (mfi_dcmd_command(fd, MFI_DCMD_LD_GET_PROP, props,
	    sizeof(struct mfi_ld_props), mbox, 1, NULL));
}

static int
mfi_ld_set_props(int fd, struct mfi_ld_props *props)
{
	uint8_t mbox[4];

	mbox_store_ldref(mbox, &props->ld);
	return (mfi_dcmd_command(fd, MFI_DCMD_LD_SET_PROP, props,
	    sizeof(struct mfi_ld_props), mbox, 4, NULL));
}

static int
update_cache_policy(int fd, struct mfi_ld_props *props, uint8_t new_policy,
    uint8_t mask)
{
	int error;
	uint8_t changes, policy;

	policy = (props->default_cache_policy & ~mask) | new_policy;
	if (policy == props->default_cache_policy)
		return (0);
	changes = policy ^ props->default_cache_policy;
	if (changes & MR_LD_CACHE_ALLOW_WRITE_CACHE)
		printf("%s caching of I/O writes\n",
		    policy & MR_LD_CACHE_ALLOW_WRITE_CACHE ? "Enabling" :
		    "Disabling");
	if (changes & MR_LD_CACHE_ALLOW_READ_CACHE)
		printf("%s caching of I/O reads\n",
		    policy & MR_LD_CACHE_ALLOW_READ_CACHE ? "Enabling" :
		    "Disabling");
	if (changes & MR_LD_CACHE_WRITE_BACK)
		printf("Setting write cache policy to %s\n",
		    policy & MR_LD_CACHE_WRITE_BACK ? "write-back" :
		    "write-through");
	if (changes & (MR_LD_CACHE_READ_AHEAD | MR_LD_CACHE_READ_ADAPTIVE))
		printf("Setting read ahead policy to %s\n",
		    policy & MR_LD_CACHE_READ_AHEAD ?
		    (policy & MR_LD_CACHE_READ_ADAPTIVE ?
		    "adaptive" : "always") : "none");
	if (changes & MR_LD_CACHE_WRITE_CACHE_BAD_BBU)
		printf("%s write caching with bad BBU\n",
		    policy & MR_LD_CACHE_WRITE_CACHE_BAD_BBU ? "Enabling" :
		    "Disabling");

	props->default_cache_policy = policy;
	if (mfi_ld_set_props(fd, props) < 0) {
		error = errno;
		warn("Failed to set volume properties");
		return (error);
	}
	return (0);
}

static int
volume_cache(int ac, char **av)
{
	struct mfi_ld_props props;
	int error, fd;
	uint8_t target_id, policy;

	if (ac < 2) {
		warnx("cache: volume required");
		return (EINVAL);
	}

	fd = mfi_open(mfi_unit);
	if (fd < 0) {
		error = errno;
		warn("mfi_open");
		return (error);
	}

	if (mfi_lookup_volume(fd, av[1], &target_id) < 0) {
		error = errno;
		warn("Invalid volume: %s", av[1]);
		return (error);
	}

	if (mfi_ld_get_props(fd, target_id, &props) < 0) {
		error = errno;
		warn("Failed to fetch volume properties");
		return (error);
	}

	if (ac == 2) {
		printf("mfi%u volume %s cache settings:\n", mfi_unit,
		    mfi_volume_name(fd, target_id));
		printf("             I/O caching: ");
		switch (props.default_cache_policy &
		    (MR_LD_CACHE_ALLOW_WRITE_CACHE |
		    MR_LD_CACHE_ALLOW_READ_CACHE)) {
		case 0:
			printf("disabled\n");
			break;
		case MR_LD_CACHE_ALLOW_WRITE_CACHE:
			printf("writes\n");
			break;
		case MR_LD_CACHE_ALLOW_READ_CACHE:
			printf("reads\n");
			break;
		case MR_LD_CACHE_ALLOW_WRITE_CACHE |
		    MR_LD_CACHE_ALLOW_READ_CACHE:
			printf("writes and reads\n");
			break;
		}
		printf("           write caching: %s\n",
		    props.default_cache_policy & MR_LD_CACHE_WRITE_BACK ?
		    "write-back" : "write-through");
		printf("write cache with bad BBU: %s\n",
		    props.default_cache_policy &
		    MR_LD_CACHE_WRITE_CACHE_BAD_BBU ? "enabled" : "disabled");
		printf("              read ahead: %s\n",
		    props.default_cache_policy & MR_LD_CACHE_READ_AHEAD ?
		    (props.default_cache_policy & MR_LD_CACHE_READ_ADAPTIVE ?
		    "adaptive" : "always") : "none");
		printf("       drive write cache: ");
		switch (props.disk_cache_policy) {
		case MR_PD_CACHE_UNCHANGED:
			printf("default\n");
			break;
		case MR_PD_CACHE_ENABLE:
			printf("enabled\n");
			break;
		case MR_PD_CACHE_DISABLE:
			printf("disabled\n");
			break;
		default:
			printf("??? %d\n", props.disk_cache_policy);
			break;
		}
		if (props.default_cache_policy != props.current_cache_policy)
			printf("Cache Disabled Due to Dead Battery\n");
		error = 0;
	} else {
		if (strcmp(av[2], "all") == 0 || strcmp(av[2], "enable") == 0)
			error = update_cache_policy(fd, &props,
			    MR_LD_CACHE_ALLOW_READ_CACHE |
			    MR_LD_CACHE_ALLOW_WRITE_CACHE,
			    MR_LD_CACHE_ALLOW_READ_CACHE |
			    MR_LD_CACHE_ALLOW_WRITE_CACHE);
		else if (strcmp(av[2], "none") == 0 ||
		    strcmp(av[2], "disable") == 0)
			error = update_cache_policy(fd, &props, 0,
			    MR_LD_CACHE_ALLOW_READ_CACHE |
			    MR_LD_CACHE_ALLOW_WRITE_CACHE);
		else if (strcmp(av[2], "reads") == 0)
			error = update_cache_policy(fd, &props,
			    MR_LD_CACHE_ALLOW_READ_CACHE,
			    MR_LD_CACHE_ALLOW_READ_CACHE |
			    MR_LD_CACHE_ALLOW_WRITE_CACHE);
		else if (strcmp(av[2], "writes") == 0)
			error = update_cache_policy(fd, &props,
			    MR_LD_CACHE_ALLOW_WRITE_CACHE,
			    MR_LD_CACHE_ALLOW_READ_CACHE |
			    MR_LD_CACHE_ALLOW_WRITE_CACHE);
		else if (strcmp(av[2], "write-back") == 0)
			error = update_cache_policy(fd, &props,
			    MR_LD_CACHE_WRITE_BACK,
			    MR_LD_CACHE_WRITE_BACK);
		else if (strcmp(av[2], "write-through") == 0)
			error = update_cache_policy(fd, &props, 0,
			    MR_LD_CACHE_WRITE_BACK);
		else if (strcmp(av[2], "read-ahead") == 0) {
			if (ac < 4) {
				warnx("cache: read-ahead setting required");
				return (EINVAL);
			}
			if (strcmp(av[3], "none") == 0)
				policy = 0;
			else if (strcmp(av[3], "always") == 0)
				policy = MR_LD_CACHE_READ_AHEAD;
			else if (strcmp(av[3], "adaptive") == 0)
				policy = MR_LD_CACHE_READ_AHEAD |
				    MR_LD_CACHE_READ_ADAPTIVE;
			else {
				warnx("cache: invalid read-ahead setting");
				return (EINVAL);
			}
			error = update_cache_policy(fd, &props, policy,
			    MR_LD_CACHE_READ_AHEAD |
			    MR_LD_CACHE_READ_ADAPTIVE);
		} else if (strcmp(av[2], "bad-bbu-write-cache") == 0) {
			if (ac < 4) {
				warnx("cache: bad BBU setting required");
				return (EINVAL);
			}
			if (strcmp(av[3], "enable") == 0)
				policy = MR_LD_CACHE_WRITE_CACHE_BAD_BBU;
			else if (strcmp(av[3], "disable") == 0)
				policy = 0;
			else {
				warnx("cache: invalid bad BBU setting");
				return (EINVAL);
			}
			error = update_cache_policy(fd, &props, policy,
			    MR_LD_CACHE_WRITE_CACHE_BAD_BBU);
		} else if (strcmp(av[2], "write-cache") == 0) {
			if (ac < 4) {
				warnx("cache: write-cache setting required");
				return (EINVAL);
			}
			if (strcmp(av[3], "enable") == 0)
				policy = MR_PD_CACHE_ENABLE;
			else if (strcmp(av[3], "disable") == 0)
				policy = MR_PD_CACHE_DISABLE;
			else if (strcmp(av[3], "default") == 0)
				policy = MR_PD_CACHE_UNCHANGED;
			else {
				warnx("cache: invalid write-cache setting");
				return (EINVAL);
			}
			error = 0;
			if (policy != props.disk_cache_policy) {
				switch (policy) {
				case MR_PD_CACHE_ENABLE:
					printf("Enabling write-cache on physical drives\n");
					break;
				case MR_PD_CACHE_DISABLE:
					printf("Disabling write-cache on physical drives\n");
					break;
				case MR_PD_CACHE_UNCHANGED:
					printf("Using default write-cache setting on physical drives\n");
					break;
				}
				props.disk_cache_policy = policy;
				if (mfi_ld_set_props(fd, &props) < 0) {
					error = errno;
					warn("Failed to set volume properties");
				}
			}
		} else {
			warnx("cache: Invalid command");
			return (EINVAL);
		}
	}
	close(fd);

	return (error);
}
MFI_COMMAND(top, cache, volume_cache);

static int
volume_name(int ac, char **av)
{
	struct mfi_ld_props props;
	int error, fd;
	uint8_t target_id;

	if (ac != 3) {
		warnx("name: volume and name required");
		return (EINVAL);
	}

	if (strlen(av[2]) >= sizeof(props.name)) {
		warnx("name: new name is too long");
		return (ENOSPC);
	}

	fd = mfi_open(mfi_unit);
	if (fd < 0) {
		error = errno;
		warn("mfi_open");
		return (error);
	}

	if (mfi_lookup_volume(fd, av[1], &target_id) < 0) {
		error = errno;
		warn("Invalid volume: %s", av[1]);
		return (error);
	}

	if (mfi_ld_get_props(fd, target_id, &props) < 0) {
		error = errno;
		warn("Failed to fetch volume properties");
		return (error);
	}

	printf("mfi%u volume %s name changed from \"%s\" to \"%s\"\n", mfi_unit,
	    mfi_volume_name(fd, target_id), props.name, av[2]);
	bzero(props.name, sizeof(props.name));
	strcpy(props.name, av[2]);
	if (mfi_ld_set_props(fd, &props) < 0) {
		error = errno;
		warn("Failed to set volume properties");
		return (error);
	}

	close(fd);

	return (0);
}
MFI_COMMAND(top, name, volume_name);

static int
volume_progress(int ac, char **av)
{
	struct mfi_ld_info info;
	int error, fd;
	uint8_t target_id;

	if (ac != 2) {
		warnx("volume progress: %s", ac > 2 ? "extra arguments" :
		    "volume required");
		return (EINVAL);
	}

	fd = mfi_open(mfi_unit);
	if (fd < 0) {
		error = errno;
		warn("mfi_open");
		return (error);
	}

	if (mfi_lookup_volume(fd, av[1], &target_id) < 0) {
		error = errno;
		warn("Invalid volume: %s", av[1]);
		return (error);
	}

	/* Get the info for this drive. */
	if (mfi_ld_get_info(fd, target_id, &info, NULL) < 0) {
		error = errno;
		warn("Failed to fetch info for volume %s",
		    mfi_volume_name(fd, target_id));
		return (error);
	}

	/* Display any of the active events. */
	if (info.progress.active & MFI_LD_PROGRESS_CC)
		mfi_display_progress("Consistency Check", &info.progress.cc);
	if (info.progress.active & MFI_LD_PROGRESS_BGI)
		mfi_display_progress("Background Init", &info.progress.bgi);
	if (info.progress.active & MFI_LD_PROGRESS_FGI)
		mfi_display_progress("Foreground Init", &info.progress.fgi);
	if (info.progress.active & MFI_LD_PROGRESS_RECON)
		mfi_display_progress("Reconstruction", &info.progress.recon);
	if ((info.progress.active & (MFI_LD_PROGRESS_CC | MFI_LD_PROGRESS_BGI |
	    MFI_LD_PROGRESS_FGI | MFI_LD_PROGRESS_RECON)) == 0)
		printf("No activity in progress for volume %s.\n",
		    mfi_volume_name(fd, target_id));
	close(fd);

	return (0);
}
MFI_COMMAND(volume, progress, volume_progress);
