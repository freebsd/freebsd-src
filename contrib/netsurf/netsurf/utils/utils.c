/*
 * Copyright 2007 Rob Kendrick <rjek@netsurf-browser.org>
 * Copyright 2004-2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 John Tytgat <joty@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <regex.h>
#include <time.h>

#include "utils/config.h"
#include "utils/messages.h"
#include "utils/utf8.h"
#include "utils/utils.h"

char * strip(char * const s)
{
	size_t i;
	for (i = strlen(s);
			i != 0 && (s[i - 1] == ' ' || s[i - 1] == '\n' ||
			s[i - 1] == '\r' || s[i - 1] == '\t');
			i--)
		;
	s[i] = 0;
	return s + strspn(s, " \t\r\n");
}

int whitespace(const char * str)
{
	unsigned int i;
	for (i = 0; i < strlen(str); i++)
		if (!isspace(str[i]))
			return 0;
	return 1;
}

/**
 * returns a string without its underscores
 * \param replacespace true to insert a space where there was an underscore
 */

char *remove_underscores(const char *s, bool replacespace)
{
	size_t i, ii, len;
	char *ret;
	len = strlen(s);
	ret = malloc(len + 1);
	if (ret == NULL)
		return NULL;
	for (i = 0, ii = 0; i < len; i++) {
		if (s[i] != '_')
			ret[ii++] = s[i];
		else if (replacespace)
			ret[ii++] = ' ';
	}
	ret[ii] = '\0';
	return ret;
}

/**
 * Replace consecutive whitespace with a single space.
 *
 * @todo determine if squash_whitespace utf-8 safe and that it needs to be
 *
 * \param  s  source string
 * \return  heap allocated result, or NULL on memory exhaustion
 */

char *squash_whitespace(const char *s)
{
	char *c;
	int i = 0, j = 0;

	c = malloc(strlen(s) + 1);
	if (c != NULL) {
		do {
			if (s[i] == ' ' ||
			    s[i] == '\n' ||
			    s[i] == '\r' ||
			    s[i] == '\t') {
				c[j++] = ' ';
				while (s[i] == ' ' ||
				       s[i] == '\n' ||
				       s[i] == '\r' ||
				       s[i] == '\t')
					i++;
			}
			c[j++] = s[i++];
		} while (s[i - 1] != 0);
	}
	return c;
}


/**
 * Converts NUL terminated UTF-8 encoded string s containing zero or more
 * spaces (char 32) or TABs (char 9) to non-breaking spaces
 * (0xC2 + 0xA0 in UTF-8 encoding).
 *
 * Caller needs to free() result.  Returns NULL in case of error.  No
 * checking is done on validness of the UTF-8 input string.
 */
char *cnv_space2nbsp(const char *s)
{
	const char *srcP;
	char *d, *d0;
	unsigned int numNBS;
	/* Convert space & TAB into non breaking space character (0xA0) */
	for (numNBS = 0, srcP = (const char *)s; *srcP != '\0'; ++srcP)
		if (*srcP == ' ' || *srcP == '\t')
			++numNBS;
	if ((d = (char *)malloc((srcP - s) + numNBS + 1)) == NULL)
		return NULL;
	for (d0 = d, srcP = (const char *)s; *srcP != '\0'; ++srcP) {
		if (*srcP == ' ' || *srcP == '\t') {
			*d0++ = 0xC2;
			*d0++ = 0xA0;
		} else
			*d0++ = *srcP;
	}
	*d0 = '\0';
	return d;
}

/**
 * Check if a directory exists.
 */

bool is_dir(const char *path)
{
	struct stat s;

	if (stat(path, &s))
		return false;

	return S_ISDIR(s.st_mode) ? true : false;
}


/**
 * Compile a regular expression, handling errors.
 *
 * Parameters as for regcomp(), see man regex.
 */

void regcomp_wrapper(regex_t *preg, const char *regex, int cflags)
{
	int r;
	r = regcomp(preg, regex, cflags);
	if (r) {
		char errbuf[200];
		regerror(r, preg, errbuf, sizeof errbuf);
		fprintf(stderr, "Failed to compile regexp '%s'\n", regex);
		die(errbuf);
	}
}

/** We can have a fairly good estimate of how long the buffer needs to
  * be.  The unsigned long can store a value representing a maximum size
  * of around 4 GB.  Therefore the greatest space required is to
  * represent 1023MB.  Currently that would be represented as "1023MB" so 12
  * including a null terminator.
  * Ideally we would be able to know this value for sure, in the mean
  * time the following should suffice.
 **/

