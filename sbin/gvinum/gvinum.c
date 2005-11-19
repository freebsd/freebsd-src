/*
 *  Copyright (c) 2004 Lukas Ertl, 2005 Chris Jones
 *  All rights reserved.
 * 
 * Portions of this software were developed for the FreeBSD Project 
 * by Chris Jones thanks to the support of Google's Summer of Code 
 * program and mentoring by Lukas Ertl. 
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/param.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/utsname.h>

#include <geom/vinum/geom_vinum_var.h>
#include <geom/vinum/geom_vinum_share.h>

#include <ctype.h>
#include <err.h>
#include <libgeom.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <paths.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>

#include "gvinum.h"

void	gvinum_create(int, char **);
void	gvinum_help(void);
void	gvinum_list(int, char **);
void	gvinum_move(int, char **);
void	gvinum_parityop(int, char **, int);
void	gvinum_printconfig(int, char **);
void	gvinum_rename(int, char **);
void	gvinum_rm(int, char **);
void	gvinum_saveconfig(void);
void	gvinum_setstate(int, char **);
void	gvinum_start(int, char **);
void	gvinum_stop(int, char **);
void	parseline(int, char **);
void	printconfig(FILE *, char *);

int
main(int argc, char **argv)
{
	int line, tokens;
	char buffer[BUFSIZ], *inputline, *token[GV_MAXARGS];

	/* Load the module if necessary. */
	if (kldfind(GVINUMMOD) < 0 && kldload(GVINUMMOD) < 0)
		err(1, GVINUMMOD ": Kernel module not available");

	/* Arguments given on the command line. */
	if (argc > 1) {
		argc--;
		argv++;
		parseline(argc, argv);

	/* Interactive mode. */
	} else {
		for (;;) {
			inputline = readline("gvinum -> ");
			if (inputline == NULL) {
				if (ferror(stdin)) {
					err(1, "can't read input");
				} else {
					printf("\n");
					exit(0);
				}
			} else if (*inputline) {
				add_history(inputline);
				strcpy(buffer, inputline);
				free(inputline);
				line++;		    /* count the lines */
				tokens = gv_tokenize(buffer, token, GV_MAXARGS);
				if (tokens)
					parseline(tokens, token);
			}
		}
	}
	exit(0);
}

