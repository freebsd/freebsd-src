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
#include <jail.h>
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

#define	HEADER_BE	"BE"
#define	HEADER_BEPLUS	"BE/Dataset/Snapshot"
#define	HEADER_ACTIVE	"Active"
#define	HEADER_MOUNT	"Mountpoint"
#define	HEADER_SPACE	"Space"
#define	HEADER_CREATED	"Created"

/* Spaces */
#define	INDENT_INCREMENT	2

struct printc {
	int	active_colsz_def;
	int	be_colsz;
	int	current_indent;
	int	mount_colsz;
	int	space_colsz;
	bool	script_fmt;
	bool	show_all_datasets;
	bool	show_snaps;
	bool	show_space;
};

static int bectl_cmd_activate(int argc, char *argv[]);
static int bectl_cmd_create(int argc, char *argv[]);
static int bectl_cmd_destroy(int argc, char *argv[]);
static int bectl_cmd_export(int argc, char *argv[]);
static int bectl_cmd_import(int argc, char *argv[]);
static int bectl_cmd_add(int argc, char *argv[]);
static int bectl_cmd_jail(int argc, char *argv[]);
static const char *get_origin_props(nvlist_t *dsprops, nvlist_t **originprops);
static void print_padding(const char *fval, int colsz, struct printc *pc);
static void print_info(const char *name, nvlist_t *dsprops, struct printc *pc);
static void print_headers(nvlist_t *props, struct printc *pc);
static int bectl_cmd_list(int argc, char *argv[]);
static int bectl_cmd_mount(int argc, char *argv[]);
static int bectl_cmd_rename(int argc, char *argv[]);
static int bectl_search_jail_paths(const char *mnt);
static int bectl_locate_jail(const char *ident);
static int bectl_cmd_unjail(int argc, char *argv[]);
static int bectl_cmd_unmount(int argc, char *argv[]);

static libbe_handle_t *be;

