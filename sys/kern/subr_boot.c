/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * All Rights Reserved.
 * Copyright (c) 1998 Robert Nordier
 * All Rights Reserved.
 * Copyright (c) 2009, Oleksandr Tymoshenko <gonzo@FreeBSD.org>
 * All rights reserved.
 * Copyright (c) 2014 Roger Pau Monné <roger.pau@citrix.com>
 * All Rights Reserved.
 * Copyright (c) 2018 Kyle Evans <kevans@FreeBSD.org>
 * Copyright (c) 2018 Netflix, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 */

#include <sys/cdefs.h>
/* Note: This is compiled in both the kernel and boot loader contexts */

#include <sys/param.h>
#ifdef _KERNEL
#include <sys/systm.h>
#else
#include <stand.h>
#endif
#include <sys/reboot.h>
#include <sys/boot.h>
#include <sys/tslog.h>

#ifdef _KERNEL
#define SETENV(k, v)	kern_setenv(k, v)
#define GETENV(k)	kern_getenv(k)
#define FREE(v)		freeenv(v)
#else	/* Boot loader */
#define SETENV(k, v)	setenv(k, v, 1)
#define GETENV(k)	getenv(k)
#define	FREE(v)
#endif

static struct
{
	const char	*ev;
	int		mask;
} howto_names[] = {
	{ "boot_askname",	RB_ASKNAME},
	{ "boot_cdrom",		RB_CDROM},
	{ "boot_ddb",		RB_KDB},
	{ "boot_dfltroot",	RB_DFLTROOT},
	{ "boot_gdb",		RB_GDB},
	{ "boot_multicons",	RB_MULTIPLE},
	{ "boot_mute",		RB_MUTE},
	{ "boot_mutemsgs",	RB_MUTEMSGS},
	{ "boot_pause",		RB_PAUSE},
	{ "boot_serial",	RB_SERIAL},
	{ "boot_single",	RB_SINGLE},
	{ "boot_verbose",	RB_VERBOSE},
	{ NULL,	0}
};

/*
 * In the boot environment, we often parse a command line and have to throw away
 * its contents. As we do so, we set environment variables that correspond to
 * the flags we encounter. Later, to get a howto mask, we grovel through these
 * to reconstruct it. This also allows users in their loader.conf to set them
 * and have the kernel see them.
 */

/**
 * @brief convert the env vars in howto_names into a howto mask
 */
int
boot_env_to_howto(void)
{
	int i, howto;
	char *val;

	TSENTER();
	for (howto = 0, i = 0; howto_names[i].ev != NULL; i++) {
		val = GETENV(howto_names[i].ev);
		if (val != NULL && strcasecmp(val, "no") != 0)
			howto |= howto_names[i].mask;
		FREE(val);
	}
	TSEXIT();
	return (howto);
}

/**
 * @brief Set env vars from howto_names based on howto passed in
 */
void
boot_howto_to_env(int howto)
{
	int i;

	for (i = 0; howto_names[i].ev != NULL; i++)
		if (howto & howto_names[i].mask)
			SETENV(howto_names[i].ev, "YES");
}

/**
 * @brief Helper routine to parse a single arg and return its mask
 *
 * Parse all the - options to create a mask (or a serial speed in the
 * case of -S). If the arg doesn't start with '-' assume it's an env
 * variable and set that instead.
 */
int
boot_parse_arg(const char *v)
{
	char *n;
	int howto;

#if 0
/* Need to see if this is better or worse than the meat of the #else */
static const char howto_switches[] = "aCdrgDmMphsv";
static int howto_masks[] = {
	RB_ASKNAME, RB_CDROM, RB_KDB, RB_DFLTROOT, RB_GDB, RB_MULTIPLE,
	RB_MUTE, RB_MUTEMSGS, RB_PAUSE, RB_SERIAL, RB_SINGLE, RB_VERBOSE
};

	opts = strchr(kargs, '-');
	while (opts != NULL) {
		while (*(++opts) != '\0') {
			sw = strchr(howto_switches, *opts);
			if (sw == NULL)
				break;
			howto |= howto_masks[sw - howto_switches];
		}
		opts = strchr(opts, '-');
	}
#else
	howto = 0;
	if (*v == '-') {
		while (*v != '\0') {
			v++;
			switch (*v) {
			case 'a': howto |= RB_ASKNAME; break;
			case 'C': howto |= RB_CDROM; break;
			case 'd': howto |= RB_KDB; break;
			case 'D': howto |= RB_MULTIPLE; break;
			case 'm': howto |= RB_MUTE; break;
			case 'M': howto |= RB_MUTEMSGS; break;
			case 'g': howto |= RB_GDB; break;
			case 'h': howto |= RB_SERIAL; break;
			case 'p': howto |= RB_PAUSE; break;
			case 'P': howto |= RB_PROBE; break;
			case 'r': howto |= RB_DFLTROOT; break;
			case 's': howto |= RB_SINGLE; break;
			case 'S': SETENV("comconsole_speed", v + 1); v += strlen(v); break;
			case 'v': howto |= RB_VERBOSE; break;
			}
		}
	} else {
		char buf[128];
		char *vv = buf;

		strlcpy(buf, v, sizeof(buf));
		n = strsep(&vv, "=");
		if (vv == NULL)
			SETENV(n, "1");
		else
			SETENV(n, vv);
	}
#endif
	return (howto);
}

/**
 * @brief breakup the command line into args, and pass to boot_parse_arg
 */
int
boot_parse_cmdline_delim(char *cmdline, const char *delim)
{
	char *v;
	int howto;

	TSENTER();
	howto = 0;
	while ((v = strsep(&cmdline, delim)) != NULL) {
		if (*v == '\0')
			continue;
		howto |= boot_parse_arg(v);
	}
	TSEXIT();
	return (howto);
}

/**
 * @brief Simplified interface for common 'space or tab separated' args
 */
int
boot_parse_cmdline(char *cmdline)
{

	return (boot_parse_cmdline_delim(cmdline, " \t\n"));
}

/**
 * @brief Pass a vector of strings to boot_parse_arg
 */
int
boot_parse_args(int argc, char *argv[])
{
        int i, howto;

	howto = 0;
        for (i = 1; i < argc; i++)
                howto |= boot_parse_arg(argv[i]);
	return (howto);
}
