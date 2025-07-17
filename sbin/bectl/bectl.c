/*
 * Copyright (c) 2017 Kyle J. Kneitinger <kyle@kneit.in>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <errno.h>
#include <libutil.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#include <be.h>

#include "bectl.h"

static int bectl_cmd_activate(int argc, char *argv[]);
static int bectl_cmd_check(int argc, char *argv[]);
static int bectl_cmd_create(int argc, char *argv[]);
static int bectl_cmd_destroy(int argc, char *argv[]);
static int bectl_cmd_export(int argc, char *argv[]);
static int bectl_cmd_import(int argc, char *argv[]);
#if SOON
static int bectl_cmd_add(int argc, char *argv[]);
#endif
static int bectl_cmd_mount(int argc, char *argv[]);
static int bectl_cmd_rename(int argc, char *argv[]);
static int bectl_cmd_unmount(int argc, char *argv[]);

libbe_handle_t *be;

int
usage(bool explicit)
{
	FILE *fp;

	fp =  explicit ? stdout : stderr;
	fprintf(fp, "%s",
	    "Usage:\tbectl {-h | subcommand [args...]}\n"
#if SOON
	    "\tbectl [-r beroot] add (path)*\n"
#endif
	    "\tbectl [-r beroot] activate [-t] beName\n"
	    "\tbectl [-r beroot] activate [-T]\n"
	    "\tbectl [-r beroot] check\n"
	    "\tbectl [-r beroot] create [-r] [-e {nonActiveBe | beName@snapshot}] beName\n"
	    "\tbectl [-r beroot] create [-r] beName@snapshot\n"
	    "\tbectl [-r beroot] destroy [-Fo] {beName | beName@snapshot}\n"
	    "\tbectl [-r beroot] export sourceBe\n"
	    "\tbectl [-r beroot] import targetBe\n"
	    "\tbectl [-r beroot] jail [-bU] [{-o key=value | -u key}]... beName\n"
	    "\t      [utility [argument ...]]\n"
	    "\tbectl [-r beroot] list [-aDHs] [{-c property | -C property}]\n"
	    "\tbectl [-r beroot] mount beName [mountpoint]\n"
	    "\tbectl [-r beroot] rename origBeName newBeName\n"
	    "\tbectl [-r beroot] {ujail | unjail} {jailID | jailName | beName}\n"
	    "\tbectl [-r beroot] {umount | unmount} [-f] beName\n");

	return (explicit ? 0 : EX_USAGE);
}


/*
 * Represents a relationship between the command name and the parser action
 * that handles it.
 */
struct command_map_entry {
	const char *command;
	int (*fn)(int argc, char *argv[]);
	/* True if libbe_print_on_error should be disabled */
	bool silent;
};

static struct command_map_entry command_map[] =
{
	{ "activate", bectl_cmd_activate,false   },
	{ "create",   bectl_cmd_create,  false   },
	{ "destroy",  bectl_cmd_destroy, false   },
	{ "export",   bectl_cmd_export,  false   },
	{ "import",   bectl_cmd_import,  false   },
#if SOON
	{ "add",      bectl_cmd_add,     false   },
#endif
	{ "jail",     bectl_cmd_jail,    false   },
	{ "list",     bectl_cmd_list,    false   },
	{ "mount",    bectl_cmd_mount,   false   },
	{ "rename",   bectl_cmd_rename,  false   },
	{ "unjail",   bectl_cmd_unjail,  false   },
	{ "ujail",    bectl_cmd_unjail,  false   },
	{ "unmount",  bectl_cmd_unmount, false   },
	{ "umount",   bectl_cmd_unmount, false   },
	{ "check",    bectl_cmd_check,   true    },
};

static struct command_map_entry *
get_cmd_info(const char *cmd)
{
	size_t i;

	for (i = 0; i < nitems(command_map); ++i) {
		if (strcmp(cmd, command_map[i].command) == 0)
			return (&command_map[i]);
	}

	return (NULL);
}