void
gvinum_create(int argc, char **argv)
{
	struct gctl_req *req;
	struct gv_drive *d;
	struct gv_plex *p;
	struct gv_sd *s;
	struct gv_volume *v;
	FILE *tmp;
	int drives, errors, fd, line, plexes, plex_in_volume;
	int sd_in_plex, status, subdisks, tokens, volumes;
	const char *errstr;
	char buf[BUFSIZ], buf1[BUFSIZ], commandline[BUFSIZ], *ed;
	char original[BUFSIZ], tmpfile[20], *token[GV_MAXARGS];
	char plex[GV_MAXPLEXNAME], volume[GV_MAXVOLNAME];

	if (argc == 2) {
		if ((tmp = fopen(argv[1], "r")) == NULL) {
			warn("can't open '%s' for reading", argv[1]);
			return;
		}
	} else {
		snprintf(tmpfile, sizeof(tmpfile), "/tmp/gvinum.XXXXXX");
		
		if ((fd = mkstemp(tmpfile)) == -1) {
			warn("temporary file not accessible");
			return;
		}
		if ((tmp = fdopen(fd, "w")) == NULL) {
			warn("can't open '%s' for writing", tmpfile);
			return;
		}
		printconfig(tmp, "# ");
		fclose(tmp);
		
		ed = getenv("EDITOR");
		if (ed == NULL)
			ed = _PATH_VI;
		
		snprintf(commandline, sizeof(commandline), "%s %s", ed,
		    tmpfile);
		status = system(commandline);
		if (status != 0) {
			warn("couldn't exec %s; status: %d", ed, status);
			return;
		}
		
		if ((tmp = fopen(tmpfile, "r")) == NULL) {
			warn("can't open '%s' for reading", tmpfile);
			return;
		}
	}

	req = gctl_get_handle();
	gctl_ro_param(req, "class", -1, "VINUM");
	gctl_ro_param(req, "verb", -1, "create");

	drives = volumes = plexes = subdisks = 0;
	plex_in_volume = sd_in_plex = 0;
	errors = 0;
	line = 1;
	while ((fgets(buf, BUFSIZ, tmp)) != NULL) {

		/* Skip empty lines and comments. */
		if (*buf == '\0' || *buf == '#') {
			line++;
			continue;
		}

		/* Kill off the newline. */
		buf[strlen(buf) - 1] = '\0';

		/*
		 * Copy the original input line in case we need it for error
		 * output.
		 */
		strncpy(original, buf, sizeof(buf));

		tokens = gv_tokenize(buf, token, GV_MAXARGS);
		if (tokens <= 0) {
			line++;
			continue;
		}

		/* Volume definition. */
		if (!strcmp(token[0], "volume")) {
			v = gv_new_volume(tokens, token);
			if (v == NULL) {
				warnx("line %d: invalid volume definition",
				    line);
				warnx("line %d: '%s'", line, original);
				errors++;
				line++;
				continue;
			}

			/* Reset plex count for this volume. */
			plex_in_volume = 0;

			/*
			 * Set default volume name for following plex
			 * definitions.
			 */
			strncpy(volume, v->name, sizeof(volume));

			snprintf(buf1, sizeof(buf1), "volume%d", volumes);
			gctl_ro_param(req, buf1, sizeof(*v), v);
			volumes++;

		/* Plex definition. */
		} else if (!strcmp(token[0], "plex")) {
			p = gv_new_plex(tokens, token);
			if (p == NULL) {
				warnx("line %d: invalid plex definition", line);
				warnx("line %d: '%s'", line, original);
				errors++;
				line++;
				continue;
			}

			/* Reset subdisk count for this plex. */
			sd_in_plex = 0;

			/* Default name. */
			if (strlen(p->name) == 0) {
				snprintf(p->name, GV_MAXPLEXNAME, "%s.p%d",
				    volume, plex_in_volume++);
			}

			/* Default volume. */
			if (strlen(p->volume) == 0) {
				snprintf(p->volume, GV_MAXVOLNAME, "%s",
				    volume);
			}

			/*
			 * Set default plex name for following subdisk
			 * definitions.
			 */
			strncpy(plex, p->name, GV_MAXPLEXNAME);

			snprintf(buf1, sizeof(buf1), "plex%d", plexes);
			gctl_ro_param(req, buf1, sizeof(*p), p);
			plexes++;

		/* Subdisk definition. */
		} else if (!strcmp(token[0], "sd")) {
			s = gv_new_sd(tokens, token);
			if (s == NULL) {
				warnx("line %d: invalid subdisk "
				    "definition:", line);
				warnx("line %d: '%s'", line, original);
				errors++;
				line++;
				continue;
			}

			/* Default name. */
			if (strlen(s->name) == 0) {
				snprintf(s->name, GV_MAXSDNAME, "%s.s%d",
				    plex, sd_in_plex++);
			}

			/* Default plex. */
			if (strlen(s->plex) == 0)
				snprintf(s->plex, GV_MAXPLEXNAME, "%s", plex);

			snprintf(buf1, sizeof(buf1), "sd%d", subdisks);
			gctl_ro_param(req, buf1, sizeof(*s), s);
			subdisks++;

		/* Subdisk definition. */
		} else if (!strcmp(token[0], "drive")) {
			d = gv_new_drive(tokens, token);
			if (d == NULL) {
				warnx("line %d: invalid drive definition:",
				    line);
				warnx("line %d: '%s'", line, original);
				errors++;
				line++;
				continue;
			}

			snprintf(buf1, sizeof(buf1), "drive%d", drives);
			gctl_ro_param(req, buf1, sizeof(*d), d);
			drives++;

		/* Everything else is bogus. */
		} else {
			warnx("line %d: invalid definition:", line);
			warnx("line %d: '%s'", line, original);
			errors++;
		}
		line++;
	}

	fclose(tmp);
	unlink(tmpfile);

	if (!errors && (volumes || plexes || subdisks || drives)) {
		gctl_ro_param(req, "volumes", sizeof(int), &volumes);
		gctl_ro_param(req, "plexes", sizeof(int), &plexes);
		gctl_ro_param(req, "subdisks", sizeof(int), &subdisks);
		gctl_ro_param(req, "drives", sizeof(int), &drives);
		errstr = gctl_issue(req);
		if (errstr != NULL)
			warnx("create failed: %s", errstr);
	}
	gctl_free(req);
	gvinum_list(0, NULL);
}

