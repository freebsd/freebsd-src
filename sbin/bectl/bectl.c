/*
 * be.c
 *
 * Copyright (c) 2017 Kyle J. Kneitinger <kyle@kneit.in>
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/mount.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <sys/nv.h>
#include <be.h>

static int bectl_cmd_activate(int argc, char *argv[]);
static int bectl_cmd_create(int argc, char *argv[]);
static int bectl_cmd_destroy(int argc, char *argv[]);
static int bectl_cmd_export(int argc, char *argv[]);
static int bectl_cmd_import(int argc, char *argv[]);
static int bectl_cmd_add(int argc, char *argv[]);
static int bectl_cmd_jail(int argc, char *argv[]);
static int bectl_cmd_list(int argc, char *argv[]);
static int bectl_cmd_mount(int argc, char *argv[]);
static int bectl_cmd_rename(int argc, char *argv[]);
static int bectl_cmd_unjail(int argc, char *argv[]);
static int bectl_cmd_unmount(int argc, char *argv[]);

libbe_handle_t *be;

static int
usage(bool explicit)
{
	FILE *fp = explicit ? stdout : stderr;

	fprintf(fp,
	    "usage:\tbectl ( -h | -? | subcommand [args...] )\n"
	    "\tbectl activate [-t] beName\n"
	    "\tbectl create [-e nonActiveBe | -e beName@snapshot] beName\n"
	    "\tbectl create beName@snapshot\n"
	    "\tbectl destroy [-F] beName | beName@snapshot⟩\n"
	    "\tbectl export sourceBe\n"
	    "\tbectl import targetBe\n"
	    "\tbectl add (path)*\n"
	    "\tbectl jail bootenv\n"
	    "\tbectl list [-a] [-D] [-H] [-s]\n"
	    "\tbectl mount beName [mountpoint]\n"
	    "\tbectl rename origBeName newBeName\n"
	    "\tbectl { ujail | unjail } ⟨jailID | jailName⟩ bootenv\n"
	    "\tbectl { umount | unmount } [-f] beName\n");

	return (explicit ? 0 : EX_USAGE);
}


/*
 * Represents a relationship between the command name and the parser action
 * that handles it.
 */
struct command_map_entry {
	const char *command;
	int (*fn)(int argc, char *argv[]);
};

static struct command_map_entry command_map[] =
{
	{ "activate", bectl_cmd_activate },
	{ "create",   bectl_cmd_create   },
	{ "destroy",  bectl_cmd_destroy  },
	{ "export",   bectl_cmd_export   },
	{ "import",   bectl_cmd_import   },
	{ "add",      bectl_cmd_add      },
	{ "jail",     bectl_cmd_jail     },
	{ "list",     bectl_cmd_list     },
	{ "mount",    bectl_cmd_mount    },
	{ "rename",   bectl_cmd_rename   },
	{ "unjail",   bectl_cmd_unjail   },
	{ "unmount",  bectl_cmd_unmount  },
};

static int
get_cmd_index(char *cmd, int *index)
{
	int map_size = sizeof(command_map) / sizeof(struct command_map_entry);

	for (int i = 0; i < map_size; ++i) {
		if (strcmp(cmd, command_map[i].command) == 0) {
			*index = i;
			return (0);
		}
	}

	return (1);
}