static int
bectl_cmd_activate(int argc, char *argv[])
{
	int err, opt;
	bool temp, reset;

	temp = false;
	reset = false;
	while ((opt = getopt(argc, argv, "tT")) != -1) {
		switch (opt) {
		case 't':
			if (reset)
				return (usage(false));
			temp = true;
			break;
		case 'T':
			if (temp)
				return (usage(false));
			reset = true;
			break;
		default:
			fprintf(stderr, "bectl activate: unknown option '-%c'\n",
			    optopt);
			return (usage(false));
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1 && (!reset || argc != 0)) {
		fprintf(stderr, "bectl activate: wrong number of arguments\n");
		return (usage(false));
	}

	if (reset) {
		if ((err = be_deactivate(be, NULL, reset)) == 0)
			printf("Temporary activation removed\n");
		else
			printf("Failed to remove temporary activation\n");
		return (err);
	}

	/* activate logic goes here */
	if ((err = be_activate(be, argv[0], temp)) != 0)
		/* XXX TODO: more specific error msg based on err */
		printf("Did not successfully activate boot environment %s",
		    argv[0]);
	else
		printf("Successfully activated boot environment %s", argv[0]);

	if (temp)
		printf(" for next boot");

	printf("\n");

	return (err);
}


/*
 * TODO: when only one arg is given, and it contains an "@" the this should
 * create that snapshot
 */
static int
bectl_cmd_create(int argc, char *argv[])
{
	char snapshot[BE_MAXPATHLEN];
	char *atpos, *bootenv, *snapname;
	int err, opt;
	bool recursive;

	snapname = NULL;
	recursive = false;
	while ((opt = getopt(argc, argv, "e:r")) != -1) {
		switch (opt) {
		case 'e':
			snapname = optarg;
			break;
		case 'r':
			recursive = true;
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

	err = BE_ERR_SUCCESS;
	if ((atpos = strchr(bootenv, '@')) != NULL) {
		/*
		 * This is the "create a snapshot variant". No new boot
		 * environment is to be created here.
		 */
		*atpos++ = '\0';
		err = be_snapshot(be, bootenv, atpos, recursive, NULL);
	} else {
		if (snapname == NULL)
			/* Create from currently booted BE */
			err = be_snapshot(be, be_active_path(be), NULL,
			    recursive, snapshot);
		else if (strchr(snapname, '@') != NULL)
			/* Create from given snapshot */
			strlcpy(snapshot, snapname, sizeof(snapshot));
		else
			/* Create from given BE */
			err = be_snapshot(be, snapname, NULL, recursive,
			    snapshot);

		if (err == BE_ERR_SUCCESS)
			err = be_create_depth(be, bootenv, snapshot,
					      recursive == true ? -1 : 0);
	}

	switch (err) {
	case BE_ERR_SUCCESS:
		break;
	case BE_ERR_INVALIDNAME:
		fprintf(stderr,
		    "bectl create: boot environment name must not contain spaces\n");
		break;
	default:
		if (atpos != NULL)
			fprintf(stderr,
			    "Failed to create a snapshot '%s' of '%s'\n",
			    atpos, bootenv);
		else if (snapname == NULL)
			fprintf(stderr,
			    "Failed to create bootenv %s\n", bootenv);
		else
			fprintf(stderr,
			    "Failed to create bootenv %s from snapshot %s\n",
			    bootenv, snapname);
	}

	return (err);
}


static int
bectl_cmd_export(int argc, char *argv[])
{
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

#if SOON
static int
bectl_cmd_add(int argc, char *argv[])
{

	if (argc < 2) {
		fprintf(stderr, "bectl add: must provide at least one path\n");
		return (usage(false));
	}

	for (int i = 1; i < argc; ++i) {
		printf("arg %d: %s\n", i, argv[i]);
		/* XXX TODO catch err */
		be_add_child(be, argv[i], true);
	}

	return (0);
}
#endif

static int
bectl_cmd_destroy(int argc, char *argv[])
{
	nvlist_t *props;
	char *target, targetds[BE_MAXPATHLEN];
	const char *origin;
	int err, flags, opt;

	flags = 0;
	while ((opt = getopt(argc, argv, "Fo")) != -1) {
		switch (opt) {
		case 'F':
			flags |= BE_DESTROY_FORCE;
			break;
		case 'o':
			flags |= BE_DESTROY_ORIGIN;
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

	/* We'll emit a notice if there's an origin to be cleaned up */
	if ((flags & BE_DESTROY_ORIGIN) == 0 && strchr(target, '@') == NULL) {
		flags |= BE_DESTROY_AUTOORIGIN;
		if (be_root_concat(be, target, targetds) != 0)
			goto destroy;
		if (be_prop_list_alloc(&props) != 0)
			goto destroy;
		if (be_get_dataset_props(be, targetds, props) != 0) {
			be_prop_list_free(props);
			goto destroy;
		}
		if (nvlist_lookup_string(props, "origin", &origin) == 0 &&
		    !be_is_auto_snapshot_name(be, origin))
			fprintf(stderr, "bectl destroy: leaving origin '%s' intact\n",
			    origin);
		be_prop_list_free(props);
	}

destroy:
	err = be_destroy(be, target, flags);

	return (err);
}

static int
bectl_cmd_mount(int argc, char *argv[])
{
	char result_loc[BE_MAXPATHLEN];
	char *bootenv, *mountpoint;
	int err, mntflags;

	/* XXX TODO: Allow shallow */
	mntflags = BE_MNT_DEEP;
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

	err = be_mount(be, bootenv, mountpoint, mntflags, result_loc);

	switch (err) {
	case BE_ERR_SUCCESS:
		printf("%s\n", result_loc);
		break;
	default:
		fprintf(stderr,
		    (argc == 3) ? "Failed to mount bootenv %s at %s\n" :
		    "Failed to mount bootenv %s at temporary path %s\n",
		    bootenv, mountpoint);
	}

	return (err);
}


static int
bectl_cmd_rename(int argc, char *argv[])
{
	char *dest, *src;
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
		fprintf(stderr, "Failed to rename bootenv %s to %s\n",
		    src, dest);
	}

	return (err);
}

static int
bectl_cmd_unmount(int argc, char *argv[])
{
	char *bootenv, *cmd;
	int err, flags, opt;

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
		fprintf(stderr, "Failed to unmount bootenv %s\n", bootenv);
	}

	return (err);
}

static int
bectl_cmd_check(int argc, char *argv[] __unused)
{

	/* The command is left as argv[0] */
	if (argc != 1) {
		fprintf(stderr, "bectl check: wrong number of arguments\n");
		return (usage(false));
	}

	return (0);
}

int
main(int argc, char *argv[])
{
	struct command_map_entry *cmd;
	const char *command;
	char *root = NULL;
	int opt, rc;

	while ((opt = getopt(argc, argv, "hr:")) != -1) {
		switch (opt) {
		case 'h':
			exit(usage(true));
		case 'r':
			root = strdup(optarg);
			break;
		default:
			exit(usage(false));
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		exit(usage(false));

	command = *argv;
	optreset = 1;
	optind = 1;

	if ((cmd = get_cmd_info(command)) == NULL) {
		fprintf(stderr, "Unknown command: %s\n", command);
		return (usage(false));
	}

	if ((be = libbe_init(root)) == NULL) {
		if (!cmd->silent)
			fprintf(stderr, "libbe_init(\"%s\") failed.\n",
			    root != NULL ? root : "");
		return (-1);
	}

	libbe_print_on_error(be, !cmd->silent);

	rc = cmd->fn(argc, argv);

	libbe_close(be);
	return (rc);
}
