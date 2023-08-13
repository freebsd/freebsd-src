#include <setjmp.h>
#include "lesstest.h"

extern TermInfo terminfo;

int verbose = 0;
int less_quit = 0;
int details = 0;
int err_only = 0;
char* lt_screen = "./lt_screen";
char* lt_screen_opts = NULL;

static char* testfile = NULL;

static int usage(void) {
	fprintf(stderr, "usage: lesstest -o file.lt [-w#] [-h#] [-eEdv] [-S lt_screen-opts] [--] less.exe [flags] textfile\n");
	fprintf(stderr, "   or: lesstest -t file.lt less.exe\n");
	return 0;
}

static int setup(int argc, char* const* argv) {
	char* logfile = NULL;
	int ch;
	while ((ch = getopt(argc, argv, "deEo:s:S:t:v")) != -1) {
		switch (ch) {
		case 'd':
			details = 1;
			break;
		case 'e':
			err_only = 1;
			break;
		case 'E':
			err_only = 2;
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
			return RUN_ERR;
		}
		ok = run_testfile(testfile, argv[optind]);
	} else { // gen; create new test
		if (optind+2 > argc) {
			usage();
			return RUN_ERR;
		}
		log_file_header();
		ok = run_interactive(argv+optind, argc-optind, envp);
		log_close();
	}
	return ok ? RUN_OK : RUN_ERR;
}
