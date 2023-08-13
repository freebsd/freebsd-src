#include <time.h>
#include <errno.h>
#include <setjmp.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include "lesstest.h"

extern int verbose;
extern int less_quit;
extern int details;
extern int err_only;
extern TermInfo terminfo;

static pid_t less_pid;
static jmp_buf run_catch;

static void set_signal(int signum, void (*handler)(int)) {
	struct sigaction sa;
	sa.sa_handler = handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(signum, &sa, NULL);
}

static void child_handler(int signum) {
	int status;
	pid_t child = wait(&status);
	if (verbose) fprintf(stderr, "child %d died, status 0x%x\n", child, status);
	if (child == less_pid) {
		if (verbose) fprintf(stderr, "less died\n");
		less_quit = 1;
	}
}

static void set_signal_handlers(int set) {
	set_signal(SIGINT,  set ? SIG_IGN : SIG_DFL);
	set_signal(SIGQUIT, set ? SIG_IGN : SIG_DFL);
	set_signal(SIGKILL, set ? SIG_IGN : SIG_DFL);
	set_signal(SIGPIPE, set ? SIG_IGN : SIG_DFL);
	set_signal(SIGCHLD, set ? child_handler : SIG_DFL);
}

// Send a command char to a LessPipeline.
static void send_char(LessPipeline* pipeline, wchar ch) {
	if (verbose) fprintf(stderr, "lt.send %lx\n", ch);
	byte cbuf[UNICODE_MAX_BYTES];
	byte* cp = cbuf;
	store_wchar(&cp, ch);
	write(pipeline->less_in, cbuf, cp-cbuf);
}

// Read the screen image from the lt_screen in a LessPipeline.
static int read_screen(LessPipeline* pipeline, byte* buf, int buflen) {
	if (verbose) fprintf(stderr, "lt.gen: read screen\n");
	send_char(pipeline, LESS_DUMP_CHAR);
	int rn = 0;
	for (; rn <= buflen; ++rn) {
		byte ch;
		if (read(pipeline->screen_out, &ch, 1) != 1)
			break;
		if (ch == '\n')
			break;
		if (buf != NULL) buf[rn] = ch;
	}
	return rn;
}

// Read screen image from a LessPipeline and display it.
static void read_and_display_screen(LessPipeline* pipeline) {
	byte rbuf[MAX_SCREENBUF_SIZE];
	int rn = read_screen(pipeline, rbuf, sizeof(rbuf));
	if (rn == 0) return;
	printf("%s", terminfo.clear_screen);
	display_screen(rbuf, rn, pipeline->screen_width, pipeline->screen_height);
	log_screen(rbuf, rn);
}

// Is the screen image in a LessPipeline equal to a given buffer?
static int curr_screen_match(LessPipeline* pipeline, const byte* img, int imglen) {
	byte curr[MAX_SCREENBUF_SIZE];
	int currlen = read_screen(pipeline, curr, sizeof(curr));
	if (currlen == imglen && memcmp(img, curr, imglen) == 0)
		return 1;
	if (details) {
		fprintf(stderr, "lt: mismatch: expect:\n");
		display_screen_debug(img, imglen, pipeline->screen_width, pipeline->screen_height);
		fprintf(stderr, "lt: got:\n");
		display_screen_debug(curr, currlen, pipeline->screen_width, pipeline->screen_height);
	}
	return 0;
}

