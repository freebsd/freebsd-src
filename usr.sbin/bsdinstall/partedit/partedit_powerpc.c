/*-
 * Copyright (c) 2011 Nathan Whitehorn
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

#include <sys/types.h>
#include <sys/sysctl.h>
#include <string.h>

#include "partedit.h"

static char platform[255] = "";

const char *
default_scheme(void) {
	size_t platlen = sizeof(platform);
	if (strlen(platform) == 0)
		sysctlbyname("hw.platform", platform, &platlen, NULL, -1);

	if (strcmp(platform, "powermac") == 0)
		return ("APM");
	if (strcmp(platform, "chrp") == 0)
		return ("MBR");

	/* Pick GPT (bootable on PS3) as a generic default */
	return ("GPT");
}

int
is_scheme_bootable(const char *part_type) {
	size_t platlen = sizeof(platform);
	if (strlen(platform) == 0)
		sysctlbyname("hw.platform", platform, &platlen, NULL, -1);

	if (strcmp(platform, "powermac") == 0 && strcmp(part_type, "APM") == 0)
		return (1);
	if (strcmp(platform, "ps3") == 0 && strcmp(part_type, "GPT") == 0)
		return (1);
	if (strcmp(platform, "chrp") == 0 && strcmp(part_type, "MBR") == 0)
		return (1);

	return (0);
}

size_t
bootpart_size(const char *part_type) {
	if (strcmp(part_type, "APM") == 0 || strcmp(part_type, "MBR") == 0)
		return (800*1024);
	return (0);
}

const char *
bootcode_path(const char *part_type) {
	return (NULL);
}
	
const char *
partcode_path(const char *part_type) {
	if (strcmp(part_type, "APM") == 0)
		return ("/boot/boot1.hfs");
	if (strcmp(part_type, "MBR") == 0)
		return ("/boot/boot1.elf");
	return (NULL);
}