#define BYTESIZE_BUFFER_SIZE 20

/**
  * Does a simple conversion which assumes the user speaks English.  The buffer
  * returned is one of three static ones so may change each time this call is
  * made.  Don't store the buffer for later use.  It's done this way for
  * convenience and to fight possible memory leaks, it is not necessarily pretty.
 **/

char *human_friendly_bytesize(unsigned long bsize) {
	static char buffer1[BYTESIZE_BUFFER_SIZE];
	static char buffer2[BYTESIZE_BUFFER_SIZE];
	static char buffer3[BYTESIZE_BUFFER_SIZE];
	static char *curbuffer = buffer3;
	enum {bytes, kilobytes, megabytes, gigabytes} unit = bytes;
	static char units[][7] = {"Bytes", "kBytes", "MBytes", "GBytes"};

	float bytesize = (float)bsize;

	if (curbuffer == buffer1)
		curbuffer = buffer2;
	else if (curbuffer == buffer2)
		curbuffer = buffer3;
	else
		curbuffer = buffer1;

	if (bytesize > 1024) {
		bytesize /= 1024;
		unit = kilobytes;
	}

	if (bytesize > 1024) {
		bytesize /= 1024;
		unit = megabytes;
	}

	if (bytesize > 1024) {
		bytesize /= 1024;
		unit = gigabytes;
	}

	sprintf(curbuffer, "%3.2f%s", bytesize, messages_get(units[unit]));

	return curbuffer;
}

/**
 * Create an RFC 1123 compliant date string from a Unix timestamp
 *
 * \param t The timestamp to consider
 * \return Pointer to buffer containing string - invalidated by next call.
 */
const char *rfc1123_date(time_t t)
{
	static char ret[30];

	struct tm *tm = gmtime(&t);
	const char *days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" },
		*months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
				"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	snprintf(ret, sizeof ret, "%s, %02d %s %d %02d:%02d:%02d GMT",
			days[tm->tm_wday], tm->tm_mday, months[tm->tm_mon],
			tm->tm_year + 1900, tm->tm_hour, tm->tm_min,
			tm->tm_sec);

	return ret;
}

/**
 * Returns a number of centiseconds, that increases in real time, for the
 * purposes of measuring how long something takes in wall-clock terms.  It uses
 * gettimeofday() for this.  Should the call to gettimeofday() fail, it returns
 * zero.
 *
 * \return number of centiseconds that increases monotonically
 */
unsigned int wallclock(void)
{
	struct timeval tv;

	if (gettimeofday(&tv, NULL) == -1)
		return 0;

	return ((tv.tv_sec * 100) + (tv.tv_usec / 10000));
}

#ifndef HAVE_STRCASESTR

/**
 * Case insensitive strstr implementation
 *
 * \param haystack String to search in
 * \param needle String to look for
 * \return Pointer to start of found substring, or NULL if not found
 */
char *strcasestr(const char *haystack, const char *needle)
{
	size_t needle_len = strlen(needle);
	const char * last_start = haystack + (strlen(haystack) - needle_len);

	while (haystack <= last_start) {
		if (strncasecmp(haystack, needle, needle_len) == 0)
			return (char *)haystack;
		haystack++;
	}

	return NULL;
}

#endif

#ifndef HAVE_STRNDUP

/**
 * Duplicate up to n characters of a string.
 */

char *strndup(const char *s, size_t n)
{
	size_t len;
	char *s2;

	for (len = 0; len != n && s[len]; len++)
		continue;

	s2 = malloc(len + 1);
	if (!s2)
		return 0;

	memcpy(s2, s, len);
	s2[len] = 0;
	return s2;
}

#endif


/* Exported interface, documented in utils.c */
int dir_sort_alpha(const struct dirent **d1, const struct dirent **d2)
{
	const char *s1 = (*d1)->d_name;
	const char *s2 = (*d2)->d_name;

	while (*s1 != '\0' && *s2 != '\0') {
		if ((*s1 >= '0' && *s1 <= '9') &&
				(*s2 >= '0' && *s2 <= '9')) {
			int n1 = 0,  n2 = 0;
			while (*s1 >= '0' && *s1 <= '9') {
				n1 = n1 * 10 + (*s1) - '0';
				s1++;
			}
			while (*s2 >= '0' && *s2 <= '9') {
				n2 = n2 * 10 + (*s2) - '0';
				s2++;
			}
			if (n1 != n2) {
				return n1 - n2;
			}
			if (*s1 == '\0' || *s2 == '\0')
				break;
		}
		if (tolower(*s1) != tolower(*s2))
			break;

		s1++;
		s2++;
	}

	return tolower(*s1) - tolower(*s2);
}


