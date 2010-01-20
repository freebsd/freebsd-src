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
 * $P4: //depot/projects/trustedbsd/openbsm/libbsm/bsm_control.c#16 $
 */

#include <bsm/libbsm.h>

#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <config/config.h>
#ifndef HAVE_STRLCAT
#include <compat/strlcat.h>
#endif

/*
 * Parse the contents of the audit_control file to return the audit control
 * parameters.  These static fields are protected by 'mutex'.
 */
static FILE	*fp = NULL;
static char	linestr[AU_LINE_MAX];
static char	*delim = ":";

static char	inacdir = 0;
static char	ptrmoved = 0;

static pthread_mutex_t	mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Returns the string value corresponding to the given label from the
 * configuration file.
 *
 * Must be called with mutex held.
 */
static int
getstrfromtype_locked(char *name, char **str)
{
	char *type, *nl;
	char *tokptr;
	char *last;

	*str = NULL;

	if ((fp == NULL) && ((fp = fopen(AUDIT_CONTROL_FILE, "r")) == NULL))
		return (-1); /* Error */

	while (1) {
		if (fgets(linestr, AU_LINE_MAX, fp) == NULL) {
			if (ferror(fp))
				return (-1);
			return (0);	/* EOF */
		}

		if (linestr[0] == '#')
			continue;

		/* Remove trailing new line character. */
		if ((nl = strrchr(linestr, '\n')) != NULL)
			*nl = '\0';

		tokptr = linestr;
		if ((type = strtok_r(tokptr, delim, &last)) != NULL) {
			if (strcmp(name, type) == 0) {
				/* Found matching name. */
				*str = strtok_r(NULL, delim, &last);
				if (*str == NULL) {
					errno = EINVAL;
					return (-1); /* Parse error in file */
				}
				return (0); /* Success */
			}
		}
	}
}

/*
 * Convert a policy to a string.  Return -1 on failure, or >= 0 representing
 * the actual size of the string placed in the buffer (excluding terminating
 * nul).
 */
ssize_t
au_poltostr(long policy, size_t maxsize, char *buf)
{
	int first;

	if (maxsize < 1)
		return (-1);
	first = 1;
	buf[0] = '\0';

	if (policy & AUDIT_CNT) {
		if (strlcat(buf, "cnt", maxsize) >= maxsize)
			return (-1);
		first = 0;
	}
	if (policy & AUDIT_AHLT) {
		if (!first) {
			if (strlcat(buf, ",", maxsize) >= maxsize)
				return (-1);
		}
		if (strlcat(buf, "ahlt", maxsize) >= maxsize)
			return (-1);
		first = 0;
	}
	if (policy & AUDIT_ARGV) {
		if (!first) {
			if (strlcat(buf, ",", maxsize) >= maxsize)
				return (-1);
		}
		if (strlcat(buf, "argv", maxsize) >= maxsize)
			return (-1);
		first = 0;
	}
	if (policy & AUDIT_ARGE) {
		if (!first) {
			if (strlcat(buf, ",", maxsize) >= maxsize)
				return (-1);
		}
		if (strlcat(buf, "arge", maxsize) >= maxsize)
			return (-1);
		first = 0;
	}
	if (policy & AUDIT_SEQ) {
		if (!first) {
			if (strlcat(buf, ",", maxsize) >= maxsize)
				return (-1);
		}
		if (strlcat(buf, "seq", maxsize) >= maxsize)
			return (-1);
		first = 0;
	}
	if (policy & AUDIT_WINDATA) {
		if (!first) {
			if (strlcat(buf, ",", maxsize) >= maxsize)
				return (-1);
		}
		if (strlcat(buf, "windata", maxsize) >= maxsize)
			return (-1);
		first = 0;
	}
	if (policy & AUDIT_USER) {
		if (!first) {
			if (strlcat(buf, ",", maxsize) >= maxsize)
				return (-1);
		}
		if (strlcat(buf, "user", maxsize) >= maxsize)
			return (-1);
		first = 0;
	}
	if (policy & AUDIT_GROUP) {
		if (!first) {
			if (strlcat(buf, ",", maxsize) >= maxsize)
				return (-1);
		}
		if (strlcat(buf, "group", maxsize) >= maxsize)
			return (-1);
		first = 0;
	}
	if (policy & AUDIT_TRAIL) {
		if (!first) {
			if (strlcat(buf, ",", maxsize) >= maxsize)
				return (-1);
		}
		if (strlcat(buf, "trail", maxsize) >= maxsize)
			return (-1);
		first = 0;
	}
	if (policy & AUDIT_PATH) {
		if (!first) {
			if (strlcat(buf, ",", maxsize) >= maxsize)
				return (-1);
		}
		if (strlcat(buf, "path", maxsize) >= maxsize)
			return (-1);
		first = 0;
	}
	if (policy & AUDIT_SCNT) {
		if (!first) {
			if (strlcat(buf, ",", maxsize) >= maxsize)
				return (-1);
		}
		if (strlcat(buf, "scnt", maxsize) >= maxsize)
			return (-1);
		first = 0;
	}
	if (policy & AUDIT_PUBLIC) {
		if (!first) {
			if (strlcat(buf, ",", maxsize) >= maxsize)
				return (-1);
		}
		if (strlcat(buf, "public", maxsize) >= maxsize)
			return (-1);
		first = 0;
	}
	if (policy & AUDIT_ZONENAME) {
		if (!first) {
			if (strlcat(buf, ",", maxsize) >= maxsize)
				return (-1);
		}
		if (strlcat(buf, "zonename", maxsize) >= maxsize)
			return (-1);
		first = 0;
	}
	if (policy & AUDIT_PERZONE) {
		if (!first) {
			if (strlcat(buf, ",", maxsize) >= maxsize)
				return (-1);
		}
		if (strlcat(buf, "perzone", maxsize) >= maxsize)
			return (-1);
		first = 0;
	}
	return (strlen(buf));
}