void
gvinum_help(void)
{
	printf("COMMANDS\n"
	    "checkparity [-f] plex\n"
	    "        Check the parity blocks of a RAID-5 plex.\n"
	    "create description-file\n"
	    "        Create as per description-file or open editor.\n"
	    "l | list [-r] [-v] [-V] [volume | plex | subdisk]\n"
	    "        List information about specified objects.\n"
	    "ld [-r] [-v] [-V] [volume]\n"
	    "        List information about drives.\n"
	    "ls [-r] [-v] [-V] [subdisk]\n"
	    "        List information about subdisks.\n"
	    "lp [-r] [-v] [-V] [plex]\n"
	    "        List information about plexes.\n"
	    "lv [-r] [-v] [-V] [volume]\n"
	    "        List information about volumes.\n"
	    "move | mv -f drive object ...\n"
	    "        Move the object(s) to the specified drive.\n"
	    "quit    Exit the vinum program when running in interactive mode."
	    "  Nor-\n"
	    "        mally this would be done by entering the EOF character.\n"
	    "rename [-r] [drive | subdisk | plex | volume] newname\n"
	    "        Change the name of the specified object.\n"
	    "rebuildparity plex [-f]\n"
	    "        Rebuild the parity blocks of a RAID-5 plex.\n"
	    "rm [-r] volume | plex | subdisk | drive\n"
	    "        Remove an object.\n"
	    "saveconfig\n"
	    "        Save vinum configuration to disk after configuration"
	    " failures.\n"
	    "setstate [-f] state [volume | plex | subdisk | drive]\n"
	    "        Set state without influencing other objects, for"
	    " diagnostic pur-\n"
	    "        poses only.\n"
	    "start [-S size] volume | plex | subdisk\n"
	    "        Allow the system to access the objects.\n"
	);

	return;
}

