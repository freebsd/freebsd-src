/*
 * Copyright (C) 1984-2026  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

#include <setjmp.h>
#include "lesstest.h"

extern TermInfo terminfo;

int verbose = 0;
int less_quit = 0;
int details = 0;
char* details_file = NULL;
int err_only = 0;
char* lt_screen = "./lt_screen";
char* lt_screen_opts = NULL;

static char* testfile = NULL;
static char* keyfile = NULL;

static int usage(void) {
	fprintf(stderr, "%s",
	"usage: lesstest [options] [--] less.exe [flags] textfile\n"
	"   or: lesstest [options] -t file.lt [--] less.exe\n"
	"       Options:\n"
	"          -d       print details of any screen mismatches (test failures)\n"
	"          -D FILE  dump details of screen mismatches to file in ltview format\n"
	"          -e       do not print PASS messages\n"
	"          -h NUM   height of emulated screen\n"
	"          -k FILE  read input keys from file rather than keyboard\n"
	"          -o FILE  name of .lt file to create\n"
	"          -s FILE  name of lt_screen program\n"
	"          -S OPTS  options to pass to lt_screen program\n"
	"          -t FILE  name of .lt file to test\n"
	"          -v       print verbose messages\n"
	"          -w NUM   width of emulated screen\n"
	);
	return 0;
}

static int setup(int argc, char* const* argv) {
	char* logfile = NULL;
	int ch;
	while ((ch = getopt(argc, argv, "dD:ek:o:s:S:t:v")) != -1) {
		switch (ch) {
		case 'd':
			details = 1;
			break;
		case 'D':
			details_file = optarg;
			break;
		case 'e':
			err_only = 1;
			break;
        case 'k':
            keyfile = optarg;
            break;
		case 'o':
			logfile = optarg;
			break;
		case 's':
			lt_screen = optarg;
			break;
		case 'S':
			lt_screen_opts = optarg;
			break;
		case 't':
			testfile = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			return usage();
		}
	}
	if (logfile != NULL && !log_open(logfile)) {
		fprintf(stderr, "cannot create %s: %s\n", logfile, strerror(errno));
		return 0;
	}
	return 1;
}

int main(int argc, char* const* argv, char* const* envp) {
	if (!setup(argc, argv))
		return RUN_ERR;
	int ok = 0;
	if (testfile != NULL) { // run existing test
        if (optind+1 != argc) {
            usage();
        } else {
            ok = run_testfile(testfile, argv[optind]);
        }
	} else { // gen; create new test
		if (optind+2 > argc) {
			usage();
		} else {
			log_file_header();
			ok = run_interactive(argv+optind, argc-optind, envp, keyfile);
			log_close();
		}
	}
	return ok ? RUN_OK : RUN_ERR;
}
