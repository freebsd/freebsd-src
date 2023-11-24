/*
 * Copyright (c) 2018-2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PT_VERSION_H
#define PT_VERSION_H

#include "intel-pt.h"

#include <stdio.h>
#include <inttypes.h>


static inline int pt_fprint_version(FILE *file, struct pt_version version)
{
	if (version.build) {
		if (version.ext && version.ext[0])
			return fprintf(file, "%" PRIu8 ".%" PRIu8 ".%" PRIu16
				       "-%" PRIu32 "-%s", version.major,
				       version.minor, version.patch,
				       version.build, version.ext);
		else
			return fprintf(file, "%" PRIu8 ".%" PRIu8 ".%" PRIu16
				       "-%" PRIu32 "", version.major,
				       version.minor, version.patch,
				       version.build);
	} else {
		if (version.ext && version.ext[0])
			return fprintf(file, "%" PRIu8 ".%" PRIu8 ".%" PRIu16
				       "-%s", version.major, version.minor,
				       version.patch, version.ext);
		else
			return fprintf(file, "%" PRIu8 ".%" PRIu8 ".%" PRIu16,
				       version.major, version.minor,
				       version.patch);
	}
}

static inline int pt_print_version(struct pt_version version)
{
	return pt_fprint_version(stdout, version);
}

static inline void pt_print_tool_version(const char *name)
{
	struct pt_version v = {
		/* .major = */ PT_VERSION_MAJOR,
		/* .minor = */ PT_VERSION_MINOR,
		/* .patch = */ PT_VERSION_PATCH,
		/* .build = */ PT_VERSION_BUILD,
		/* .ext = */ PT_VERSION_EXT
	};

	if (!name)
		name = "<unknown>";

	printf("%s-", name);
	pt_print_version(v);
	printf(" / libipt-");
	pt_print_version(pt_library_version());
	printf("\n");
}

#endif /* PT_VERSION_H */
