/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Aleksandr Rybalko under sponsorship from the
 * FreeBSD Foundation.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/vt/vt.h>

#ifndef SC_NO_CUTPASTE
struct mouse_cursor vt_default_mouse_pointer = {
	.map = {
		0x00, /* "__      " */
		0x40, /* "_*_     " */
		0x60, /* "_**_    " */
		0x70, /* "_***_   " */
		0x78, /* "_****_  " */
		0x7c, /* "_*****_ " */
		0x7e, /* "_******_" */
		0x68, /* "_**_****" */
		0x4c, /* "_*__**__" */
		0x0c, /* " _ _**_ " */
		0x06, /* "    _**_" */
		0x06, /* "    _**_" */
		0x00, /* "    ____" */
	},
	.mask = {
		0xc0, /* "__      " */
		0xe0, /* "___     " */
		0xf0, /* "____    " */
		0xf8, /* "_____   " */
		0xfc, /* "______  " */
		0xfe, /* "_______ " */
		0xff, /* "________" */
		0xff, /* "________" */
		0xff, /* "________" */
		0x1e, /* "   ____ " */
		0x0f, /* "    ____" */
		0x0f, /* "    ____" */
		0x0f, /* "    ____" */
	},
	.w = 8,
	.h = 13,
};
#endif