void
gvinum_setstate(int argc, char **argv)
{
	struct gctl_req *req;
	int flags, i;
	const char *errstr;

	flags = 0;

	optreset = 1;
	optind = 1;

	while ((i = getopt(argc, argv, "f")) != -1) {
		switch (i) {
		case 'f':
			flags |= GV_FLAG_F;
			break;
		case '?':
		default:
			warn("invalid flag: %c", i);
			return;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 2) {
		warnx("usage: setstate [-f] <state> <obj>");
		return;
	}

	/*
	 * XXX: This hack is needed to avoid tripping over (now) invalid
	 * 'classic' vinum states and will go away later.
	 */
	if (strcmp(argv[0], "up") && strcmp(argv[0], "down") &&
	    strcmp(argv[0], "stale")) {
		warnx("invalid state '%s'", argv[0]);
		return;
	}

	req = gctl_get_handle();
	gctl_ro_param(req, "class", -1, "VINUM");
	gctl_ro_param(req, "verb", -1, "setstate");
	gctl_ro_param(req, "state", -1, argv[0]);
	gctl_ro_param(req, "object", -1, argv[1]);
	gctl_ro_param(req, "flags", sizeof(int), &flags);

	errstr = gctl_issue(req);
	if (errstr != NULL)
		warnx("%s", errstr);
	gctl_free(req);
}

void
gvinum_list(int argc, char **argv)
{
	struct gctl_req *req;
	int flags, i, j;
	const char *errstr;
	char buf[20], *cmd, config[GV_CFG_LEN + 1];

	flags = 0;
	cmd = "list";

	if (argc) {
		optreset = 1;
		optind = 1;
		cmd = argv[0];
		while ((j = getopt(argc, argv, "rsvV")) != -1) {
			switch (j) {
			case 'r':
				flags |= GV_FLAG_R;
				break;
			case 's':
				flags |= GV_FLAG_S;
				break;
			case 'v':
				flags |= GV_FLAG_V;
				break;
			case 'V':
				flags |= GV_FLAG_V;
				flags |= GV_FLAG_VV;
				break;
			case '?':
			default:
				return;
			}
		}
		argc -= optind;
		argv += optind;

	}

	req = gctl_get_handle();
	gctl_ro_param(req, "class", -1, "VINUM");
	gctl_ro_param(req, "verb", -1, "list");
	gctl_ro_param(req, "cmd", -1, cmd);
	gctl_ro_param(req, "argc", sizeof(int), &argc);
	gctl_ro_param(req, "flags", sizeof(int), &flags);
	gctl_rw_param(req, "config", sizeof(config), config);
	if (argc) {
		for (i = 0; i < argc; i++) {
			snprintf(buf, sizeof(buf), "argv%d", i);
			gctl_ro_param(req, buf, -1, argv[i]);
		}
	}
	errstr = gctl_issue(req);
	if (errstr != NULL) {
		warnx("can't get configuration: %s", errstr);
		gctl_free(req);
		return;
	}

	printf("%s", config);
	gctl_free(req);
	return;
}

/* Note that move is currently of form '[-r] target object [...]' */
void
gvinum_move(int argc, char **argv)
{
	struct gctl_req *req;
	const char *errstr;
	char buf[20];
	int flags, i, j;

	flags = 0;
	if (argc) {
		optreset = 1;
		optind = 1;
		while ((j = getopt(argc, argv, "f")) != -1) {
			switch (j) {
			case 'f':
				flags |= GV_FLAG_F;
				break;
			case '?':
			default:
				return;
			}
		}
		argc -= optind;
		argv += optind;
	}

	switch (argc) {
		case 0:
			warnx("no destination or object(s) to move specified");
			return;
		case 1:
			warnx("no object(s) to move specified");
			return;
		default:
			break;
	}

	req = gctl_get_handle();
	gctl_ro_param(req, "class", -1, "VINUM");
	gctl_ro_param(req, "verb", -1, "move");
	gctl_ro_param(req, "argc", sizeof(int), &argc);
	gctl_ro_param(req, "flags", sizeof(int), &flags);
	gctl_ro_param(req, "destination", -1, argv[0]);
	for (i = 1; i < argc; i++) {
		snprintf(buf, sizeof(buf), "argv%d", i);
		gctl_ro_param(req, buf, -1, argv[i]);
	}
	errstr = gctl_issue(req); 
	if (errstr != NULL)
		warnx("can't move object(s):  %s", errstr);
	gctl_free(req);
	return;
}

void
gvinum_printconfig(int argc, char **argv)
{
	printconfig(stdout, "");
}

void
gvinum_parityop(int argc, char **argv, int rebuild)
{
	struct gctl_req *req;
	int flags, i, rv;
	off_t offset;
	const char *errstr;
	char *op, *msg;

	if (rebuild) {
		op = "rebuildparity";
		msg = "Rebuilding";
	} else {
		op = "checkparity";
		msg = "Checking";
	}

	optreset = 1;
	optind = 1;
	flags = 0;
	while ((i = getopt(argc, argv, "fv")) != -1) {
		switch (i) {
		case 'f':
			flags |= GV_FLAG_F;
			break;
		case 'v':
			flags |= GV_FLAG_V;
			break;
		case '?':
		default:
			warnx("invalid flag '%c'", i);
			return;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		warn("usage: %s [-f] [-v] <plex>", op);
		return;
	}

	do {
		rv = 0;
		req = gctl_get_handle();
		gctl_ro_param(req, "class", -1, "VINUM");
		gctl_ro_param(req, "verb", -1, "parityop");
		gctl_ro_param(req, "flags", sizeof(int), &flags);
		gctl_ro_param(req, "rebuild", sizeof(int), &rebuild);
		gctl_rw_param(req, "rv", sizeof(int), &rv);
		gctl_rw_param(req, "offset", sizeof(off_t), &offset);
		gctl_ro_param(req, "plex", -1, argv[0]);
		errstr = gctl_issue(req);
		if (errstr) {
			warnx("%s\n", errstr);
			gctl_free(req);
			break;
		}
		gctl_free(req);
		if (flags & GV_FLAG_V) {
			printf("\r%s at %s ... ", msg,
			    gv_roughlength(offset, 1));
		}
		if (rv == 1) {
			printf("Parity incorrect at offset 0x%jx\n",
			    (intmax_t)offset);
			if (!rebuild)
				break;
		}
		fflush(stdout);

		/* Clear the -f flag. */
		flags &= ~GV_FLAG_F;
	} while (rv >= 0);

	if ((rv == 2) && (flags & GV_FLAG_V)) {
		if (rebuild)
			printf("Rebuilt parity on %s\n", argv[0]);
		else
			printf("%s has correct parity\n", argv[0]);
	}
}

void
gvinum_rename(int argc, char **argv)
{
	struct gctl_req *req;
	const char *errstr;
	int flags, j;

	flags = 0;

	if (argc) {
		optreset = 1;
		optind = 1;
		while ((j = getopt(argc, argv, "r")) != -1) {
			switch (j) {
			case 'r':
				flags |= GV_FLAG_R;
				break;
			case '?':
			default:
				return;
			}
		}
		argc -= optind;
		argv += optind;
	}

	switch (argc) {
		case 0:
			warnx("no object to rename specified");
			return;
		case 1:
			warnx("no new name specified");
			return;
		case 2:
			break;
		default:
			warnx("more than one new name specified");
			return;
	}

	req = gctl_get_handle();
	gctl_ro_param(req, "class", -1, "VINUM");
	gctl_ro_param(req, "verb", -1, "rename");
	gctl_ro_param(req, "flags", sizeof(int), &flags);
	gctl_ro_param(req, "object", -1, argv[0]);
	gctl_ro_param(req, "newname", -1, argv[1]);
	errstr = gctl_issue(req); 
	if (errstr != NULL)
		warnx("can't rename object:  %s", errstr);
	gctl_free(req);
	return;
}

void
gvinum_rm(int argc, char **argv)
{
	struct gctl_req *req;
	int flags, i, j;
	const char *errstr;
	char buf[20], *cmd;

	cmd = argv[0];
	flags = 0;
	optreset = 1;
	optind = 1;
	while ((j = getopt(argc, argv, "r")) != -1) {
		switch (j) {
		case 'r':
			flags |= GV_FLAG_R;
			break;
		case '?':
		default:
			return;
		}
	}
	argc -= optind;
	argv += optind;

	req = gctl_get_handle();
	gctl_ro_param(req, "class", -1, "VINUM");
	gctl_ro_param(req, "verb", -1, "remove");
	gctl_ro_param(req, "argc", sizeof(int), &argc);
	gctl_ro_param(req, "flags", sizeof(int), &flags);
	if (argc) {
		for (i = 0; i < argc; i++) {
			snprintf(buf, sizeof(buf), "argv%d", i);
			gctl_ro_param(req, buf, -1, argv[i]);
		}
	}
	errstr = gctl_issue(req);
	if (errstr != NULL) {
		warnx("can't remove: %s", errstr);
		gctl_free(req);
		return;
	}
	gctl_free(req);
	gvinum_list(0, NULL);
}

void
gvinum_saveconfig(void)
{
	struct gctl_req *req;
	const char *errstr;

	req = gctl_get_handle();
	gctl_ro_param(req, "class", -1, "VINUM");
	gctl_ro_param(req, "verb", -1, "saveconfig");
	errstr = gctl_issue(req);
	if (errstr != NULL)
		warnx("can't save configuration: %s", errstr);
	gctl_free(req);
}

void
gvinum_start(int argc, char **argv)
{
	struct gctl_req *req;
	int i, initsize, j;
	const char *errstr;
	char buf[20];

	/* 'start' with no arguments is a no-op. */
	if (argc == 1)
		return;

	initsize = 0;

	optreset = 1;
	optind = 1;
	while ((j = getopt(argc, argv, "S")) != -1) {
		switch (j) {
		case 'S':
			initsize = atoi(optarg);
			break;
		case '?':
		default:
			return;
		}
	}
	argc -= optind;
	argv += optind;

	if (!initsize)
		initsize = 512;

	req = gctl_get_handle();
	gctl_ro_param(req, "class", -1, "VINUM");
	gctl_ro_param(req, "verb", -1, "start");
	gctl_ro_param(req, "argc", sizeof(int), &argc);
	gctl_ro_param(req, "initsize", sizeof(int), &initsize);
	if (argc) {
		for (i = 0; i < argc; i++) {
			snprintf(buf, sizeof(buf), "argv%d", i);
			gctl_ro_param(req, buf, -1, argv[i]);
		}
	}
	errstr = gctl_issue(req);
	if (errstr != NULL) {
		warnx("can't start: %s", errstr);
		gctl_free(req);
		return;
	}

	gctl_free(req);
	gvinum_list(0, NULL);
}

void
gvinum_stop(int argc, char **argv)
{
	int fileid;

	fileid = kldfind(GVINUMMOD);
	if (fileid == -1) {
		warn("cannot find " GVINUMMOD);
		return;
	}
	if (kldunload(fileid) != 0) {
		warn("cannot unload " GVINUMMOD);
		return;
	}

	warnx(GVINUMMOD " unloaded");
	exit(0);
}

void
parseline(int argc, char **argv)
{
	if (argc <= 0)
		return;

	if (!strcmp(argv[0], "create"))
		gvinum_create(argc, argv);
	else if (!strcmp(argv[0], "exit") || !strcmp(argv[0], "quit"))
		exit(0);
	else if (!strcmp(argv[0], "help"))
		gvinum_help();
	else if (!strcmp(argv[0], "list") || !strcmp(argv[0], "l"))
		gvinum_list(argc, argv);
	else if (!strcmp(argv[0], "ld"))
		gvinum_list(argc, argv);
	else if (!strcmp(argv[0], "lp"))
		gvinum_list(argc, argv);
	else if (!strcmp(argv[0], "ls"))
		gvinum_list(argc, argv);
	else if (!strcmp(argv[0], "lv"))
		gvinum_list(argc, argv);
	else if (!strcmp(argv[0], "move"))
		gvinum_move(argc, argv);
	else if (!strcmp(argv[0], "mv"))
		gvinum_move(argc, argv);
	else if (!strcmp(argv[0], "printconfig"))
		gvinum_printconfig(argc, argv);
	else if (!strcmp(argv[0], "rename"))
		gvinum_rename(argc, argv);
	else if (!strcmp(argv[0], "rm"))
		gvinum_rm(argc, argv);
	else if (!strcmp(argv[0], "saveconfig"))
		gvinum_saveconfig();
	else if (!strcmp(argv[0], "setstate"))
		gvinum_setstate(argc, argv);
	else if (!strcmp(argv[0], "start"))
		gvinum_start(argc, argv);
	else if (!strcmp(argv[0], "stop"))
		gvinum_stop(argc, argv);
	else if (!strcmp(argv[0], "checkparity"))
		gvinum_parityop(argc, argv, 0);
	else if (!strcmp(argv[0], "rebuildparity"))
		gvinum_parityop(argc, argv, 1);
	else
		printf("unknown command '%s'\n", argv[0]);

	return;
}

/*
 * The guts of printconfig.  This is called from gvinum_printconfig and from
 * gvinum_create when called without an argument, in order to give the user
 * something to edit.
 */
void
printconfig(FILE *of, char *comment)
{
	struct gctl_req *req;
	struct utsname uname_s;
	const char *errstr;
	time_t now;
	char buf[GV_CFG_LEN + 1];
	
	uname(&uname_s);
	time(&now);

	req = gctl_get_handle();
	gctl_ro_param(req, "class", -1, "VINUM");
	gctl_ro_param(req, "verb", -1, "getconfig");
	gctl_ro_param(req, "comment", -1, comment);
	gctl_rw_param(req, "config", sizeof(buf), buf);
	errstr = gctl_issue(req);
	if (errstr != NULL) {
		warnx("can't get configuration: %s", errstr);
		return;
	}
	gctl_free(req);

	fprintf(of, "# Vinum configuration of %s, saved at %s",
	    uname_s.nodename,
	    ctime(&now));
	
	if (*comment != '\0')
	    fprintf(of, "# Current configuration:\n");

	fprintf(of, buf);
}
