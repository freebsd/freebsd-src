#ifndef test_testutils_h_
#define test_testutils_h_

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef UNUSED
#define UNUSED(x) ((x) = (x))
#endif

/* Redefine assert, so we can simply use the standard assert mechanism
 * within testcases and exit with the right output for the testrunner
 * to do the right thing. */
void __assert2(const char *expr, const char *function,
		const char *file, int line);

void __assert2(const char *expr, const char *function,
		const char *file, int line)
{
	UNUSED(function);
	UNUSED(file);

	printf("FAIL - %s at line %d\n", expr, line);

	exit(EXIT_FAILURE);
}

#define assert(expr) \
  ((void) ((expr) || (__assert2 (#expr, __func__, __FILE__, __LINE__), 0)))


typedef bool (*line_func)(const char *data, size_t datalen, void *pw);

static size_t parse_strlen(const char *str, size_t limit);
bool parse_testfile(const char *filename, line_func callback, void *pw);
size_t parse_filesize(const char *filename);

/**
 * Testcase datafile parser driver
 *
 * \param filename  Name of file to parse
 * \param callback  Pointer to function to handle each line of input data
 * \param pw        Pointer to client-specific private data
 * \return true on success, false otherwise.
 */
bool parse_testfile(const char *filename, line_func callback, void *pw)
{
	FILE *fp;
	char buf[300];

	fp = fopen(filename, "rb");
	if (fp == NULL) {
		printf("Failed opening %s\n", filename);
		return false;
	}

	while (fgets(buf, sizeof buf, fp)) {
		if (buf[0] == '\n')
			continue;

		if (!callback(buf, parse_strlen(buf, sizeof buf - 1), pw)) {
			fclose(fp);
			return false;
		}
	}

	fclose(fp);

	return true;
}

/**
 * Utility string length measurer; assumes strings are '\n' terminated
 *
 * \param str    String to measure length of
 * \param limit  Upper bound on string length
 * \return String length
 */
size_t parse_strlen(const char *str, size_t limit)
{
	size_t len = 0;

	if (str == NULL)
		return 0;

	while (len < limit - 1 && *str != '\n') {
		len++;
		str++;
	}

	len++;

	return len;
}

/**
 * Read the size of a file
 *
 * \param filename  Name of file to read size of
 * \return File size (in bytes), or 0 on error
 */
size_t parse_filesize(const char *filename)
{
	FILE *fp;
	size_t len = 0;

	fp = fopen(filename, "rb");
	if (fp == NULL) {
		printf("Failed opening %s\n", filename);
		return 0;
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);

	fclose(fp);

	return len;
}


#endif
