/* drm_init.h -- Setup/Cleanup for DRM -*- linux-c -*-
 * Created: Mon Jan  4 08:58:31 1999 by faith@valinux.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 */

#include "drmP.h"

#if 0
int DRM(flags) = DRM_FLAG_DEBUG;
#else
int DRM(flags) = 0;
#endif

/* drm_parse_option parses a single option.  See description for
 * drm_parse_options for details.
 */
static void DRM(parse_option)(char *s)
{
	char *c, *r;

	DRM_DEBUG("\"%s\"\n", s);
	if (!s || !*s) return;
	for (c = s; *c && *c != ':'; c++); /* find : or \0 */
	if (*c) r = c + 1; else r = NULL;  /* remember remainder */
	*c = '\0';			   /* terminate */
	if (!strcmp(s, "noctx")) {
		DRM(flags) |= DRM_FLAG_NOCTX;
		DRM_INFO("Server-mediated context switching OFF\n");
		return;
	}
	if (!strcmp(s, "debug")) {
		DRM(flags) |= DRM_FLAG_DEBUG;
		DRM_INFO("Debug messages ON\n");
		return;
	}
	DRM_ERROR("\"%s\" is not a valid option\n", s);
	return;
}

/* drm_parse_options parse the insmod "drm_opts=" options, or the command-line
 * options passed to the kernel via LILO.  The grammar of the format is as
 * follows:
 *
 * drm		::= 'drm_opts=' option_list
 * option_list	::= option [ ';' option_list ]
 * option	::= 'device:' major
 *		|   'debug'
 *		|   'noctx'
 * major	::= INTEGER
 *
 * Note that 's' contains option_list without the 'drm_opts=' part.
 *
 * device=major,minor specifies the device number used for /dev/drm
 *	  if major == 0 then the misc device is used
 *	  if major == 0 and minor == 0 then dynamic misc allocation is used
 * debug=on specifies that debugging messages will be printk'd
 * debug=trace specifies that each function call will be logged via printk
 * debug=off turns off all debugging options
 *
 */

void DRM(parse_options)(char *s)
{
	char *h, *t, *n;

	DRM_DEBUG("\"%s\"\n", s ?: "");
	if (!s || !*s) return;

	for (h = t = n = s; h && *h; h = n) {
		for (; *t && *t != ';'; t++);	       /* find ; or \0 */
		if (*t) n = t + 1; else n = NULL;      /* remember next */
		*t = '\0';			       /* terminate */
		DRM(parse_option)(h);		       /* parse */
	}
}

/* drm_cpu_valid returns non-zero if the DRI will run on this CPU, and 0
 * otherwise.
 */
int DRM(cpu_valid)(void)
{
#if defined(__i386__)
	if (boot_cpu_data.x86 == 3) return 0; /* No cmpxchg on a 386 */
#endif
#if defined(__sparc__) && !defined(__sparc_v9__)
	return 0; /* No cmpxchg before v9 sparc. */
#endif
	return 1;
}