/*
 * Convert a string to a policy.  Return -1 on failure (with errno EINVAL,
 * ENOMEM) or 0 on success.
 */
int
au_strtopol(const char *polstr, long *policy)
{
	char *bufp, *string;
	char *buffer;

	*policy = 0;
	buffer = strdup(polstr);
	if (buffer == NULL)
		return (-1);

	bufp = buffer;
	while ((string = strsep(&bufp, ",")) != NULL) {
		if (strcmp(string, "cnt") == 0)
			*policy |= AUDIT_CNT;
		else if (strcmp(string, "ahlt") == 0)
			*policy |= AUDIT_AHLT;
		else if (strcmp(string, "argv") == 0)
			*policy |= AUDIT_ARGV;
		else if (strcmp(string, "arge") == 0)
			*policy |= AUDIT_ARGE;
		else if (strcmp(string, "seq") == 0)
			*policy |= AUDIT_SEQ;
		else if (strcmp(string, "winau_fstat") == 0)
			*policy |= AUDIT_WINDATA;
		else if (strcmp(string, "user") == 0)
			*policy |= AUDIT_USER;
		else if (strcmp(string, "group") == 0)
			*policy |= AUDIT_GROUP;
		else if (strcmp(string, "trail") == 0)
			*policy |= AUDIT_TRAIL;
		else if (strcmp(string, "path") == 0)
			*policy |= AUDIT_PATH;
		else if (strcmp(string, "scnt") == 0)
			*policy |= AUDIT_SCNT;
		else if (strcmp(string, "public") == 0)
			*policy |= AUDIT_PUBLIC;
		else if (strcmp(string, "zonename") == 0)
			*policy |= AUDIT_ZONENAME;
		else if (strcmp(string, "perzone") == 0)
			*policy |= AUDIT_PERZONE;
		else {
			free(buffer);
			errno = EINVAL;
			return (-1);
		}
	}
	free(buffer);
	return (0);
}

/*
 * Rewind the file pointer to beginning.
 */
static void
setac_locked(void)
{

	ptrmoved = 1;
	if (fp != NULL)
		fseek(fp, 0, SEEK_SET);
}

void
setac(void)
{

	pthread_mutex_lock(&mutex);
	setac_locked();
	pthread_mutex_unlock(&mutex);
}

/*
 * Close the audit_control file.
 */
void
endac(void)
{

	pthread_mutex_lock(&mutex);
	ptrmoved = 1;
	if (fp != NULL) {
		fclose(fp);
		fp = NULL;
	}
	pthread_mutex_unlock(&mutex);
}

/*
 * Return audit directory information from the audit control file.
 */
