/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Kyle Evans <kevans@FreeBSD.org>
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/mount.h>
#include <err.h>
#include <jail.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <be.h>

#include "bectl.h"

static void jailparam_grow(void);
static void jailparam_add(const char *name, const char *val);
static int jailparam_del(const char *name);
static bool jailparam_addarg(char *arg);
static int jailparam_delarg(char *arg);

static int bectl_search_jail_paths(const char *mnt);
static int bectl_locate_jail(const char *ident);

/* We'll start with 8 parameters initially and grow as needed. */
#define	INIT_PARAMCOUNT	8

static struct jailparam *jp;
static int jpcnt;
static int jpused;
static char mnt_loc[BE_MAXPATHLEN];

static void
jailparam_grow(void)
{

	jpcnt *= 2;
	jp = realloc(jp, jpcnt * sizeof(*jp));
	if (jp == NULL)
		err(2, "realloc");
}

static void
jailparam_add(const char *name, const char *val)
{
	int i;

	for (i = 0; i < jpused; ++i) {
		if (strcmp(name, jp[i].jp_name) == 0)
			break;
	}

	if (i < jpused)
		jailparam_free(&jp[i], 1);
	else if (jpused == jpcnt)
		/* The next slot isn't allocated yet */
		jailparam_grow();

	if (jailparam_init(&jp[i], name) != 0)
		return;
	if (jailparam_import(&jp[i], val) != 0)
		return;
	++jpused;
}

static int
jailparam_del(const char *name)
{
	int i;
	char *val;

	for (i = 0; i < jpused; ++i) {
		if (strcmp(name, jp[i].jp_name) == 0)
			break;
	}

	if (i == jpused)
		return (ENOENT);

	for (; i < jpused - 1; ++i) {
		val = jailparam_export(&jp[i + 1]);

		jailparam_free(&jp[i], 1);
		/*
		 * Given the context, the following will really only fail if
		 * they can't allocate the copy of the name or value.
		 */
		if (jailparam_init(&jp[i], jp[i + 1].jp_name) != 0) {
			free(val);
			return (ENOMEM);
		}
		if (jailparam_import(&jp[i], val) != 0) {
			jailparam_free(&jp[i], 1);
			free(val);
			return (ENOMEM);
		}
		free(val);
	}

	jailparam_free(&jp[i], 1);
	--jpused;
	return (0);
}

static bool
jailparam_addarg(char *arg)
{
	char *name, *val;

	if (arg == NULL)
		return (false);
	name = arg;
	if ((val = strchr(arg, '=')) == NULL) {
		fprintf(stderr, "bectl jail: malformed jail option '%s'\n",
		    arg);
		return (false);
	}

	*val++ = '\0';
	if (strcmp(name, "path") == 0) {
		if (strlen(val) >= BE_MAXPATHLEN) {
			fprintf(stderr,
			    "bectl jail: skipping too long path assignment '%s' (max length = %d)\n",
			    val, BE_MAXPATHLEN);
			return (false);
		}
		strlcpy(mnt_loc, val, sizeof(mnt_loc));
	}
	jailparam_add(name, val);
	return (true);
}

static int
jailparam_delarg(char *arg)
{
	char *name, *val;

	if (arg == NULL)
		return (EINVAL);
	name = arg;
	if ((val = strchr(name, '=')) != NULL)
		*val++ = '\0';

	if (strcmp(name, "path") == 0)
		*mnt_loc = '\0';
	return (jailparam_del(name));
}

