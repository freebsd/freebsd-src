/*-
 * Copyright (c) 2000 Doug Rabson
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
 *	$FreeBSD$
 */

#include <efi.h>
#include <efilib.h>

#include "bootstrap.h"

static SIMPLE_TEXT_OUTPUT_INTERFACE	*conout;
static SIMPLE_INPUT_INTERFACE		*conin;

static void
efi_cons_probe(struct console *cp)
{
	conout = ST->ConOut;
	conin = ST->ConIn;
	cp->c_flags |= C_PRESENTIN | C_PRESENTOUT;
}

static int
efi_cons_init(int arg)
{
	return 0;
}

void
efi_cons_putchar(int c)
{
	CHAR16 buf[2];

	if (c == '\n')
		efi_cons_putchar('\r');

	buf[0] = c;
	buf[1] = 0;

	conout->OutputString(conout, buf);
}

int
efi_cons_getchar()
{
	EFI_INPUT_KEY key;
	UINTN junk;

	BS->WaitForEvent(1, &conin->WaitForKey, &junk);
	conin->ReadKeyStroke(conin, &key);
	return key.UnicodeChar;
}

int
efi_cons_poll()
{
	return BS->CheckEvent(conin->WaitForKey) == EFI_SUCCESS;
}

struct console efi_console = {
	"efi",
	"EFI console",
	0,
	efi_cons_probe,
	efi_cons_init,
	efi_cons_putchar,
	efi_cons_getchar,
	efi_cons_poll
};
