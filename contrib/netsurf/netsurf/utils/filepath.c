/*
 * Copyright 2010 Vincent Sanders <vince@kyllikki.org>
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

/** \file
 * Provides utility functions for finding readable files.
 *
 * These functions are intended to make finding resource files more straightforward.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "utils/config.h"
#include "utils/filepath.h"

/** maximum number of elements in the resource vector */
#define MAX_RESPATH 128 

/* exported interface documented in filepath.h */
char *filepath_vsfindfile(char *str, const char *format, va_list ap)
{
	char *realpathname;
	char *pathname;
	int len;

	pathname = malloc(PATH_MAX);
	if (pathname == NULL)
		return NULL; /* unable to allocate memory */

	len = vsnprintf(pathname, PATH_MAX, format, ap);

	if ((len < 0) || (len >= PATH_MAX)) {
		/* error or output exceeded PATH_MAX length so
		 * operation is doomed to fail.
		 */
		free(pathname);
		return NULL;
	}

	realpathname = realpath(pathname, str);
	
	free(pathname);
	
	if (realpathname != NULL) {
		/* sucessfully expanded pathname */
		if (access(realpathname, R_OK) != 0) {
			/* unable to read the file */
			return NULL;
		}	
	}

	return realpathname;
}

/* exported interface documented in filepath.h */
char *filepath_sfindfile(char *str, const char *format, ...)
{
	va_list ap;
	char *ret;

	va_start(ap, format);
	ret = filepath_vsfindfile(str, format, ap);
	va_end(ap);

	return ret;
}

/* exported interface documented in filepath.h */
char *filepath_findfile(const char *format, ...)
{
	char *ret;
	va_list ap;

	va_start(ap, format);
	ret = filepath_vsfindfile(NULL, format, ap);
	va_end(ap);

	return ret;
}

/* exported interface documented in filepath.h */
char *filepath_sfind(char **respathv, char *filepath, const char *filename)
{
	int respathc = 0;

	if ((respathv == NULL) || (respathv[0] == NULL) || (filepath == NULL))
		return NULL;

	while (respathv[respathc] != NULL) {
		if (filepath_sfindfile(filepath, "%s/%s", respathv[respathc], filename) != NULL) {
			return filepath;
		}

		respathc++;
	}

	return NULL;
}

/* exported interface documented in filepath.h */
char *filepath_find(char **respathv, const char *filename)
{
	char *ret;
	char *filepath;

	if ((respathv == NULL) || (respathv[0] == NULL))
		return NULL;

	filepath = malloc(PATH_MAX);
	if (filepath == NULL)
		return NULL;

	ret = filepath_sfind(respathv, filepath, filename);

	if (ret == NULL)
		free(filepath);

	return ret;
}

/* exported interface documented in filepath.h */
char *filepath_sfinddef(char **respathv, char *filepath, const char *filename, const char *def)
{
	char t[PATH_MAX];
	char *ret;

	if ((respathv == NULL) || (respathv[0] == NULL) || (filepath == NULL))
		return NULL;

	ret = filepath_sfind(respathv, filepath, filename);

	if ((ret == NULL) && (def != NULL)) {
		/* search failed, return the path specified */
		ret = filepath;
		if (def[0] == '~') {
			snprintf(t, PATH_MAX, "%s/%s/%s", getenv("HOME"), def + 1, filename);
		} else {
			snprintf(t, PATH_MAX, "%s/%s", def, filename);
		}		
		if (realpath(t, ret) == NULL) {
			strcpy(ret, t);
		}

	}
	return ret;
}


/* exported interface documented in filepath.h */
char **
filepath_generate(char * const *pathv, const char * const *langv)
{
	char **respath; /* resource paths vector */
	int pathc = 0;
	int langc = 0;
	int respathc = 0;
	struct stat dstat;
	char tmppath[PATH_MAX];

	respath = calloc(MAX_RESPATH, sizeof(char *));

	while (pathv[pathc] != NULL) {
		if ((stat(pathv[pathc], &dstat) == 0) && 
		    S_ISDIR(dstat.st_mode)) {
			/* path element exists and is a directory */
			langc = 0;
			while (langv[langc] != NULL) {
				snprintf(tmppath, sizeof tmppath, "%s/%s", pathv[pathc],langv[langc]);
				if ((stat(tmppath, &dstat) == 0) && 
				    S_ISDIR(dstat.st_mode)) {
					/* path element exists and is a directory */
					respath[respathc++] = strdup(tmppath);
				}
				langc++;
			}
			respath[respathc++] = strdup(pathv[pathc]);
		}
		pathc++;
	}

	return respath;
}

/* expand ${} in a string into environment variables */
static char *
expand_path(const char *path, int pathlen)
{
	char *exp;
	int explen;
	int cstart = -1;
	int cloop = 0;
	char *envv;
	int envlen;
	int replen; /* length of replacement */

	exp = malloc(pathlen + 1);
	if (exp == NULL)
		return NULL;

	memcpy(exp, path, pathlen);
	exp[pathlen] = 0;

	explen = pathlen;

	while (exp[cloop] != 0) {
		if ((exp[cloop] == '$') && 
		    (exp[cloop + 1] == '{')) {
			cstart = cloop;
			cloop++;
		} 
		
		if ((cstart != -1) &&
		    (exp[cloop] == '}')) {
			replen = cloop - cstart;
			exp[cloop] = 0;
			envv = getenv(exp + cstart + 2);
			if (envv == NULL) {
				memmove(exp + cstart, 
					exp + cloop + 1, 
					explen - cloop);
				explen -= replen;
			} else {
				envlen = strlen(envv);
				exp = realloc(exp, explen + envlen - replen);
				memmove(exp + cstart + envlen, 
					exp + cloop + 1, 
					explen - cloop );
				memmove(exp + cstart, envv, envlen);
				explen += envlen - replen;
			}
			cloop -= replen;
			cstart = -1;
		}

		cloop++;
	}

	if (explen == 1) {
		free(exp);
		exp = NULL;
	}

	return exp;
}

/* exported interface documented in filepath.h */
char **
filepath_path_to_strvec(const char *path)
{
	char **strvec;
	int strc = 0;
	const char *estart; /* path element start */
	const char *eend; /* path element end */
	int elen;

	strvec = calloc(MAX_RESPATH, sizeof(char *));
	if (strvec == NULL)
		return NULL;

	estart = eend = path;

	while (strc < (MAX_RESPATH - 2)) {
		while ( (*eend != 0) && (*eend != ':') )
			eend++;
		elen = eend - estart;

		if (elen > 1) {
			/* more than an empty colon */
			strvec[strc] = expand_path(estart, elen);
			if (strvec[strc] != NULL) {
				/* successfully expanded an element */
				strc++;
			}
		}

		/* skip colons */
		while (*eend == ':')
			eend++;

		/* check for termination */
		if (*eend == 0)
			break;
		
		estart = eend;
	}
	return strvec;
}

/* exported interface documented in filepath.h */
void filepath_free_strvec(char **pathv)
{
	int p = 0;

	while (pathv[p] != NULL) {
		free(pathv[p++]);
	}
	free(pathv);
}
