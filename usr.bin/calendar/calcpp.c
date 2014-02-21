/*-
 * Copyright (c) 2013 Diane Bruce
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* calendar fake cpp does a very limited cpp version */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <langinfo.h>
#include <locale.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"
#include "calendar.h"

#define MAXFPSTACK	50
static FILE *fpstack[MAXFPSTACK];
static int curfpi;
static void pushfp(FILE *fp);
static FILE *popfp(void);
static int tokenscpp(char *buf, char *string);

#define T_INVALID	-1
#define T_INCLUDE	0
#define T_DEFINE	1
#define T_IFNDEF	2
#define T_ENDIF		3

#define MAXSYMS		100
static char *symtable[MAXSYMS];
static void addsym(const char *name);
static int findsym(const char *name);

FILE *
fincludegets(char *buf, int size, FILE *fp)
{
	char name[MAXPATHLEN];
	FILE *nfp=NULL;
	char *p;
	int ch;

	if (fp == NULL)
		return(NULL);

	if (fgets(buf, size, fp) == NULL) {
		*buf = '\0';
		fclose(fp);
		fp = popfp();
		return (fp);
	}
	if ((p = strchr(buf, '\n')) != NULL)
		*p = '\0';
	else {
		/* Flush this line */
		while ((ch = fgetc(fp)) != '\n' && ch != EOF);
		if (ch == EOF) {
			*buf = '\0';
			fclose(fp);
			fp = popfp();
			return(fp);
		}
	}
	switch (tokenscpp(buf, name)) {
	case T_INCLUDE:
		*buf = '\0';
		if ((nfp = fopen(name, "r")) != NULL) {
			pushfp(fp);
			fp = nfp;
		}
		break;
	case T_DEFINE:
		addsym(name);
		break;
	case T_IFNDEF:
		if (findsym(name)) {
			fclose(fp);
			fp = popfp();
			*buf = '\0';
		}
		break;
	case T_ENDIF:
		*buf = '\0';
		break;
	default:
		break;
	}
	return (fp);
}

static int
tokenscpp(char *buf, char *string)
{
	char *p;
	char *s;

	if ((p = strstr(buf, "#define")) != NULL) {
		p += 8;
		while (isspace((unsigned char)*p))
			p++;
		s = p;
		while(!isspace((unsigned char)*p))
			p++;
		strlcpy(string, s, MAXPATHLEN);
		return(T_DEFINE);
	} else if ((p = strstr(buf, "#ifndef")) != NULL) {
		p += 8;
		while (isspace((unsigned char)*p))
			p++;
		s = p;
		while(!isspace((unsigned char)*p))
			p++;
		*p = '\0';
		strncpy(string, s, MAXPATHLEN);
		return(T_IFNDEF);
	} else if ((p = strstr(buf, "#endif")) != NULL) {
		return(T_ENDIF);
	} if ((p = strstr(buf, "#include")) != NULL) {
		p += 8;
		while (isspace((unsigned char)*p))
			p++;
		if (*p == '<') {
			s = p+1;
			if ((p = strchr(s, '>')) != NULL)
				*p = '\0';
			if (*s != '/')
				snprintf (string, MAXPATHLEN, "%s/%s",
					_PATH_INCLUDE, s);
			else
				strncpy(string, s, MAXPATHLEN);
		} else if (*p == '(') {
			s = p+1;
			if ((p = strchr(p, '>')) != NULL)
				*p = '\0';
			snprintf (string, MAXPATHLEN, "%s", s);
		}
		return(T_INCLUDE);
	}
	return(T_INVALID);
}

static void
pushfp(FILE *fp)
{
	curfpi++;
	if (curfpi == MAXFPSTACK)
		errx(1, "Max #include reached");
	fpstack[curfpi] = fp;
}

static
FILE *popfp(void)
{
	FILE *tmp;

	assert(curfpi >= 0);
	tmp = fpstack[curfpi];
	curfpi--;
	return(tmp);
}

void
initcpp(void)
{
	int i;

	for (i=0; i < MAXSYMS; i++)
		symtable[i] = NULL;
	fpstack[0] = NULL;
	curfpi = 0;
}


static void
addsym(const char *name)
{
	int i;
	
	if (!findsym(name))
		for (i=0; i < MAXSYMS; i++) {
			if (symtable[i] == NULL) {
				symtable[i] = strdup(name);
				if (symtable[i] == NULL)
					errx(1, "malloc error in addsym");
				return;
			}
		}
	errx(1, "symbol table full\n");
}

static int
findsym(const char *name)
{
	int i;
	
	for (i=0; i < MAXSYMS; i++)
		if (symtable[i] != NULL && strcmp(symtable[i],name) == 0)
			return (1);
	return (0);
}
