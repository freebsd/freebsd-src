/*
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by NAI Labs,
 * the Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/syslimits.h>

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mac.h>

static int	internal_initialized;

/* Default sets of labels for various query operations. */
static char	*default_file_labels;
static char	*default_ifnet_labels;
static char	*default_process_labels;

static void
mac_destroy_labels(void)
{

	if (default_file_labels != NULL) {
		free(default_file_labels);
		default_file_labels = NULL;
	}

	if (default_ifnet_labels != NULL) {
		free(default_ifnet_labels);
		default_ifnet_labels = NULL;
	}

	if (default_process_labels != NULL) {
		free(default_process_labels);
		default_process_labels = NULL;
	}
}

static void
mac_destroy_internal(void)
{

	mac_destroy_labels();

	internal_initialized = 0;
}

static int
mac_init_internal(void)
{
	FILE *file;
	char line[LINE_MAX];
	int error;

	error = 0;

	file = fopen(MAC_CONFFILE, "r");
	if (file == NULL)
		return (0);

	while (fgets(line, LINE_MAX, file)) {
		char *argv[ARG_MAX];
		char *arg, *parse, *statement, *policyname, *modulename;
		int argc;

		if (line[strlen(line)-1] == '\n')
			line[strlen(line)-1] = '\0';
		else {
			fclose(file);
			error = EINVAL;
			goto just_return;
		}

		parse = line;
		statement = "";
		while (parse && statement[0] == '\0')
			statement = strsep(&parse, " \t");

		/* Blank lines ok. */
		if (strlen(statement) == 0)
			continue;

		/* Lines that consist only of comments ok. */
		if (statement[0] == '#')
			continue;

		if (strcmp(statement, "default_file_labels") == 0) {
			if (default_file_labels != NULL) {
				free(default_file_labels);
				default_file_labels = NULL;
			}

			arg = strsep(&parse, "# \t");
			if (arg != NULL && arg[0] != '\0') {
				default_file_labels = strdup(arg);
				if (default_file_labels == NULL) {
					error = ENOMEM;
					fclose(file);
					goto just_return;
				}
			}
		} else if (strcmp(statement, "default_ifnet_labels") == 0) {
			if (default_ifnet_labels != NULL) {
				free(default_ifnet_labels);
				default_ifnet_labels = NULL;
			}

			arg = strsep(&parse, "# \t");
			if (arg != NULL && arg[0] != '\0') {
				default_ifnet_labels = strdup(arg);
				if (default_ifnet_labels == NULL) {
					error = ENOMEM;
					fclose(file);
					goto just_return;
				}
			}
		} else if (strcmp(statement, "default_process_labels") == 0) {
			if (default_process_labels != NULL) {
				free(default_process_labels);
				default_process_labels = NULL;
			}

			arg = strsep(&parse, "# \t");
			if (arg != NULL && arg[0] != '\0') {
				default_process_labels = strdup(arg);
				if (default_process_labels == NULL) {
					error = ENOMEM;
					fclose(file);
					goto just_return;
				}
			}
		} else {
			fclose(file);
			error = EINVAL;
			goto just_return;
		}
	}

	fclose(file);

	internal_initialized = 1;

just_return:
	if (error != 0)
		mac_destroy_internal();
	return (error);
}

static int
mac_maybe_init_internal(void)
{

	if (!internal_initialized)
		return (mac_init_internal());
	else
		return (0);
}

int
mac_reload(void)
{

	if (internal_initialized)
		mac_destroy_internal();
	return (mac_init_internal());
}

int
mac_free(struct mac *mac)
{
	int error;

	if (mac->m_string != NULL)
		free(mac->m_string);
	free(mac);

	return (0);
}

int
mac_from_text(struct mac **mac, const char *text)
{
	struct mac *temp;
	char *dup, *element, *search;
	int count, error;

	*mac = (struct mac *) malloc(sizeof(**mac));
	if (*mac == NULL)
		return (ENOMEM);

	(*mac)->m_string = strdup(text);
	if ((*mac)->m_string == NULL) {
		free(*mac);
		*mac = NULL;
		return (ENOMEM);
	}

	(*mac)->m_buflen = strlen((*mac)->m_string)+1;

	return (0);
}

int
mac_to_text(struct mac *mac, char **text)
{

	*text = strdup(mac->m_string);
	if (*text == NULL)
		return (ENOMEM);
	return (0);
}

int
mac_prepare(struct mac **mac, char *elements)
{
	struct mac *temp;

	if (strlen(elements) >= MAC_MAX_LABEL_BUF_LEN)
		return (EINVAL);

	*mac = (struct mac *) malloc(sizeof(**mac));
	if (*mac == NULL)
		return (ENOMEM);

	(*mac)->m_string = malloc(MAC_MAX_LABEL_BUF_LEN);
	if ((*mac)->m_string == NULL) {
		free(*mac);
		*mac = NULL;
		return (ENOMEM);
	}

	strcpy((*mac)->m_string, elements);
	(*mac)->m_buflen = MAC_MAX_LABEL_BUF_LEN;

	return (0);
}

int
mac_prepare_file_label(struct mac **mac)
{
	int error;

	error = mac_maybe_init_internal();
	if (error != 0)
		return (error);

	if (default_file_labels == NULL)
		return (mac_prepare(mac, ""));

	return (mac_prepare(mac, default_file_labels));
}

int
mac_prepare_ifnet_label(struct mac **mac)
{
	int error;

	error = mac_maybe_init_internal();
	if (error != 0)
		return (error);

	if (default_ifnet_labels == NULL)
		return (mac_prepare(mac, ""));

	return (mac_prepare(mac, default_ifnet_labels));
}
int
mac_prepare_process_label(struct mac **mac)
{
	int error;

	error = mac_maybe_init_internal();
	if (error != 0)
		return (error);

	if (default_process_labels == NULL)
		return (mac_prepare(mac, ""));

	return (mac_prepare(mac, default_process_labels));
}

/*
 * Simply test whether the TrustedBSD/MAC MIB tree is present; if so,
 * return 1 to indicate that the system has MAC enabled overall or for
 * a given policy.
 */
int
mac_is_present(const char *policyname)
{
	int mib[5];
	size_t siz;
	char *mibname;
	int error;

	if (policyname != NULL) {
		if (policyname[strcspn(policyname, ".=")] != '\0') {
			errno = EINVAL;
			return (-1);
		}
		mibname = malloc(sizeof("security.mac.") - 1 +
		    strlen(policyname) + sizeof(".enabled"));
		if (mibname == NULL)
			return (-1);
		strcpy(mibname, "security.mac.");
		strcat(mibname, policyname);
		strcat(mibname, ".enabled");
		siz = 5;
		error = sysctlnametomib(mibname, mib, &siz);
		free(mibname);
	} else {
		siz = 3;
		error = sysctlnametomib("security.mac", mib, &siz);
	}
	if (error == -1) {
		switch (errno) {
		case ENOTDIR:
		case ENOENT:
			return (0);
		default:
			return (error);
		}
	}
	return (1);
}