int
bectl_cmd_jail(int argc, char *argv[])
{
	char *bootenv, *mountpoint;
	int jid, opt, ret;
	bool default_hostname, default_name;

	default_hostname = default_name = true;
	jpcnt = INIT_PARAMCOUNT;
	jp = malloc(jpcnt * sizeof(*jp));
	if (jp == NULL)
		err(2, "malloc");

	jailparam_add("persist", "true");
	jailparam_add("allow.mount", "true");
	jailparam_add("allow.mount.devfs", "true");
	jailparam_add("enforce_statfs", "1");

	while ((opt = getopt(argc, argv, "o:u:")) != -1) {
		switch (opt) {
		case 'o':
			if (jailparam_addarg(optarg)) {
				/*
				 * optarg has been modified to null terminate
				 * at the assignment operator.
				 */
				if (strcmp(optarg, "name") == 0)
					default_name = false;
				if (strcmp(optarg, "host.hostname") == 0)
					default_hostname = false;
			}
			break;
		case 'u':
			if ((ret = jailparam_delarg(optarg)) == 0) {
				if (strcmp(optarg, "name") == 0)
					default_name = true;
				if (strcmp(optarg, "host.hostname") == 0)
					default_hostname = true;
			} else if (ret != ENOENT) {
				fprintf(stderr,
				    "bectl jail: error unsetting \"%s\"\n",
				    optarg);
				return (ret);
			}
			break;
		default:
			fprintf(stderr, "bectl jail: unknown option '-%c'\n",
			    optopt);
			return (usage(false));
		}
	}

	argc -= optind;
	argv += optind;

	/* struct jail be_jail = { 0 }; */
	if (argc < 1) {
		fprintf(stderr, "bectl jail: missing boot environment name\n");
		return (usage(false));
	}
	if (argc > 2) {
		fprintf(stderr, "bectl jail: too many arguments\n");
		return (usage(false));
	}

	bootenv = argv[0];

	/*
	 * XXX TODO: if its already mounted, perhaps there should be a flag to
	 * indicate its okay to proceed??
	 */
	if (*mnt_loc == '\0')
		mountpoint = NULL;
	else
		mountpoint = mnt_loc;
	if (be_mount(be, bootenv, mountpoint, 0, mnt_loc) != BE_ERR_SUCCESS) {
		fprintf(stderr, "could not mount bootenv\n");
		return (1);
	}

	if (default_name)
		jailparam_add("name", bootenv);
	if (default_hostname)
		jailparam_add("host.hostname", bootenv);
	/*
	 * This is our indicator that path was not set by the user, so we'll use
	 * the path that libbe generated for us.
	 */
	if (mountpoint == NULL)
		jailparam_add("path", mnt_loc);
	jid = jailparam_set(jp, jpused, JAIL_CREATE | JAIL_ATTACH);
	if (jid == -1) {
		fprintf(stderr, "unable to create jail.  error: %d\n", errno);
		return (1);
	}

	jailparam_free(jp, jpused);
	free(jp);

	/* We're attached within the jail... good bye! */
	chdir("/");
	execl("/bin/sh", "/bin/sh", NULL);
	return (0);
}

static int
bectl_search_jail_paths(const char *mnt)
{
	char jailpath[MAXPATHLEN];
	int jid;

	jid = 0;
	(void)mnt;
	while ((jid = jail_getv(0, "lastjid", &jid, "path", &jailpath,
	    NULL)) != -1) {
		if (strcmp(jailpath, mnt) == 0)
			return (jid);
	}

	return (-1);
}

/*
 * Locate a jail based on an arbitrary identifier.  This may be either a name,
 * a jid, or a BE name.  Returns the jid or -1 on failure.
 */
static int
bectl_locate_jail(const char *ident)
{
	nvlist_t *belist, *props;
	char *mnt;
	int jid;

	/* Try the easy-match first */
	jid = jail_getid(ident);
	if (jid != -1)
		return (jid);

	/* Attempt to try it as a BE name, first */
	if (be_prop_list_alloc(&belist) != 0)
		return (-1);

	if (be_get_bootenv_props(be, belist) != 0)
		return (-1);

	if (nvlist_lookup_nvlist(belist, ident, &props) == 0) {
		/* We'll attempt to resolve the jid by way of mountpoint */
		if (nvlist_lookup_string(props, "mountpoint", &mnt) == 0) {
			jid = bectl_search_jail_paths(mnt);
			be_prop_list_free(belist);
			return (jid);
		}

		be_prop_list_free(belist);
	}

	return (-1);
}

int
bectl_cmd_unjail(int argc, char *argv[])
{
	char path[MAXPATHLEN];
	char *cmd, *name, *target;
	int jid;

	/* Store alias used */
	cmd = argv[0];

	if (argc != 2) {
		fprintf(stderr, "bectl %s: wrong number of arguments\n", cmd);
		return (usage(false));
	}

	target = argv[1];

	/* Locate the jail */
	if ((jid = bectl_locate_jail(target)) == -1) {
		fprintf(stderr, "bectl %s: failed to locate BE by '%s'\n", cmd,
		    target);
		return (1);
	}

	bzero(&path, MAXPATHLEN);
	name = jail_getname(jid);
	if (jail_getv(0, "name", name, "path", path, NULL) != jid) {
		free(name);
		fprintf(stderr,
		    "bectl %s: failed to get path for jail requested by '%s'\n",
		    cmd, target);
		return (1);
	}

	free(name);

	if (be_mounted_at(be, path, NULL) != 0) {
		fprintf(stderr, "bectl %s: jail requested by '%s' not a BE\n",
		    cmd, target);
		return (1);
	}

	jail_remove(jid);
	unmount(path, 0);

	return (0);
}