#ifndef HAVE_SCANDIR
int alphasort(const struct dirent **d1, const struct dirent **d2)
{
	return strcasecmp((*d1)->d_name, (*d2)->d_name);
}

int scandir(const char *dir, struct dirent ***namelist,
		int (*sel)(const struct dirent *),
		int (*compar)(const struct dirent **, const struct dirent **))
{
	struct dirent **entlist = NULL;
	struct dirent **entlist_temp = NULL;
	struct dirent *ent = NULL, *new_ent;
	int alloc_n = 0;
	int n = 0;
	DIR *d;

	d = opendir(dir);
	if (d == NULL) {
		goto error;
	}

	while ((ent = readdir(d)) != NULL) {
		/* Avoid entries that caller doesn't want */
		if (sel && (*sel)(ent) == 0)
			continue;

		/* Ensure buffer is big enough to list this entry */
		if (n == alloc_n) {
			alloc_n *= 4;
			if (alloc_n == 0) {
				alloc_n = 32;
			}
			entlist_temp = realloc(entlist,
					sizeof(*entlist) * alloc_n);
			if (entlist_temp == NULL) {
				goto error;
			}
			entlist = entlist_temp;
		}

		/* Make copy of ent */
		new_ent = malloc(sizeof(*new_ent));
		if (new_ent == NULL) {
			goto error;
		}
		memcpy(new_ent, ent, sizeof(struct dirent));

		/* Make list entry point to this copy of ent */
		entlist[n] = new_ent;

		n++;
	}

	closedir(d);

	/* Sort */
	if (compar != NULL && n > 1)
		qsort(entlist, n, sizeof(*entlist),
				(int (*)(const void *, const void *))compar);
	*namelist = entlist;
	return n;

error:

	if (entlist != NULL) {
		int i;
		for (i = 0; i < n; i++) {
			free(entlist[i]);
		}
		free(entlist);
	}

	if (d != NULL) {
		closedir(d);
	}

	return -1;
}

#endif


#ifndef HAVE_STRCHRNUL

/**
 *  Find the first occurrence of C in S or the final NUL byte.
 */
char *strchrnul (const char *s, int c_in)
{
	const unsigned char *us = (const unsigned char *) s;

	while (*us != c_in && *us != '\0')
		us++;

	return (void *) us;
}

#endif

#ifndef HAVE_UTSNAME
#include "utils/utsname.h"

int uname(struct utsname *buf) {
	strcpy(buf->sysname,"windows");
	strcpy(buf->nodename,"nodename");
	strcpy(buf->release,"release");
	strcpy(buf->version,"version");
	strcpy(buf->machine,"pc");
	
	return 0;
}
#endif

#ifndef HAVE_REALPATH
char *realpath(const char *path, char *resolved_path)
{
	char *ret;
	if (resolved_path == NULL) {
		ret=strdup(path);
	} else {
		ret = resolved_path;
		strcpy(resolved_path, path);
	}
	return ret;
}

#ifndef HAVE_INETATON


int inet_aton(const char *cp, struct in_addr *inp)
{
	unsigned int b1, b2, b3, b4;
	unsigned char c;

	if (strspn(cp, "0123456789.") < strlen(cp)) 
		return 0;

	if (sscanf(cp, "%3u.%3u.%3u.%3u%c", &b1, &b2, &b3, &b4, &c) != 4)
		return 0;

	if ((b1 > 255) || (b2 > 255) || (b3 > 255) || (b4 > 255))
		return 0;

	inp->s_addr = b4 << 24 | b3 << 16 | b2 << 8 | b1;

	return 1;
}

#endif

#ifndef HAVE_INETPTON

int inet_pton(int af, const char *src, void *dst)
{
	int ret;

	if (af == AF_INET) {
		ret = inet_aton(src, dst);
	} 
#if !defined(NO_IPV6)
	else if (af == AF_INET6) {
		/* TODO: implement v6 address support */
		ret = -1;
		errno = EAFNOSUPPORT; 
	} 
#endif
	else {
		ret = -1;
		errno = EAFNOSUPPORT; 
	}

	return ret;
}

#endif


#endif