int
getacdir(char *name, int len)
{
	char *dir;
	int ret = 0;

	/*
	 * Check if another function was called between successive calls to
	 * getacdir.
	 */
	pthread_mutex_lock(&mutex);
	if (inacdir && ptrmoved) {
		ptrmoved = 0;
		if (fp != NULL)
			fseek(fp, 0, SEEK_SET);
		ret = 2;
	}
	if (getstrfromtype_locked(DIR_CONTROL_ENTRY, &dir) < 0) {
		pthread_mutex_unlock(&mutex);
		return (-2);
	}
	if (dir == NULL) {
		pthread_mutex_unlock(&mutex);
		return (-1);
	}
	if (strlen(dir) >= len) {
		pthread_mutex_unlock(&mutex);
		return (-3);
	}
	strcpy(name, dir);
	pthread_mutex_unlock(&mutex);
	return (ret);
}

/*
 * Return the minimum free diskspace value from the audit control file.
 */
int
getacmin(int *min_val)
{
	char *min;

	pthread_mutex_lock(&mutex);
	setac_locked();
	if (getstrfromtype_locked(MINFREE_CONTROL_ENTRY, &min) < 0) {
		pthread_mutex_unlock(&mutex);
		return (-2);
	}
	if (min == NULL) {
		pthread_mutex_unlock(&mutex);
		return (1);
	}
	*min_val = atoi(min);
	pthread_mutex_unlock(&mutex);
	return (0);
}

/*
 * Return the desired trail rotation size from the audit control file.
 */
int
getacfilesz(size_t *filesz_val)
{
	char *filesz, *dummy;
	long long ll;

	pthread_mutex_lock(&mutex);
	setac_locked();
	if (getstrfromtype_locked(FILESZ_CONTROL_ENTRY, &filesz) < 0) {
		pthread_mutex_unlock(&mutex);
		return (-2);
	}
	if (filesz == NULL) {
		pthread_mutex_unlock(&mutex);
		errno = EINVAL;
		return (1);
	}
	ll = strtoll(filesz, &dummy, 10);
	if (*dummy != '\0') {
		pthread_mutex_unlock(&mutex);
		errno = EINVAL;
		return (-1);
	}
	/*
	 * The file size must either be 0 or >= MIN_AUDIT_FILE_SIZE.  0
	 * indicates no rotation size.
	 */
	if (ll < 0 || (ll > 0 && ll < MIN_AUDIT_FILE_SIZE)) {
		pthread_mutex_unlock(&mutex);
		errno = EINVAL;
		return (-1);
	}
	*filesz_val = ll;
	pthread_mutex_unlock(&mutex);
	return (0);
}

/*
 * Return the system audit value from the audit contol file.
 */
int
getacflg(char *auditstr, int len)
{
	char *str;

	pthread_mutex_lock(&mutex);
	setac_locked();
	if (getstrfromtype_locked(FLAGS_CONTROL_ENTRY, &str) < 0) {
		pthread_mutex_unlock(&mutex);
		return (-2);
	}
	if (str == NULL) {
		pthread_mutex_unlock(&mutex);
		return (1);
	}
	if (strlen(str) >= len) {
		pthread_mutex_unlock(&mutex);
		return (-3);
	}
	strcpy(auditstr, str);
	pthread_mutex_unlock(&mutex);
	return (0);
}

/*
 * Return the non attributable flags from the audit contol file.
 */
int
getacna(char *auditstr, int len)
{
	char *str;

	pthread_mutex_lock(&mutex);
	setac_locked();
	if (getstrfromtype_locked(NA_CONTROL_ENTRY, &str) < 0) {
		pthread_mutex_unlock(&mutex);
		return (-2);
	}
	if (str == NULL) {
		pthread_mutex_unlock(&mutex);
		return (1);
	}
	if (strlen(str) >= len) {
		pthread_mutex_unlock(&mutex);
		return (-3);
	}
	strcpy(auditstr, str);
	return (0);
}

/*
 * Return the policy field from the audit control file.
 */
int
getacpol(char *auditstr, size_t len)
{
	char *str;

	pthread_mutex_lock(&mutex);
	setac_locked();
	if (getstrfromtype_locked(POLICY_CONTROL_ENTRY, &str) < 0) {
		pthread_mutex_unlock(&mutex);
		return (-2);
	}
	if (str == NULL) {
		pthread_mutex_unlock(&mutex);
		return (-1);
	}
	if (strlen(str) >= len) {
		pthread_mutex_unlock(&mutex);
		return (-3);
	}
	strcpy(auditstr, str);
	pthread_mutex_unlock(&mutex);
	return (0);
}