static int
usage(bool explicit)
{
	FILE *fp;

	fp =  explicit ? stdout : stderr;
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
	    "\tbectl { ujail | unjail } ⟨jailID | jailName | bootenv)\n"
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
get_cmd_index(const char *cmd, int *index)
{
	int map_size;

	map_size = nitems(command_map);
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
	if ((err = be_activate(be, argv[0], temp)) != 0)
		/* XXX TODO: more specific error msg based on err */
		printf("did not successfully activate boot environment %s\n",
		    argv[0]);
	else
		printf("successfully activated boot environment %s\n", argv[0]);

	if (temp)
		printf("for next boot\n");

	return (err);
}


/*
 * TODO: when only one arg is given, and it contains an "@" the this should
 * create that snapshot
 */
static int
bectl_cmd_create(int argc, char *argv[])
{
	char *bootenv, *snapname, *source;
	int err, opt;

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
		if (strchr(snapname, '@') != NULL)
			err = be_create_from_existing_snap(be, bootenv,
			    snapname);
		else
			err = be_create_from_existing(be, bootenv, snapname);
	} else {
		if ((snapname = strchr(bootenv, '@')) != NULL) {
			*(snapname++) = '\0';
			if ((err = be_snapshot(be, be_active_path(be),
			    snapname, true, NULL)) != BE_ERR_SUCCESS)
				fprintf(stderr, "failed to create snapshot\n");
			asprintf(&source, "%s@%s", be_active_path(be), snapname);
			err = be_create_from_existing_snap(be, bootenv,
			    source);
			return (err);
		} else
			err = be_create(be, bootenv);
	}

	switch (err) {
	case BE_ERR_SUCCESS:
		break;
	default:
		if (snapname == NULL)
			fprintf(stderr,
			    "failed to create bootenv %s\n", bootenv);
		else
			fprintf(stderr,
			    "failed to create bootenv %s from snapshot %s\n",
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


static int
bectl_cmd_destroy(int argc, char *argv[])
{
	char *target;
	int opt, err;
	bool force;

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
	int err, jid;

	/* struct jail be_jail = { 0 }; */

	if (argc == 1) {
		fprintf(stderr, "bectl jail: missing boot environment name\n");
		return (usage(false));
	}
	if (argc > 2) {
		fprintf(stderr, "bectl jail: too many arguments\n");
		return (usage(false));
	}

	bootenv = argv[1];

	/*
	 * XXX TODO: if its already mounted, perhaps there should be a flag to
	 * indicate its okay to proceed??
	 */
	if ((err = be_mount(be, bootenv, NULL, 0, mnt_loc)) != BE_ERR_SUCCESS) {
		fprintf(stderr, "could not mount bootenv\n");
		return (1);
	}

	/* XXX TODO: Make the IP/hostname configurable? */
	jid = jail_setv(JAIL_CREATE | JAIL_ATTACH,
	    "name", bootenv,
	    "path", mnt_loc,
	    "host.hostname", bootenv,
	    "persist", "true",
	    "ip4.addr", "10.20.30.40",
	    "allow.mount", "true",
	    "allow.mount.devfs", "true",
	    "enforce_statfs", "1",
	    NULL);
	if (jid == -1) {
		fprintf(stderr, "unable to create jail.  error: %d\n", errno);
		return (1);
	}

	/* We're attached within the jail... good bye! */
	chdir("/");
	execl("/bin/sh", "/bin/sh", NULL);
	return (0);
}

/*
 * Given a set of dataset properties (for a BE dataset), populate originprops
 * with the origin's properties.
 */
static const char *
get_origin_props(nvlist_t *dsprops, nvlist_t **originprops)
{
	char *propstr;

	if (nvlist_lookup_string(dsprops, "origin", &propstr) == 0) {
		if (be_prop_list_alloc(originprops) != 0) {
			fprintf(stderr,
			    "bectl list: failed to allocate origin prop nvlist\n");
			return (NULL);
		}
		if (be_get_dataset_props(be, propstr, *originprops) != 0) {
			/* XXX TODO: Real errors */
			fprintf(stderr,
			    "bectl list: failed to fetch origin properties\n");
			return (NULL);
		}

		return (propstr);
	}
	return (NULL);
}

static void
print_padding(const char *fval, int colsz, struct printc *pc)
{

	if (pc->script_fmt) {
		printf("\t");
		return;
	}

	if (fval != NULL)
		colsz -= strlen(fval);
	printf("%*s ", colsz, "");
}

static unsigned long long
dataset_space(const char *oname)
{
	unsigned long long space;
	char *dsname, *propstr, *sep;
	nvlist_t *dsprops;

	space = 0;
	dsname = strdup(oname);
	if (dsname == NULL)
		return (0);

	if ((sep = strchr(dsname, '@')) != NULL)
		*sep = '\0';

	if (be_prop_list_alloc(&dsprops) != 0) {
		free(dsname);
		return (0);
	}

	if (be_get_dataset_props(be, dsname, dsprops) != 0) {
		nvlist_free(dsprops);
		free(dsname);
		return (0);
	}

	if (nvlist_lookup_string(dsprops, "used", &propstr) == 0)
		space = strtoull(propstr, NULL, 10);

	nvlist_free(dsprops);
	free(dsname);
	return (space);
}

static void
print_info(const char *name, nvlist_t *dsprops, struct printc *pc)
{
#define	BUFSZ	64
	char buf[BUFSZ];
	unsigned long long ctimenum, space;
	nvlist_t *originprops;
	const char *oname;
	char *propstr;
	int active_colsz;
	boolean_t active_now, active_reboot;

	originprops = NULL;
	printf("%*s%s", pc->current_indent, "", name);

	/* Recurse at the base level if we're breaking info down */
	if (pc->current_indent == 0 && (pc->show_all_datasets ||
	    pc->show_snaps)) {
		printf("\n");
		if (nvlist_lookup_string(dsprops, "dataset", &propstr) != 0)
			/* XXX TODO: Error? */
			return;
		pc->current_indent += INDENT_INCREMENT;
		print_info(propstr, dsprops, pc);
		pc->current_indent += INDENT_INCREMENT;
		if ((oname = get_origin_props(dsprops, &originprops)) != NULL) {
			print_info(oname, originprops, pc);
			nvlist_free(originprops);
		}
		pc->current_indent = 0;
		return;
	} else
		print_padding(name, pc->be_colsz - pc->current_indent, pc);

	active_colsz = pc->active_colsz_def;
	if (nvlist_lookup_boolean_value(dsprops, "active",
	    &active_now) == 0 && active_now) {
		printf("N");
		active_colsz--;
	}
	if (nvlist_lookup_boolean_value(dsprops, "nextboot",
	    &active_reboot) == 0 && active_reboot) {
		printf("R");
		active_colsz--;
	}
	if (active_colsz == pc->active_colsz_def) {
		printf("-");
		active_colsz--;
	}
	print_padding(NULL, active_colsz, pc);
	if (nvlist_lookup_string(dsprops, "mountpoint", &propstr) == 0){
		printf("%s", propstr);
		print_padding(propstr, pc->mount_colsz, pc);
	} else {
		printf("%s", "-");
		print_padding("-", pc->mount_colsz, pc);
	}

	oname = get_origin_props(dsprops, &originprops);

	if (nvlist_lookup_string(dsprops, "used", &propstr) == 0) {
		space = strtoull(propstr, NULL, 10);

		if (!pc->show_all_datasets && originprops != NULL &&
		    nvlist_lookup_string(originprops, "used", &propstr) == 0)
			space += strtoull(propstr, NULL, 10);

		if (!pc->show_all_datasets && pc->show_space && oname != NULL)
			/* Get the used space of the origin's dataset, too. */
			space += dataset_space(oname);

		/* Alas, there's more to it,. */
		be_nicenum(space, buf, 6);
		printf("%s", buf);
		print_padding(buf, pc->space_colsz, pc);
	} else {
		printf("%s", "-");
		print_padding("-", pc->space_colsz, pc);
	}

	if (nvlist_lookup_string(dsprops, "creation", &propstr) == 0) {
		ctimenum = strtoull(propstr, NULL, 10);
		strftime(buf, BUFSZ, "%Y-%m-%d %H:%M",
		    localtime((time_t *)&ctimenum));
		printf("%s", buf);
	}

	printf("\n");
	if (originprops != NULL) {
		/*if (pc->show_all_datasets) {
		}*/
		be_prop_list_free(originprops);
	}
#undef BUFSZ
}

static void
print_headers(nvlist_t *props, struct printc *pc)
{
	const char *chosen_be_header;
	nvpair_t *cur;
	nvlist_t *dsprops;
	char *propstr;
	size_t be_maxcol;

	if (pc->show_all_datasets || pc->show_snaps)
		chosen_be_header = HEADER_BEPLUS;
	else
		chosen_be_header = HEADER_BE;
	be_maxcol = strlen(chosen_be_header);
	for (cur = nvlist_next_nvpair(props, NULL); cur != NULL;
	    cur = nvlist_next_nvpair(props, cur)) {
		be_maxcol = MAX(be_maxcol, strlen(nvpair_name(cur)));
		if (!pc->show_all_datasets && !pc->show_snaps)
			continue;
		nvpair_value_nvlist(cur, &dsprops);
		if (nvlist_lookup_string(dsprops, "dataset", &propstr) != 0)
			continue;
		be_maxcol = MAX(be_maxcol, strlen(propstr) + INDENT_INCREMENT);
		if (nvlist_lookup_string(dsprops, "origin", &propstr) != 0)
			continue;
		be_maxcol = MAX(be_maxcol,
		    strlen(propstr) + INDENT_INCREMENT * 2);
	}

	pc->be_colsz = be_maxcol;
	pc->active_colsz_def = strlen(HEADER_ACTIVE);
	pc->mount_colsz = strlen(HEADER_MOUNT);
	pc->space_colsz = strlen(HEADER_SPACE);
	printf("%*s %s %s %s %s\n", -pc->be_colsz, chosen_be_header,
	    HEADER_ACTIVE, HEADER_MOUNT, HEADER_SPACE, HEADER_CREATED);

	/*
	 * All other invocations in which we aren't using the default header
	 * will produce quite a bit of input.  Throw an extra blank line after
	 * the header to make it look nicer.
	 */
	if (chosen_be_header != HEADER_BE)
		printf("\n");
}

static int
bectl_cmd_list(int argc, char *argv[])
{
	struct printc pc;
	nvpair_t *cur;
	nvlist_t *dsprops, *props;
	int opt, printed;
	boolean_t active_now, active_reboot;

	props = NULL;
	printed = 0;
	bzero(&pc, sizeof(pc));
	while ((opt = getopt(argc, argv, "aDHs")) != -1) {
		switch (opt) {
		case 'a':
			pc.show_all_datasets = true;
			break;
		case 'D':
			pc.show_space = true;
			break;
		case 'H':
			pc.script_fmt = true;
			break;
		case 's':
			pc.show_snaps = true;
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

	if (be_prop_list_alloc(&props) != 0) {
		fprintf(stderr, "bectl list: failed to allocate prop nvlist\n");
		return (1);
	}
	if (be_get_bootenv_props(be, props) != 0) {
		/* XXX TODO: Real errors */
		fprintf(stderr, "bectl list: failed to fetch boot environments\n");
		return (1);
	}

	if (!pc.script_fmt)
		print_headers(props, &pc);
	/* Do a first pass to print active and next active first */
	for (cur = nvlist_next_nvpair(props, NULL); cur != NULL;
	    cur = nvlist_next_nvpair(props, cur)) {
		nvpair_value_nvlist(cur, &dsprops);
		active_now = active_reboot = false;

		nvlist_lookup_boolean_value(dsprops, "active", &active_now);
		nvlist_lookup_boolean_value(dsprops, "nextboot",
		    &active_reboot);
		if (!active_now && !active_reboot)
			continue;
		if (printed > 0 && (pc.show_all_datasets || pc.show_snaps))
			printf("\n");
		print_info(nvpair_name(cur), dsprops, &pc);
		printed++;
	}

	/* Now pull everything else */
	for (cur = nvlist_next_nvpair(props, NULL); cur != NULL;
	    cur = nvlist_next_nvpair(props, cur)) {
		nvpair_value_nvlist(cur, &dsprops);
		active_now = active_reboot = false;

		nvlist_lookup_boolean_value(dsprops, "active", &active_now);
		nvlist_lookup_boolean_value(dsprops, "nextboot",
		    &active_reboot);
		if (active_now || active_reboot)
			continue;
		if (printed > 0 && (pc.show_all_datasets || pc.show_snaps))
			printf("\n");
		print_info(nvpair_name(cur), dsprops, &pc);
		printed++;
	}
	be_prop_list_free(props);

	return (0);
}


static int
bectl_cmd_mount(int argc, char *argv[])
{
	char result_loc[BE_MAXPATHLEN];
	char *bootenv, *mountpoint;
	int err;

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
		fprintf(stderr, "failed to rename bootenv %s to %s\n",
		    src, dest);
	}

	return (0);
}

static int
bectl_search_jail_paths(const char *mnt)
{
	char jailpath[MAXPATHLEN + 1];
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

static int
bectl_cmd_unjail(int argc, char *argv[])
{
	char path[MAXPATHLEN + 1];
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
		fprintf(stderr, "bectl %s: failed to locate BE by '%s'\n", cmd, target);
		return (1);
	}

	bzero(&path, MAXPATHLEN + 1);
	name = jail_getname(jid);
	if (jail_getv(0, "name", name, "path", path, NULL) != jid) {
		free(name);
		fprintf(stderr, "bectl %s: failed to get path for jail requested by '%s'\n", cmd, target);
		return (1);
	}

	free(name);

	if (be_mounted_at(be, path, NULL) != 0) {
		fprintf(stderr, "bectl %s: jail requested by '%s' not a BE\n", cmd, target);
		return (1);
	}

	jail_remove(jid);
	unmount(path, 0);

	return (0);
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
		fprintf(stderr, "failed to unmount bootenv %s\n", bootenv);
	}

	return (err);
}


int
main(int argc, char *argv[])
{
	const char *command;
	int command_index, rc;

	if (argc < 2) {
		fprintf(stderr, "missing command\n");
		return (usage(false));
	}

	command = argv[1];

	/* Handle command aliases */
	if (strcmp(command, "umount") == 0)
		command = "unmount";

	if (strcmp(command, "ujail") == 0)
		command = "unjail";

	if ((strcmp(command, "-?") == 0) || (strcmp(command, "-h") == 0))
		return (usage(true));

	if (get_cmd_index(command, &command_index)) {
		fprintf(stderr, "unknown command: %s\n", command);
		return (usage(false));
	}


	if ((be = libbe_init()) == NULL)
		return (-1);

	libbe_print_on_error(be, true);

	/* XXX TODO: can be simplified if offset by 2 instead of one */
	rc = command_map[command_index].fn(argc-1, argv+1);

	libbe_close(be);
	return (rc);
}
