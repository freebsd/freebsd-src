/*
 * Copyright (c) 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <kvm.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* libgdb stuff. */
#include <defs.h>
#include <frame.h>
#include <inferior.h>
#include <interps.h>
#include <cli-out.h>
#include <main.h>
#include <target.h>
#include <top.h>

extern void (*init_ui_hook)(char *);

extern void symbol_file_add_main (char *args, int from_tty);

#include "kgdb.h"

kvm_t *kvm;

static int dumpnr;
static int verbose;

static char crashdir[PATH_MAX];
static char *kernel;
static char *vmcore;

static void (*kgdb_new_objfile_chain)(struct objfile * objfile);

static void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-v] [-d crashdir] [-n dumpnr] [kernel [core]]\n",
	    getprogname());
	exit(1);
}

static void
use_dump(int nr)
{
	char path[PATH_MAX];
	FILE *info;
	char *s;
	struct stat st;
	int l;

	snprintf(path, sizeof(path), "%s/vmcore.%d", crashdir, nr);
	if (stat(path, &st) == -1)
		err(1, path);
	if (!S_ISREG(st.st_mode))
		errx(1, "%s: not a regular file", path);

	vmcore = strdup(path);

	/*
	 * See if there's a kernel image right here, use it. The kernel
	 * image is either called kernel.<nr> or is in a subdirectory
	 * kernel.<nr> and called kernel.
	 */
	snprintf(path, sizeof(path), "%s/kernel.%d", crashdir, nr);
	if (stat(path, &st) == 0) {
		if (S_ISREG(st.st_mode)) {
			kernel = strdup(path);
			return;
		}
		if (S_ISDIR(st.st_mode)) {
			snprintf(path, sizeof(path), "%s/kernel.%d/kernel",
			    crashdir, nr);
			if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
				kernel = strdup(path);
				return;
			}
		}
	}

	/*
	 * No kernel image here. Parse the dump header. The kernel object
	 * directory can be found there and we probably have the kernel
	 * image in it still.
	 */
	snprintf(path, sizeof(path), "%s/info.%d", crashdir, nr);
	info = fopen(path, "r");
	if (info == NULL) {
		warn(path);
		return;
	}
	while (fgets(path, sizeof(path), info) != NULL) {
		l = strlen(path);
		if (l > 0 && path[l - 1] == '\n')
			path[--l] = '\0';
		if (strncmp(path, "    ", 4) == 0) {
			s = strchr(path, ':');
			s = (s == NULL) ? path + 4 : s + 1;
			l = snprintf(path, sizeof(path), "%s/kernel.debug", s);
			if (stat(path, &st) == -1 || !S_ISREG(st.st_mode)) {
				path[l - 6] = '\0';
				if (stat(path, &st) == -1 ||
				    !S_ISREG(st.st_mode))
					break;
			}
			kernel = strdup(path);
			break;
		}
	}
	fclose(info);

	if (verbose && kernel == NULL)
		warnx("dump %d: no kernel found", nr);
}

static void
kgdb_new_objfile(struct objfile *objfile)
{
#if 0
	printf("XXX: %s(%p)\n", __func__, objfile);
	if (objfile != NULL) {
		goto out;
	}

out:
#endif
	if (kgdb_new_objfile_chain != NULL)
		kgdb_new_objfile_chain(objfile);
}

static void
kgdb_interp_command_loop(void *data)
{
	static int once = 0;

	if (!once) {
		symbol_file_add_main (kernel, 0);
		print_stack_frame(get_current_frame(), -1, 0);
		once = 1;
	}
	command_loop();
}

static void
kgdb_init(char *argv0 __unused)
{
	static struct interp_procs procs = {
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		kgdb_interp_command_loop
	};
	struct interp *kgdb;
	kgdb = interp_new("kgdb", NULL, cli_out_new(gdb_stdout), &procs);
	interp_add(kgdb);

	set_prompt("(kgdb) ");
	kgdb_target();
	kgdb_new_objfile_chain = target_new_objfile_hook;
	target_new_objfile_hook = kgdb_new_objfile;
}

int
main(int argc, char *argv[])
{
	struct captured_main_args args;
	char *s;
	int ch;

	dumpnr = -1;

	strlcpy(crashdir, "/var/crash", sizeof(crashdir));
	s = getenv("KGDB_CRASH_DIR");
	if (s != NULL)
		strlcpy(crashdir, s, sizeof(crashdir));

	while ((ch = getopt(argc, argv, "d:n:v")) != -1) {
		switch (ch) {
		case 'd':
			strlcpy(crashdir, optarg, sizeof(crashdir));
			break;
		case 'n':
			dumpnr = strtol(optarg, &s, 0);
			if (dumpnr < 0 || *s != '\0') {
				warnx("option %c: invalid kernel dump number",
				    optopt);
				usage();
				/* NOTREACHED */
			}
			break;
		case 'v':
			verbose++;
			break;
		case '?':
		default:
			usage();
		}
	}

	if (verbose > 1)
		warnx("using %s as the crash directory", crashdir);

	if (dumpnr >= 0)
		use_dump(dumpnr);

	if (argc > optind) {
		if (kernel != NULL)
			free(kernel);
		kernel = strdup(argv[optind++]);
	}
	while (argc > optind) {
		if (vmcore != NULL)
			errx(1, "multiple core files specified");
		vmcore = strdup(argv[optind++]);
	}

	if (kernel == NULL)
		errx(1, "kernel not specified");
	if (vmcore == NULL)
		errx(1, "core file not specified");

	if (verbose) {
		warnx("kernel image: %s", kernel);
		warnx("core file: %s", vmcore);
	}

	s = malloc(_POSIX2_LINE_MAX);
	kvm = kvm_openfiles(kernel, vmcore, NULL, O_RDONLY, s);
	if (kvm == NULL)
		errx(1, s);
	free(s);

	kgdb_thr_init();

	memset (&args, 0, sizeof args);
	args.argc = 1;
	args.argv = argv;
	args.use_windows = 0;
	args.interpreter_p = "kgdb";

	init_ui_hook = kgdb_init;

	return (gdb_main(&args));
}
