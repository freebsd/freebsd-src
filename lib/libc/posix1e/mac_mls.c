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

#include <security/mac_mls/mac_mls.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/*
 * MLS labels take the following format:
 * [optional mlssingle][optional mlsrange]
 * mlssingle: {low,0-65535,high,equal}
 * mlsrange: ([mlssingle]-[mlssingle])
 */

/*
 * Extract mac_mls_element contents from a string.
 */
static int
mac_mls_element_from_string(char *string, struct mac_mls_element *element)
{
	unsigned long value;
	char *endp;
	int error;

	if (strcmp(string, "low") == 0) {
		element->mme_type = MAC_MLS_TYPE_LOW;
		element->mme_level = 0;
		error = 0;
	} else if (strcmp(string, "high") == 0) {
		element->mme_type = MAC_MLS_TYPE_HIGH;
		element->mme_level = 0;
		error = 0;
	} else if (strcmp(string, "equal") == 0) {
		element->mme_type = MAC_MLS_TYPE_EQUAL;
		element->mme_level = 0;
		error = 0;
	} else {
		value = strtoul(string, &endp, 10);
		if (*endp == '\0' && value == (u_short) value) {
			element->mme_type = MAC_MLS_TYPE_LEVEL;
			element->mme_level = value;
			error = 0;
		} else
			error = EINVAL;
	}

	return (error);
}

/*
 * Destructively convert a string into a mac_mls.
 */
int
mac_mls_label_from_string(char *string, struct mac *label)
{
	char *string_single, *string_rangelow, *string_rangehigh;
	int error;

	bzero(&label->m_mls, sizeof(label->m_mls));

	/*
	 * Is a '(' present?, if so check for last character of ')', and
	 * split into single and range strings after nulling the '(' and
	 * ')'.  Reject if appropriate.
	 */

	string_single = strsep(&string, "(");
	if (*string_single == '\0' && string == NULL) {
		/* No interesting elements to parse, flags already zero'd. */
		return (0);
	}
	if (string != NULL) {
		/* If a '(' was present, last character must be ')'. */
		if (*string == '\0')
			return (EINVAL);
		if (string[strlen(string)-1] != ')')
			return (EINVAL);
		string[strlen(string)-1] = '\0';
	}

	/*
	 * If range is present, split range into rangelow and rangehigh
	 * based on '-', if present, and nul it.  Process range elements.
	 * Reject if appropriate.
	 */
	if (string != NULL) {
		string_rangehigh = string;
		string_rangelow = strsep(&string_rangehigh, "-");
		if (*string_rangelow == '\0' || string_rangehigh == NULL)
			return (EINVAL);
		error = mac_mls_element_from_string(string_rangelow,
		    &label->m_mls.mm_rangelow);
		if (error)
			return (error);
		error = mac_mls_element_from_string(string_rangehigh,
		    &label->m_mls.mm_rangehigh);
		if (error)
			return (error);
		label->m_mls.mm_flags |= MAC_MLS_FLAG_RANGE;
	}

	/*
	 * If single is present, process single and reject if needed.
	 */
	if (*string_single != '\0') {
		error = mac_mls_element_from_string(string_single,
		    &label->m_mls.mm_single);
		if (error)
			return (error);
		label->m_mls.mm_flags |= MAC_MLS_FLAG_SINGLE;
	}

	return (0);
}

static char *
mac_mls_string_from_element(struct mac_mls_element *element)
{
	char *string;

	switch(element->mme_type) {
	case MAC_MLS_TYPE_LOW:
		return (strdup("low"));

	case MAC_MLS_TYPE_HIGH:
		return (strdup("high"));

	case MAC_MLS_TYPE_EQUAL:
		return (strdup("equal"));

	case MAC_MLS_TYPE_LEVEL:
		asprintf(&string, "%d", element->mme_level);
		return (string);

	default:
		return (strdup("invalid"));
	}
}

char *
mac_mls_string_from_label(struct mac *label)
{
	char *format_string = NULL;
	char *string = NULL, *string_single = NULL, *string_rangelow = NULL;
	char *string_rangehigh = NULL;

	if (label->m_mls.mm_flags & MAC_MLS_FLAG_SINGLE) {
		string_single = mac_mls_string_from_element(
		    &label->m_mls.mm_single);
	}
	if (label->m_mls.mm_flags & MAC_MLS_FLAG_RANGE) {
		string_rangelow = mac_mls_string_from_element(
		    &label->m_mls.mm_rangelow);
		string_rangehigh = mac_mls_string_from_element(
		    &label->m_mls.mm_rangehigh);
	}

	if (string_rangelow && string_single) {
		asprintf(&string, "%s(%s-%s)", string_single, string_rangelow,
		    string_rangehigh);
	} else if (string_rangelow) {
		asprintf(&string, "(%s-%s)", string_rangelow,
		    string_rangehigh);
	} else if (string_single) {
		asprintf(&string, "%s", string_single);
	} else
		string = strdup("");

	if (string_single)
		free(string_single);
	if (string_rangelow)
		free(string_rangelow);
	if (string_rangehigh)
		free(string_rangehigh);

	return (string);
}
