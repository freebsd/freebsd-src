#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include "lesstest.h"

#define RD 0
#define WR 1

extern int verbose;
extern char* lt_screen;
extern char* lt_screen_opts;
static const int run_less = 1;

// Make 2 specified file descriptors be stdin and stdout.
static void dup_std(int fd0, int fd1) {
	if (fd0 >= 0) dup2(fd0, 0);
	if (fd1 >= 0) dup2(fd1, 1);
}

static const char* basename(const char* path) {
	const char* slash = strrchr(path, '/');
	if (slash == NULL) return path;
	return slash+1;
}

// Exec an instance of less in the current process.
static void become_child_less(char* less, int argc, char* const* argv, char* const* envp, const char* tempfile, int less_in_pipe[2], int screen_in_pipe[2]) {
	if (verbose) fprintf(stderr, "less child: in %d, out %d, close %d,%d\n", less_in_pipe[RD], screen_in_pipe[WR], less_in_pipe[WR], screen_in_pipe[RD]);
	close(less_in_pipe[WR]);
	close(screen_in_pipe[RD]);
	dup_std(less_in_pipe[RD], screen_in_pipe[WR]);

	char** less_argv = malloc(sizeof(char*) * (argc + 6));
	int less_argc = 0;
	less_argv[less_argc++] = less;
	less_argv[less_argc++] = "--tty";
	less_argv[less_argc++] = "/dev/stdin";
	while (--argc > 0) {
		char* arg = *++argv;
		less_argv[less_argc++] = (argc > 1 || tempfile == NULL) ? arg : (char*) tempfile;
	}
	less_argv[less_argc] = NULL;
	if (verbose) { print_strings("less argv", less_argv); print_strings("less envp", envp); }
	execve(less, less_argv, envp);
	fprintf(stderr, "cannot exec %s: %s\n", less, strerror(errno));
	exit(1);
}

// Exec an instance of lt_screen in the current process.
static void become_child_screen(char* lt_screen, int screen_width, int screen_height, int screen_in_pipe[2], int screen_out_pipe[2]) {
	if (verbose) fprintf(stderr, "screen child: in %d, out %d, close %d\n", screen_in_pipe[RD], screen_out_pipe[WR], screen_out_pipe[RD]);
	close(screen_out_pipe[RD]);
	dup_std(screen_in_pipe[RD], screen_out_pipe[WR]);

	char* screen_argv[10];
	int screen_argc = 0;
	char sw[16];
	char sh[16];
	screen_argv[screen_argc++] = lt_screen;
	if (screen_width >= 0) {
		snprintf(sw, sizeof(sw), "%d", screen_width);
		screen_argv[screen_argc++] = "-w";
		screen_argv[screen_argc++] = sw;
	}
	if (screen_height >= 0) {
		snprintf(sh, sizeof(sh), "%d", screen_height);
		screen_argv[screen_argc++] = "-h";
		screen_argv[screen_argc++] = sh;
	}
	if (lt_screen_opts != NULL) {
		screen_argv[screen_argc++] = lt_screen_opts;
	}
	if (1)
		screen_argv[screen_argc++] = "-q";
	screen_argv[screen_argc] = NULL;
	if (verbose) print_strings("screen argv", screen_argv);
	char* const screen_envp[] = { NULL };
	execve(lt_screen, screen_argv, screen_envp);
	fprintf(stderr, "cannot exec %s: %s\n", lt_screen, strerror(errno));
	exit(1);
}

// Create an empty LessPipeline.
static LessPipeline* new_pipeline(void) {
	LessPipeline* pipeline = malloc(sizeof(LessPipeline));
	pipeline->less_in_pipe[RD] = pipeline->less_in_pipe[WR] = -1;
	pipeline->screen_in_pipe[RD] = pipeline->screen_in_pipe[WR] = -1;
	pipeline->screen_out_pipe[RD] = pipeline->screen_out_pipe[WR] = -1;
	pipeline->less_in = pipeline->screen_out = -1;
	pipeline->tempfile = NULL;
	pipeline->screen_pid = 0;
	pipeline->screen_width = pipeline->screen_height = 0;
	return pipeline;
}

