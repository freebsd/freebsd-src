#ifndef test_testutils_h_
#define test_testutils_h_

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

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

static size_t css__parse_strlen(const char *str, size_t limit);
char *css__parse_strnchr(const char *str, size_t len, int chr);
bool css__parse_testfile(const char *filename, line_func callback, void *pw);
size_t css__parse_filesize(const char *filename);

/**
 * Testcase datafile parser driver
 *
 * \param filename  Name of file to parse
 * \param callback  Pointer to function to handle each line of input data
 * \param pw        Pointer to client-specific private data
 * \return true on success, false otherwise.
 */
bool css__parse_testfile(const char *filename, line_func callback, void *pw)
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

		if (!callback(buf, css__parse_strlen(buf, sizeof buf - 1), pw)) {
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
size_t css__parse_strlen(const char *str, size_t limit)
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
 * Length-limited strchr
 *
 * \param str  String to search in
 * \param len  Length of string
 * \param chr  Character to search for
 * \return Pointer to character in string, or NULL if not found
 */
char *css__parse_strnchr(const char *str, size_t len, int chr)
{
	size_t i;

	if (str == NULL)
		return NULL;

	for (i = 0; i < len; i++) {
		if (str[i] == chr)
			break;
	}

	if (i == len)
		return NULL;

	return (char *) str + i;
}

/**
 * Read the size of a file
 *
 * \param filename  Name of file to read size of
 * \return File size (in bytes), or 0 on error
 */
size_t css__parse_filesize(const char *filename)
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


/**
 * Convert a string representation of an error name to a LibCSS error code
 *
 * \param str  String containing error name
 * \param len  Length of string (bytes)
 * \return LibCSS error code, or CSS_OK if unknown
 */
css_error css_error_from_string(const char *str, size_t len);
css_error css_error_from_string(const char *str, size_t len)
{
	if (strncmp(str, "CSS_OK", len) == 0) {
		return CSS_OK;
	} else if (strncmp(str, "CSS_NOMEM", len) == 0) {
		return CSS_NOMEM;
	} else if (strncmp(str, "CSS_BADPARM", len) == 0) {
		return CSS_BADPARM;
	} else if (strncmp(str, "CSS_INVALID", len) == 0) {
		return CSS_INVALID;
	} else if (strncmp(str, "CSS_FILENOTFOUND", len) == 0) {
		return CSS_FILENOTFOUND;
	} else if (strncmp(str, "CSS_NEEDDATA", len) == 0) {
		return CSS_NEEDDATA;
	} else if (strncmp(str, "CSS_BADCHARSET", len) == 0) {
		return CSS_BADCHARSET;
	} else if (strncmp(str, "CSS_EOF", len) == 0) {
		return CSS_EOF;
	}

	return CSS_OK;
}

#endif