// Run an interactive lesstest session to create an lt file.
// Read individual chars from stdin and send them to a LessPipeline.
// After each char, read the LessPipeline screen and display it 
// on the user's screen. 
// Also log the char and the screen image in the lt file.
int run_interactive(char* const* argv, int argc, char* const* prog_envp) {
	setup_term();
	char* const* envp = less_envp(prog_envp, 1);
	LessPipeline* pipeline = create_less_pipeline(argv, argc, envp);
	if (pipeline == NULL)
		return 0;
	less_pid = pipeline->less_pid;
	const char* textfile = (pipeline->tempfile != NULL) ? pipeline->tempfile : argv[argc-1];
	if (!log_test_header(argv, argc, textfile)) {
		destroy_less_pipeline(pipeline);
		return 0;
	}
	set_signal_handlers(1);
	less_quit = 0;
	int ttyin = 0; // stdin
	raw_mode(ttyin, 1);
	printf("%s%s", terminfo.init_term, terminfo.enter_keypad);
	read_and_display_screen(pipeline);
	while (!less_quit) {
		wchar ch = read_wchar(ttyin);
		if (ch == terminfo.backspace_key)
			ch = '\b';
		if (verbose) fprintf(stderr, "tty %c (%lx)\n", pr_ascii(ch), ch);
		log_tty_char(ch);
		send_char(pipeline, ch);
		read_and_display_screen(pipeline);
	}
	log_test_footer();
	printf("%s%s%s", terminfo.clear_screen, terminfo.exit_keypad, terminfo.deinit_term);
	raw_mode(ttyin, 0);
	destroy_less_pipeline(pipeline);
	set_signal_handlers(0);
	return 1;
}

// Run a test of less, as directed by an open lt file.
// Read a logged char and screen image from the lt file.
// Send the char to a LessPipeline, then read the LessPipeline screen image
// and compare it to the screen image from the lt file.
// Report an error if they differ.
static int run_test(TestSetup* setup, FILE* testfd) {
	const char* setup_name = setup->argv[setup->argc-1];
	//fprintf(stderr, "RUN  %s\n", setup_name);
	LessPipeline* pipeline = create_less_pipeline(setup->argv, setup->argc, 
			less_envp(setup->env.env_list, 0));
	if (pipeline == NULL)
		return 0;
	less_quit = 0;
	wchar last_char = 0;
	int ok = 1;
	int cmds = 0;
	if (setjmp(run_catch)) {
		fprintf(stderr, "\nINTR test interrupted\n");
		ok = 0;
	} else {
		set_signal_handlers(1);
		(void) read_screen(pipeline, NULL, MAX_SCREENBUF_SIZE); // wait until less is running
		while (!less_quit) {
			char line[10000];
			int line_len = read_zline(testfd, line, sizeof(line));
			if (line_len < 0)
				break;
			if (line_len < 1)
				continue;
			switch (line[0]) {
			case '+':
				last_char = (wchar) strtol(line+1, NULL, 16);
				send_char(pipeline, last_char);
				++cmds;
				break;
			case '=': 
				if (!curr_screen_match(pipeline, (byte*)line+1, line_len-1)) {
					ok = 0;
					less_quit = 1;
					fprintf(stderr, "DIFF %s on cmd #%d (%c %lx)\n",
						setup_name, cmds, pr_ascii(last_char), last_char);
				}
				break;
			case 'Q':
				less_quit = 1;
				break;
			case '\n':
			case '!':
				break;
			default:
				fprintf(stderr, "unrecognized char at start of \"%s\"\n", line);
				return 0;
			}
		}
		set_signal_handlers(0);
	}
	destroy_less_pipeline(pipeline);
	if (!ok)
		printf("FAIL: %s (%d steps)\n", setup_name, cmds);
	else if (!err_only)
		printf("PASS: %s (%d steps)\n", setup_name, cmds);
	return ok;
}

// Run a test of less, as directed by a named lt file.
// Should be run in an empty temp directory;
// it creates its own files in the current directory.
int run_testfile(const char* ltfile, const char* less) {
	FILE* testfd = fopen(ltfile, "r");
	if (testfd == NULL) {
		fprintf(stderr, "cannot open %s\n", ltfile);
		return 0;
	}
	int tests = 0;
	int fails = 0;
	// This for loop is to handle multiple tests in one file.
	for (;;) {
		TestSetup* setup = read_test_setup(testfd, less);
		if (setup == NULL)
			break;
		++tests;
		int ok = run_test(setup, testfd);
		free_test_setup(setup);
		if (!ok) ++fails;
	}
#if 0
	fprintf(stderr, "DONE %d test%s", tests, tests==1?"":"s");
	if (tests > fails)  fprintf(stderr, ", %d ok",  tests-fails);
	if (fails > 0)      fprintf(stderr, ", %d failed", fails);
	fprintf(stderr, "\n");
#endif
	fclose(testfd);
	return (fails == 0);
}