// Create a LessPipeline.
LessPipeline* create_less_pipeline(char* const* argv, int argc, char* const* envp) {
	// If textfile contains a slash, create a temporary link from 
	// the named text file to its basename, and run less on the link.
	LessPipeline* pipeline = new_pipeline();
	const char* textfile = argv[argc-1];
	const char* textbase = basename(textfile);
	if (textbase != textfile) {
		pipeline->tempfile = textbase;
		if (link(textfile, textbase) < 0) {
			fprintf(stderr, "cannot link %s to %s: %s\n", textfile, textbase, strerror(errno));
			return NULL;
		}
		textfile = textbase;
	}
	if (pipe(pipeline->screen_in_pipe) < 0) {
		destroy_less_pipeline(pipeline);
		return NULL;
	}
	const char* w = get_envp(envp, "COLUMNS");
	const char* h = get_envp(envp, "LINES");
	if (w != NULL) pipeline->screen_width = atoi(w);
	if (h != NULL) pipeline->screen_height = atoi(h);
	if (verbose) fprintf(stderr, "less out pipe %d,%d\n", pipeline->screen_in_pipe[0], pipeline->screen_in_pipe[1]);
	if (run_less) { 
		if (pipe(pipeline->less_in_pipe) < 0) {
			destroy_less_pipeline(pipeline);
			return 0;
		}
		if (verbose) fprintf(stderr, "less in pipe %d,%d\n", pipeline->less_in_pipe[RD], pipeline->less_in_pipe[WR]);
		char* less = argv[0];
		if (verbose) fprintf(stderr, "testing %s on %s\n", less, textfile);
		pipeline->less_pid = fork();
		if (pipeline->less_pid < 0) {
			destroy_less_pipeline(pipeline);
			return NULL;
		}
		if (!pipeline->less_pid)
			become_child_less(less, argc, argv, envp, pipeline->tempfile, pipeline->less_in_pipe, pipeline->screen_in_pipe);
		if (verbose) fprintf(stderr, "less child %ld\n", (long) pipeline->less_pid);
		close(pipeline->less_in_pipe[RD]); pipeline->less_in_pipe[RD] = -1;
		close(pipeline->screen_in_pipe[WR]); pipeline->screen_in_pipe[WR] = -1;
	}
	if (pipe(pipeline->screen_out_pipe) < 0) {
		destroy_less_pipeline(pipeline);
		return NULL;
	}
	if (verbose) fprintf(stderr, "screen out pipe %d,%d\n", pipeline->screen_out_pipe[RD], pipeline->screen_out_pipe[WR]);
	pipeline->screen_pid = fork();
	if (!pipeline->screen_pid) // child: lt_screen
		become_child_screen(lt_screen, pipeline->screen_width, pipeline->screen_height, pipeline->screen_in_pipe, pipeline->screen_out_pipe);
	if (verbose) fprintf(stderr, "screen child %ld\n", (long) pipeline->screen_pid);
	close(pipeline->screen_out_pipe[WR]); pipeline->screen_out_pipe[WR] = -1;
	close(pipeline->screen_in_pipe[RD]); pipeline->screen_in_pipe[RD] = -1;

	pipeline->less_in = run_less ? pipeline->less_in_pipe[WR] : pipeline->screen_in_pipe[WR];
	pipeline->screen_out = pipeline->screen_out_pipe[RD];
	if (verbose) fprintf(stderr, "less in %d, screen out %d, pid %ld\n", pipeline->less_in, pipeline->screen_out, (long) pipeline->screen_pid);
	return pipeline;
}

void destroy_less_pipeline(LessPipeline* pipeline) {
	close(pipeline->less_in);
	close(pipeline->screen_out);
	close(pipeline->less_in_pipe[RD]); close(pipeline->less_in_pipe[WR]);
	close(pipeline->screen_in_pipe[RD]); close(pipeline->screen_in_pipe[WR]);
	close(pipeline->screen_out_pipe[RD]); close(pipeline->screen_out_pipe[WR]);
	if (pipeline->tempfile != NULL)
		unlink(pipeline->tempfile);
	free(pipeline);
}
