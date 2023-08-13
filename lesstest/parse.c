#include <stdio.h>
#include "lesstest.h"

extern int verbose;

// Return the interior string of a quoted string.
static char* parse_qstring(const char** s) {
	while (*(*s) == ' ') ++(*s);
	if (*(*s)++ != '"') return NULL;
	const char* start = *s;
	while (*(*s) != '"' && *(*s) != '\0') ++(*s);
	char* ret = strndup(start, (*s)-start);
	if (*(*s) == '"') ++(*s);
	return ret;
}

static int parse_int(const char** s) {
	return (int) strtol(*s, (char**)s, 0);
}

// Parse a quoted name and value, 
// and add them as env vars to the Setup environment.
static int parse_env(TestSetup* setup, const char* line, int line_len) {
	char* name = parse_qstring(&line);
	char* value = parse_qstring(&line);
	env_addpair(&setup->env, name, value);
	free(name);
	free(value);
	return 1;
}

static int parse_command(TestSetup* setup, const char* less, const char* line, int line_len) {
	setup->argv = (char**) malloc(32*sizeof(const char*));
	setup->argc = 1;
	setup->argv[0] = (char*) less;
	for (;;) {
		const char* arg = parse_qstring(&line);
		setup->argv[setup->argc] = (char*) arg;
		if (arg == NULL) break;
		setup->argc++;
	}
	return 1;
}

static int parse_textfile(TestSetup* setup, const char* line, int line_len, FILE* fd) {
	const char* filename = parse_qstring(&line);
	if (access(filename, F_OK) == 0) {
		fprintf(stderr, "%s already exists\n", filename);
		return 0;
	}
	int fsize = parse_int(&line);
	int len = strlen(filename)+1;
	setup->textfile = malloc(len);
	strcpy(setup->textfile, filename);
	FILE* textfd = fopen(setup->textfile, "w");
	if (textfd == NULL) {
		fprintf(stderr, "cannot create %s\n", setup->textfile);
		return 0;
	}
	int nread = 0;
	while (nread < fsize) {
		char buf[4096];
		int chunk = fsize - nread;
		if (chunk > sizeof(buf)) chunk = sizeof(buf);
		size_t len = fread(buf, 1, chunk, fd);
		fwrite(buf, 1, len, textfd);
		nread += len;
	}
	fclose(textfd);
	return 1;
}

static TestSetup* new_test_setup(void) {
	TestSetup* setup = (TestSetup*) malloc(sizeof(TestSetup));
	setup->textfile = NULL;
	setup->argv = NULL;
	setup->argc = 0;
	env_init(&setup->env);
	return setup;
}

void free_test_setup(TestSetup* setup) {
	if (setup->textfile != NULL) {
		unlink(setup->textfile);
		free(setup->textfile);
	}
	int i;
	for (i = 1; i < setup->argc; ++i)
		free(setup->argv[i]);
	free((void*)setup->argv);
	free(setup);
}

// Read a newline-terminated line from a file and store it
// as a null-terminated string without the newline.
int read_zline(FILE* fd, char* line, int line_len) {
	int nread = 0;
	while (nread < line_len-1) {
		int ch = fgetc(fd);
		if (ch == EOF) return -1;
		if (ch == '\n') break;
		line[nread++] = (char) ch;
	}
	line[nread] = '\0';
	return nread;
}

// Read the header of a .lt file (up to the R line).
TestSetup* read_test_setup(FILE* fd, const char* less) {
	TestSetup* setup = new_test_setup();
	int hdr_complete = 0;
	while (!hdr_complete) {
		char line[10000];
		int line_len = read_zline(fd, line, sizeof(line));
		if (line_len < 0)
			break;
		if (line_len < 1)
			continue;
		switch (line[0]) {
		case '!': // file header
			break;
		case 'T': // test header
			break;
		case 'R': // end of test header; start run
			hdr_complete = 1;
			break;
		case 'E': // environment variable
			if (!parse_env(setup, line+1, line_len-1)) {
				free_test_setup(setup);
				return NULL;
			}
			break;
		case 'F': // text file
			if (!parse_textfile(setup, line+1, line_len-1, fd)) {
				free_test_setup(setup);
				return NULL;
			}
			break;
		case 'A': // less cmd line parameters
			if (!parse_command(setup, less, line+1, line_len-1)) {
				free_test_setup(setup);
				return NULL;
			}
			break;
		default:
			break;
		}
	}
	if (setup->textfile == NULL || setup->argv == NULL) {
		free_test_setup(setup);
		return NULL;
	}
	if (verbose) { fprintf(stderr, "setup: textfile %s\n", setup->textfile); print_strings("argv:", setup->argv); }
	return setup;
}
