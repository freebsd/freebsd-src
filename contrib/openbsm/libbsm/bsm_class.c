/*
 * Copyright (c) 2004 Apple Computer, Inc.
 * Copyright (c) 2006 Robert N. M. Watson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $P4: //depot/projects/trustedbsd/openbsm/libbsm/bsm_class.c#11 $
 */

#include <bsm/libbsm.h>

#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Parse the contents of the audit_class file to return struct au_class_ent
 * entries.
 */
static FILE		*fp = NULL;
static char		 linestr[AU_LINE_MAX];
static const char	*classdelim = ":";

static pthread_mutex_t	mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Parse a single line from the audit_class file passed in str to the struct
 * au_class_ent elements; store the result in c.
 */
static struct au_class_ent *
classfromstr(char *str, struct au_class_ent *c)
{
	char *classname, *classdesc, *classflag;
	char *last;

	/* Each line contains flag:name:desc. */
	classflag = strtok_r(str, classdelim, &last);
	classname = strtok_r(NULL, classdelim, &last);
	classdesc = strtok_r(NULL, classdelim, &last);

	if ((classflag == NULL) || (classname == NULL) || (classdesc == NULL))
		return (NULL);

	/*
	 * Check for very large classnames.
	 */
	if (strlen(classname) >= AU_CLASS_NAME_MAX)
		return (NULL);

	strcpy(c->ac_name, classname);

	/*
	 * Check for very large class description.
	 */
	if (strlen(classdesc) >= AU_CLASS_DESC_MAX)
		return (NULL);
	strcpy(c->ac_desc, classdesc);
	c->ac_class = strtoul(classflag, (char **) NULL, 0);

	return (c);
}

/*
 * Return the next au_class_ent structure from the file setauclass should be
 * called before invoking this function for the first time.
 *
 * Must be called with mutex held.
 */
static struct au_class_ent *
getauclassent_r_locked(struct au_class_ent *c)
{
	char *tokptr, *nl;

	if ((fp == NULL) && ((fp = fopen(AUDIT_CLASS_FILE, "r")) == NULL))
		return (NULL);

	/*
	 * Read until next non-comment line is found, or EOF.
	 */
	while (1) {
		if (fgets(linestr, AU_LINE_MAX, fp) == NULL)
			return (NULL);

		/* Skip comments. */
		if (linestr[0] == '#')
			continue;

		/* Remove trailing new line character. */
		if ((nl = strrchr(linestr, '\n')) != NULL)
			*nl = '\0';

		/* Parse tokptr to au_class_ent components. */
		tokptr = linestr;
		if (classfromstr(tokptr, c) == NULL)
			return (NULL);
		break;
	}

	return (c);
}

struct au_class_ent *
getauclassent_r(struct au_class_ent *c)
{
	struct au_class_ent *cp;

	pthread_mutex_lock(&mutex);
	cp = getauclassent_r_locked(c);
	pthread_mutex_unlock(&mutex);
	return (cp);
}

struct au_class_ent *
getauclassent(void)
{
	static char class_ent_name[AU_CLASS_NAME_MAX];
	static char class_ent_desc[AU_CLASS_DESC_MAX];
	static struct au_class_ent c, *cp;

	bzero(&c, sizeof(c));
	bzero(class_ent_name, sizeof(class_ent_name));
	bzero(class_ent_desc, sizeof(class_ent_desc));
	c.ac_name = class_ent_name;
	c.ac_desc = class_ent_desc;

	pthread_mutex_lock(&mutex);
	cp = getauclassent_r_locked(&c);
	pthread_mutex_unlock(&mutex);
	return (cp);
}

/*
 * Rewind to the beginning of the enumeration.
 *
 * Must be called with mutex held.
 */
static void
setauclass_locked(void)
{

	if (fp != NULL)
		fseek(fp, 0, SEEK_SET);
}

void
setauclass(void)
{

	pthread_mutex_lock(&mutex);
	setauclass_locked();
	pthread_mutex_unlock(&mutex);
}

/*
 * Return the next au_class_entry having the given class name.
 */
struct au_class_ent *
getauclassnam_r(struct au_class_ent *c, const char *name)
{
	struct au_class_ent *cp;

	if (name == NULL)
		return (NULL);

	pthread_mutex_lock(&mutex);
	setauclass_locked();
	while ((cp = getauclassent_r_locked(c)) != NULL) {
		if (strcmp(name, cp->ac_name) == 0) {
			pthread_mutex_unlock(&mutex);
			return (cp);
		}
	}
	pthread_mutex_unlock(&mutex);
	return (NULL);
}

struct au_class_ent *
getauclassnam(const char *name)
{
	static char class_ent_name[AU_CLASS_NAME_MAX];
	static char class_ent_desc[AU_CLASS_DESC_MAX];
	static struct au_class_ent c;

	bzero(&c, sizeof(c));
	bzero(class_ent_name, sizeof(class_ent_name));
	bzero(class_ent_desc, sizeof(class_ent_desc));
	c.ac_name = class_ent_name;
	c.ac_desc = class_ent_desc;

	return (getauclassnam_r(&c, name));
}


/*
 * Return the next au_class_entry having the given class number.
 *
 * OpenBSM extension.
 */
struct au_class_ent *
getauclassnum_r(struct au_class_ent *c, au_class_t class_number)
{
	struct au_class_ent *cp;

	pthread_mutex_lock(&mutex);
	setauclass_locked();
	while ((cp = getauclassent_r_locked(c)) != NULL) {
		if (class_number == cp->ac_class)
			return (cp);
	}
	pthread_mutex_unlock(&mutex);
	return (NULL);
}

struct au_class_ent *
getauclassnum(au_class_t class_number)
{
	static char class_ent_name[AU_CLASS_NAME_MAX];
	static char class_ent_desc[AU_CLASS_DESC_MAX];
	static struct au_class_ent c;

	bzero(&c, sizeof(c));
	bzero(class_ent_name, sizeof(class_ent_name));
	bzero(class_ent_desc, sizeof(class_ent_desc));
	c.ac_name = class_ent_name;
	c.ac_desc = class_ent_desc;

	return (getauclassnum_r(&c, class_number));
}

/*
 * audit_class processing is complete; close any open files.
 */
void
endauclass(void)
{

	pthread_mutex_lock(&mutex);
	if (fp != NULL) {
		fclose(fp);
		fp = NULL;
	}
	pthread_mutex_unlock(&mutex);
}