static int
bectl_cmd_activate(int argc, char *argv[])
{
	int err, opt;
	bool temp;
	char *bootenv;

	temp = false;
	while ((opt = getopt(argc, argv, "t")) != -1) {
		switch (opt) {
		case 't':
			temp = true;
			break;
		default:
			fprintf(stderr, "bectl activate: unknown option '-%c'\n",
			    optopt);
			return (usage(false));
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		fprintf(stderr, "bectl activate: wrong number of arguments\n");
		return (usage(false));
	}


	/* activate logic goes here */
	if ((err = be_activate(be, argv[0], temp)) != 0) {
		// TODO: more specific error msg based on err
		printf("did not successfully activate boot environment %s\n",
		    argv[0]);
	} else {
		printf("successfully activated boot environment %s\n", argv[0]);
	}

	if (temp) {
		printf("for next boot\n");
	}

	return (err);
}


// TODO: when only one arg is given, and it contains an "@" the this should
// create that snapshot
static int
bectl_cmd_create(int argc, char *argv[])
{
	int err, opt;
	char *snapname;
	char *bootenv;
	char *source;

	snapname = NULL;
	while ((opt = getopt(argc, argv, "e:")) != -1) {
		switch (opt) {
		case 'e':
			snapname = optarg;
			break;
		default:
			fprintf(stderr, "bectl create: unknown option '-%c'\n",
			    optopt);
			return (usage(false));
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		fprintf(stderr, "bectl create: wrong number of arguments\n");
		return (usage(false));
	}

	bootenv = *argv;

	if (snapname != NULL) {
		if (strchr(snapname, '@') != NULL) {
			err = be_create_from_existing_snap(be, bootenv,
			    snapname);
		} else {
			err = be_create_from_existing(be, bootenv, snapname);
		}
	} else {
		if ((snapname = strchr(bootenv, '@')) != NULL) {
			*(snapname++) = '\0';
			if ((err = be_snapshot(be, (char *)be_active_path(be),
			    snapname, true, NULL)) != BE_ERR_SUCCESS) {
				fprintf(stderr, "failed to create snapshot\n");
			}
			asprintf(&source, "%s@%s", be_active_path(be), snapname);
			err = be_create_from_existing_snap(be, bootenv,
			    source);
			return (err);
		} else {
			err = be_create(be, bootenv);
		}
	}

	switch (err) {
	case BE_ERR_SUCCESS:
		break;
	default:
		if (snapname == NULL) {
			fprintf(stderr,
			    "failed to create bootenv %s\n", bootenv);
		} else {
			fprintf(stderr,
			    "failed to create bootenv %s from snapshot %s\n",
			    bootenv, snapname);
		}
	}

	return (err);
}


static int
bectl_cmd_export(int argc, char *argv[])
{
	int opt;
	char *bootenv;


	if (argc == 1) {
		fprintf(stderr, "bectl export: missing boot environment name\n");
		return (usage(false));
	}

	if (argc > 2) {
		fprintf(stderr, "bectl export: extra arguments provided\n");
		return (usage(false));
	}

	bootenv = argv[1];

	if (isatty(STDOUT_FILENO)) {
		fprintf(stderr, "bectl export: must redirect output\n");
		return (EX_USAGE);
	}

	be_export(be, bootenv, STDOUT_FILENO);

	return (0);
}


static int
bectl_cmd_import(int argc, char *argv[])
{
	char *bootenv;
	int err;


	if (argc == 1) {
		fprintf(stderr, "bectl import: missing boot environment name\n");
		return (usage(false));
	}


	if (argc > 2) {
		fprintf(stderr, "bectl import: extra arguments provided\n");
		return (usage(false));
	}

	bootenv = argv[1];

	if (isatty(STDIN_FILENO)) {
		fprintf(stderr, "bectl import: input can not be from terminal\n");
		return (EX_USAGE);
	}

	err = be_import(be, bootenv, STDIN_FILENO);

	return (err);
}


static int
bectl_cmd_add(int argc, char *argv[])
{
	char *bootenv;

	if (argc < 2) {
		fprintf(stderr, "bectl add: must provide at least one path\n");
		return (usage(false));
	}

	for (int i = 1; i < argc; ++i) {
		printf("arg %d: %s\n", i, argv[i]);
		// TODO catch err
		be_add_child(be, argv[i], true);
	}

	return (0);
}


static int
bectl_cmd_destroy(int argc, char *argv[])
{
	int opt, err;
	bool force;
	char *target;

	force = false;
	while ((opt = getopt(argc, argv, "F")) != -1) {
		switch (opt) {
		case 'F':
			force = true;
			break;
		default:
			fprintf(stderr, "bectl destroy: unknown option '-%c'\n",
			    optopt);
			return (usage(false));
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		fprintf(stderr, "bectl destroy: wrong number of arguments\n");
		return (usage(false));
	}

	target = argv[0];

	err = be_destroy(be, target, force);

	return (err);
}


static int
bectl_cmd_jail(int argc, char *argv[])
{
	char *bootenv;
	char mnt_loc[BE_MAXPATHLEN];
	char buf[BE_MAXPATHLEN*2];
	int err, jid;

	//struct jail be_jail = { 0 };

	if (argc == 1) {
		fprintf(stderr, "bectl jail: missing boot environment name\n");
		return (usage(false));
	}
	if (argc > 2) {
		fprintf(stderr, "bectl jail: too many arguments\n");
		return (usage(false));
	}

	bootenv = argv[1];

	// TODO: if its already mounted, perhaps there should be a flag to
	// indicate its okay to proceed??
	if ((err = be_mount(be, bootenv, NULL, 0, mnt_loc)) != BE_ERR_SUCCESS) {
		fprintf(stderr, "could not mount bootenv\n");
	}

	// NOTE: this is not quite functional:
	// see https://github.com/vermaden/beadm/blob/master/HOWTO.htm on
	// neccesary modifications to correctly boot the jail

	//snprintf(buf, BE_MAXPATHLEN*2, "jail %s %s %s /bin/sh /etc/rc", mnt_loc, bootenv, "192.168.1.123");
	snprintf(buf, BE_MAXPATHLEN*2, "jail %s %s %s /bin/sh", mnt_loc,
	    bootenv, "192.168.1.123");
	system(buf);

	unmount(mnt_loc, 0);

	/*
	 * be_jail.version = JAIL_API_VERSION;
	 * be_jail.path = "/tmp/be_mount.hCCk";
	 * be_jail.jailname = "sdfs";
	 *
	 * if ((jid = jail(&be_jail)) != -1) {
	 *      printf("jail %d created at %s\n", jid, mnt_loc);
	 *      err = 0;
	 * } else {
	 *      fprintf(stderr, "unable to create jail.  error: %d\n", errno);
	 *      err = errno;
	 * }
	 */

	return (0);
}


static int
bectl_cmd_list(int argc, char *argv[])
{
	int opt;
	bool show_all_datasets, show_space, hide_headers, show_snaps;
	char *bootenv;
	nvlist_t *props;

	show_all_datasets = show_space = hide_headers = show_snaps = false;
	while ((opt = getopt(argc, argv, "aDHs")) != -1) {
		switch (opt) {
		case 'a':
			show_all_datasets = true;
			break;
		case 'D':
			show_space = true;
			break;
		case 'H':
			hide_headers = true;
			break;
		case 's':
			show_space = true;
			break;
		default:
			fprintf(stderr, "bectl list: unknown option '-%c'\n",
			    optopt);
			return (usage(false));
		}
	}

	argc -= optind;

	if (argc != 0) {
		fprintf(stderr, "bectl list: extra argument provided\n");
		return (usage(false));
	}

	//props = be_get_bootenv_props(be);

	return (0);
}


static int
bectl_cmd_mount(int argc, char *argv[])
{
	int err;
	char result_loc[BE_MAXPATHLEN];
	char *bootenv;
	char *mountpoint;

	if (argc < 2) {
		fprintf(stderr, "bectl mount: missing argument(s)\n");
		return (usage(false));
	}

	if (argc > 3) {
		fprintf(stderr, "bectl mount: too many arguments\n");
		return (usage(false));
	}

	bootenv = argv[1];
	mountpoint = ((argc == 3) ? argv[2] : NULL);


	err = be_mount(be, bootenv, mountpoint, 0, result_loc);

	switch (err) {
	case BE_ERR_SUCCESS:
		printf("successfully mounted %s at %s\n", bootenv, result_loc);
		break;
	default:
		fprintf(stderr,
		    (argc == 3) ? "failed to mount bootenv %s at %s\n" :
		    "failed to mount bootenv %s at temporary path %s\n",
		    bootenv, mountpoint);
	}

	return (err);
}


static int
bectl_cmd_rename(int argc, char *argv[])
{
	char *src;
	char *dest;
	int err;

	if (argc < 3) {
		fprintf(stderr, "bectl rename: missing argument\n");
		return (usage(false));
	}

	if (argc > 3) {
		fprintf(stderr, "bectl rename: too many arguments\n");
		return (usage(false));
	}

	src = argv[1];
	dest = argv[2];

	err = be_rename(be, src, dest);

	switch (err) {
	case BE_ERR_SUCCESS:
		break;
	default:
		fprintf(stderr, "failed to rename bootenv %s to %s\n",
		    src, dest);
	}

	return (0);
}


static int
bectl_cmd_unjail(int argc, char *argv[])
{
	int opt;
	char *cmd, *target;
	bool force;

	/* Store alias used */
	cmd = argv[0];

	force = false;
	while ((opt = getopt(argc, argv, "f")) != -1) {
		switch (opt) {
		case 'f':
			force = true;
			break;
		default:
			fprintf(stderr, "bectl %s: unknown option '-%c'\n",
			    cmd, optopt);
			return (usage(false));
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		fprintf(stderr, "bectl %s: wrong number of arguments\n", cmd);
		return (usage(false));
	}

	target = argv[0];

	/* unjail logic goes here */

	return (0);
}


static int
bectl_cmd_unmount(int argc, char *argv[])
{
	int err, flags, opt;
	char *cmd, *bootenv;

	/* Store alias used */
	cmd = argv[0];

	flags = 0;
	while ((opt = getopt(argc, argv, "f")) != -1) {
		switch (opt) {
		case 'f':
			flags |= BE_MNT_FORCE;
			break;
		default:
			fprintf(stderr, "bectl %s: unknown option '-%c'\n",
			    cmd, optopt);
			return (usage(false));
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		fprintf(stderr, "bectl %s: wrong number of arguments\n", cmd);
		return (usage(false));
	}

	bootenv = argv[0];

	err = be_unmount(be, bootenv, flags);

	switch (err) {
	case BE_ERR_SUCCESS:
		break;
	default:
		fprintf(stderr, "failed to unmount bootenv %s\n", bootenv);
	}

	return (err);
}


int
main(int argc, char *argv[])
{
	char *command;
	int command_index, rc;

	if (argc < 2) {
		fprintf(stderr, "missing command\n");
		return (usage(false));
	}

	command = argv[1];

	/* Handle command aliases */
	if (strcmp(command, "umount") == 0) {
		command = "unmount";
	}

	if (strcmp(command, "ujail") == 0) {
		command = "unjail";
	}

	if ((strcmp(command, "-?") == 0) || (strcmp(command, "-h") == 0)) {
		return (usage(true));
	}

	if (get_cmd_index(command, &command_index)) {
		fprintf(stderr, "unknown command: %s\n", command);
		return (usage(false));
	}


	if ((be = libbe_init()) == NULL) {
		return (-1);
	}

	libbe_print_on_error(be, true);

	/* TODO: can be simplified if offset by 2 instead of one */
	rc = command_map[command_index].fn(argc-1, argv+1);

	libbe_close(be);


	return (rc);
}
