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
#include <sys/mac.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "mac_internal.h"

/*
 * POSIX.1e does not define a text format for MAC label string conversions.
 * We use the following format:
 *
 * label: policyname/policyvalue{,...}
 *
 * Each policy is responsible for parsing policyvalue on its own, although
 * policies must not use '/' or ',' in their text representation.  Currently
 * supported policies are "biba, "mls", "te".
 */

#define	STRING_LISTSEP		","
#define	STRING_ELEMENTSEP	"/"

#define	STRING_BIBA		"biba"
#define	STRING_MLS		"mls"
#define	STRING_TE		"te"

char *
mac_to_text(struct mac *mac_p, size_t *len_p)
{
	char *biba = NULL, *mls = NULL, *string = NULL, *te = NULL;
	int len = -1, before;

	biba = mac_biba_string_from_label(mac_p);
	if (biba == NULL)
		goto out;

	mls = mac_mls_string_from_label(mac_p);
	if (mls == NULL)
		goto out;

	te = mac_te_string_from_label(mac_p);
	if (te == NULL)
		goto out;

	len = 0;
	if (strlen(biba) != 0)
		len += strlen(STRING_LISTSEP) + strlen(STRING_BIBA) +
		    strlen(STRING_ELEMENTSEP) + strlen(biba);
	if (strlen(mls) != 0)
		len += strlen(STRING_LISTSEP) + strlen(STRING_MLS) +
		    strlen(STRING_ELEMENTSEP) + strlen(mls);
	if (strlen(te) != 0)
		len += strlen(STRING_LISTSEP) + strlen(STRING_TE) +
		    strlen(STRING_ELEMENTSEP) + strlen(te);

	if (len == 0) {
		string = strdup("");
		goto out;
	}

	string = (char *) malloc(len+1);
	if (string == NULL)
		return (NULL);

	len = 0;
	before = 0;

	if (strlen(biba) != 0) {
		if (before)
			len += sprintf(string + len, "%s", STRING_LISTSEP);
		len += sprintf(string + len, "%s%s%s", STRING_BIBA,
		    STRING_ELEMENTSEP, biba);
		before = 1;
	}
	if (strlen(mls) != 0) {
		if (before)
			len += sprintf(string + len, "%s", STRING_LISTSEP);
		len += sprintf(string + len, "%s%s%s", STRING_MLS,
		    STRING_ELEMENTSEP, mls);
		before = 1;
	}
	if (strlen(te) != 0) {
		if (before)
			len += sprintf(string + len, "%s", STRING_LISTSEP);
		len += sprintf(string + len, "%s%s%s", STRING_TE,
		    STRING_ELEMENTSEP, te);
		before = 1;
	}

out:
	if (biba != NULL)
		free(biba);
	if (mls != NULL)
		free(mls);
	if (te != NULL)
		free(te);

	if (len != -1 && len_p != NULL)
		*len_p = len;

	return (string);
}

struct mac *
mac_from_text(const char *text_p)
{
	struct mac *label;
	char *local_string, *next_token, *token, *tmp;
	char *policy_name, *policy_value;
	int biba_seen = 0, mls_seen = 0, te_seen = 0;
	int error;

	/*
	 * Parse into three assignments, determine which assignments
	 * they are and recurse appropriately, and reject if there are
	 * not the right assignments (or duplicates).
	 */

	label = (struct mac *) malloc(sizeof(*label));
	if (label == NULL) {
		errno = ENOMEM;
		goto exit1;
	}
	label->m_macflags = 0;
	label->m_macflags |= MAC_FLAG_INITIALIZED;

	local_string = strdup(text_p);
	if (local_string == NULL) {
		errno = ENOMEM;
		goto exit2;
	}

	next_token = local_string;
	while ((token = strsep(&next_token, STRING_LISTSEP)) != NULL) {

		policy_value = token;
		policy_name = strsep(&policy_value, STRING_ELEMENTSEP);

		if (strcmp(policy_name, STRING_BIBA) == 0) {
			error = mac_biba_label_from_string(policy_value,
			    label);
			if (error) {
				errno = error;
				goto exit2;
			}
			biba_seen++;
		} else if (strcmp(policy_name, STRING_MLS) == 0) {
			error = mac_mls_label_from_string(policy_value,
			    label);
			if (error) {
				errno = error;
				goto exit2;
			}
			mls_seen++;
		} else if (strcmp(policy_name, STRING_TE) == 0) {
			error = mac_te_label_from_string(policy_value, label);
			if (error) {
				errno = error;
				goto exit2;
			}
			te_seen++;
		} else {
			errno = EINVAL;
			goto exit2;
		}
	}

	if (biba_seen == 0) {
		error = mac_biba_label_from_string("", label);
		if (error) {
			errno = error;
			goto exit2;
		}
	}
	if (mls_seen == 0) {
		error = mac_mls_label_from_string("", label);
		if (error) {
			errno = error;
			goto exit2;
		}
	}
	if (te_seen == 0) {
		error = mac_te_label_from_string("", label);
		if (error) {
			errno = error;
			goto exit2;
		}
	}

	if (biba_seen > 1 || mls_seen > 1 || te_seen > 1) {
		errno = EINVAL;
		goto exit2;
	}

	/* Success. */
	goto exit1;

exit2:
	free(label);
	label = NULL;
exit1:
	free(local_string);
	return (label);
}
