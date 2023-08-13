#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include "lesstest.h"

static FILE* logf = NULL;

int log_open(const char* logfile) {
	if (logf != NULL) fclose(logf);
	logf = (strcmp(logfile, "-") == 0) ? stdout : fopen(logfile, "w");
	if (logf == NULL) {
		fprintf(stderr, "cannot create %s\n", logfile);
		return 0;
	}
	return 1;
}

void log_close(void) {
	if (logf == NULL) return;
	if (logf == stdout) return;
	fclose(logf);
	logf = NULL;
}

int log_file_header(void) {
	if (logf == NULL) return 0;
	time_t now = time(NULL);
	struct tm* tm = gmtime(&now);
	fprintf(logf, "!lesstest!\n!version %d\n!created %d-%02d-%02d %02d:%02d:%02d\n",
		LESSTEST_VERSION, 
		tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, 
		tm->tm_hour, tm->tm_min, tm->tm_sec);
	return 1;
}

int log_env(const char* name, int namelen, const char* value) {
	if (logf == NULL) return 0;
	fprintf(logf, "E \"%.*s\" \"%s\"\n", namelen, name, value);
	return 1;
}

int log_tty_char(wchar ch) {
	if (logf == NULL) return 0;
	fprintf(logf, "+%lx\n", ch);
	return 1;
}

int log_screen(const byte* img, int len) {
	if (logf == NULL) return 0;
	fwrite("=", 1, 1, logf);
	fwrite(img, 1, len, logf);
	fwrite("\n", 1, 1, logf);
	return 1;
}

#if 0
int log_debug(char const* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	fprintf(logf, "D ");
	vfprintf(logf, fmt, ap);
	fprintf(logf, "\n");
	va_end(ap);
	fflush(logf);
	return 1;
}
#endif

int log_command(char* const* argv, int argc, const char* textfile) {
	if (logf == NULL) return 0;
	fprintf(logf, "A");
	int a;
	for (a = 1; a < argc; ++a)
		fprintf(logf, " \"%s\"", (a < argc-1) ? argv[a] : textfile);
	fprintf(logf, "\n");
	return 1;
}

int log_textfile(const char* textfile) {
	if (logf == NULL) return 0;
	struct stat st;
	if (stat(textfile, &st) < 0) {
		fprintf(stderr, "cannot stat %s\n", textfile);
		return 0;
	}
	FILE* fd = fopen(textfile, "r");
	if (fd == NULL) {
		fprintf(stderr, "cannot open %s\n", textfile);
		return 0;
	}
	fprintf(logf, "F \"%s\" %ld\n", textfile, (long) st.st_size);
	off_t nread = 0;
	while (nread < st.st_size) {
		char buf[4096];
		size_t n = fread(buf, 1, sizeof(buf), fd);
		if (n <= 0) {
			fprintf(stderr, "read only %ld/%ld from %s\n", (long) nread, (long) st.st_size, textfile);
			fclose(fd);
			return 0;
		}
		nread += n;
		fwrite(buf, 1, n, logf);
	}
	fclose(fd);
	return 1;
}

int log_test_header(char* const* argv, int argc, const char* textfile) {
	if (logf == NULL) return 0;
	fprintf(logf, "T \"%s\"\n", textfile);
	if (!log_command(argv, argc, textfile))
		return 0;
	if (!log_textfile(textfile))
		return 0;
	fprintf(logf, "R\n");
	return 1;
}

int log_test_footer(void) {
	if (logf == NULL) return 0;
	fprintf(logf, "Q\n");
	return 1;
}
