/*-
 * Copyright (c) 2012 Joseph Koshy
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
 * $Id: elftc_version.m4 2846 2012-12-31 04:20:43Z jkoshy $
 */

#include <sys/types.h>
#include <sys/utsname.h>

#include <errno.h>
#include <libelftc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tet_api.h"

include(`elfts.m4')

void
tcReturnValueIsNonNull(void)
{
	const char *version;

	TP_ANNOUNCE("elftc_version() returns a non-null pointer");

	version = elftc_version();

	tet_result(version != NULL ? TET_PASS : TET_FAIL);
}

#define	DELIMS		" \t"

/*
 * Check the form of the returned string.
 */
void
tcReturnValueFormat(void)
{
	int result;
	const char *version;
	struct utsname unamebuf;
	char *field, *versioncopy;

	TP_ANNOUNCE("The returned string from elftc_version() has the "
	    "correct form.");

	result = TET_UNRESOLVED;
	versioncopy = NULL;
	
	if ((version = elftc_version()) == NULL ||
 	    (versioncopy = strdup(version)) == NULL) {
		TP_UNRESOLVED("version retrieval failed: %s",
		    strerror(errno));
		goto done;
	}

	if (uname(&unamebuf) < 0) {
		TP_UNRESOLVED("uname failed: %s", strerror(errno));
		goto done;
	}

	/*
	 * Field 1 should be "elftoolchain".
	 */
	if ((field = strtok(versioncopy, DELIMS)) == NULL) {
		TP_FAIL("Missing field 1: \"%s\"", version);
		goto done;
	}
	if (strcmp(field, "elftoolchain")) {
		TP_FAIL("Field 1 \"%s\" != \"elftoolchain\": \"%s\"",
		    field, version);
		goto done;
	}

	/*
	 * Field 2 is the branch identifier.  We do not check
	 * the value of this field.
	 */
	if ((field = strtok(NULL, DELIMS)) == NULL) {
		TP_FAIL("Missing field 2: \"%s\"", version);
		goto done;
	}

	/*
	 * Field 3 is the system name.
	 */	
	if ((field = strtok(NULL, DELIMS)) == NULL) {
		TP_FAIL("Missing field 3: \"%s\"", version);
		goto done;
	}
	if (strcmp(field, unamebuf.sysname)) {
		TP_FAIL("System name mismatch: field \"%s\" != "
		    "uname \"%s\": %s", field, unamebuf.sysname,
		    version);
		goto done;
	}

	/*
	 * Field 4 is the current version identifier.
	 */
	if ((field = strtok(NULL, DELIMS)) == NULL) {
		TP_FAIL("Missing field 4: \"%s\"", version);
		goto done;
	}

	/*
	 * There should be no other fields.
	 */
	if ((field = strtok(NULL, DELIMS)) != NULL) {
		TP_FAIL("Extra fields: \%s\" in \"%s\"", field, version);
		goto done;
	}

	result = TET_PASS;

done:
	free(versioncopy);
	tet_result(result);
}
